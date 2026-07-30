# RK3568 GMAC → 88E1512 PHY 初始化失败：从内核报错到根因的完整逻辑链

## 1. 现象：内核报错日志

```
rk_gmac-dwmac fe010000.ethernet end1: validation of  with support 00000000,00000000,00006000 \
    and advertisement 00000000,00000000,00000000 failed: -EINVAL
```

### 1.1 错误来源

报错代码位于 `drivers/net/phy/phylink.c:1650-1656` 的 `phylink_bringup_phy()` 函数：

```c
static int phylink_bringup_phy(struct phylink *pl, struct phy_device *phy,
                               phy_interface_t interface)
{
    ...
    linkmode_copy(supported, phy->supported);              // ← 复制 PHY 的能力位图
    linkmode_copy(config.advertising, phy->advertising);    // ← 复制 PHY 的广告位图

    ret = phylink_validate(pl, supported, &config);         // ← 用 MAC 能力验证
    if (ret) {
        phylink_warn(pl, "validation of %s with support %*pb "
                     "and advertisement %*pb failed: %pe\n",
                     ..., phy->supported, config.advertising, ERR_PTR(ret));
        return ret;                                         // ← 这里打印你的报错
    }
    ...
}
```

### 1.2 位图解码

内核用 `%*pb` 格式打印位图，每个 32-bit 组用 8 个十六进制字符表示（`lib/vsprintf.c:1197` 的 `bitmap_string()`）。

该内核的 `__ETHTOOL_LINK_MODE_MASK_NBITS = 93`（`include/uapi/linux/ethtool.h:1741`），因此打印 3 个 32-bit 组（高位优先）：

| 组 | 位范围 | 十六进制 | 含义 |
|---|--------|---------|------|
| 第3组 | bits 64–92 | `00000000` | 空 |
| 第2组 | bits 32–63 | `00000000` | 空 |
| 第1组 | bits 0–31  | `00006000` | **仅 bit13 和 bit14 被设置** |

对照 `include/uapi/linux/ethtool.h` 的定义：

```c
ETHTOOL_LINK_MODE_Pause_BIT      = 13,   // 0x00002000
ETHTOOL_LINK_MODE_Asym_Pause_BIT = 14,   // 0x00004000
                                    // 0x00006000 ← 两者之和
```

**所以 `supported` 只有两个 Pause 相关 bit，完全没有 10/100/1000M 等实质链路能力。**

---

## 2. 完整调用链概览

```
┌─────────────────────────────────────────────────────────────────────┐
│                       内核 Probe 阶段                               │
├─────────────────────────────────────────────────────────────────────┤
│ stmmac_dvr_probe()                                                  │
│   └─ stmmac_mdio_register()                                        │
│        └─ of_mdiobus_register() → __of_mdiobus_register()          │
│             ├─ mdio->phy_mask = ~0   (关闭自动扫描)                 │
│             ├─ __mdiobus_register()  (注册总线，不扫描PHY)           │
│             └─ for_each_available_child_of_node()                   │
│                  └─ of_mdiobus_register_phy(mdio, child, addr)     │
│                       └─ fwnode_mdiobus_register_phy()             │
│                            ├─ 🔴 get_phy_device()                  │
│                            │    └─ get_phy_c22_id()               │
│                            │         ├─ mdiobus_read(PHYSID1=0x02) │
│                            │         └─ mdiobus_read(PHYSID2=0x03) │
│                            └─ phy_device_register()                │
│                                 └─ device_add() → 驱动匹配         │
│                                      └─ phy_probe()               │
│                                           └─ 填充 phy->supported   │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                        ifconfig up 阶段                             │
├─────────────────────────────────────────────────────────────────────┤
│ stmmac_open() → __stmmac_open()                                    │
│   └─ stmmac_init_phy()                                             │
│        └─ phylink_fwnode_phy_connect()                             │
│             ├─ fwnode_phy_find_device()   (找到已注册的 phy_device) │
│             ├─ phy_attach_direct()                                  │
│             │    └─ phy_init_hw()                                  │
│             │         ├─ soft_reset     (写 BMCR_RESET + 轮询)     │
│             │         └─ config_init    (m88e1510 页切换+寄存器写)  │
│             └─ ❌ phylink_bringup_phy() → phylink_validate() 失败   │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 3. 关键代码路径详解

### 3.1 设备树解析路径：`__of_mdiobus_register()`

你的 DTS (`rk3568-wap2001-2.dts:690-695`)：

```dts
&mdio1 {
    wap_rgmii_phy1: phy@1 {
        compatible = "ethernet-phy-ieee802.3-c22";
        reg = <0x1>;
    };
};
```

由于 DT 中显式定义了 PHY 子节点，走的是 `of_mdiobus_register_phy()` 路径而非 `mdiobus_scan()` 遍历扫描。

`drivers/net/mdio/of_mdio.c:150-240`:

```c
int __of_mdiobus_register(struct mii_bus *mdio, struct device_node *np, ...)
{
    mdio->phy_mask = ~0;            // 关闭所有地址的自动扫描
    __mdiobus_register(mdio, ...);  // 注册总线（不会扫描）

    for_each_available_child_of_node(np, child) {
        addr = of_mdio_parse_addr(&mdio->dev, child);
        if (of_mdiobus_child_is_phy(child))
            of_mdiobus_register_phy(mdio, child, addr);  // ← 走这里
    }
}
```

### 3.2 PHY_ID 获取的关键决策：`fwnode_mdiobus_register_phy()`

`drivers/net/mdio/fwnode_mdio.c:112-186`:

```c
int fwnode_mdiobus_register_phy(struct mii_bus *bus,
                                struct fwnode_handle *child, u32 addr)
{
    bool is_c45 = false;
    u32 phy_id;
    struct phy_device *phy;

    // 你的 compatible = "ethernet-phy-ieee802.3-c22" → is_c45 = false
    rc = fwnode_property_match_string(child, "compatible",
                                      "ethernet-phy-ieee802.3-c45");
    if (rc >= 0)
        is_c45 = true;

    // 🔴 关键判断：
    // fwnode_get_phy_id() 从 compatible="ethernet-phy-idXXXX.XXXX" 提取 PHY_ID
    // 对 "ethernet-phy-ieee802.3-c22" 返回 -EINVAL（失败 = 非零）
    // 所以条件为 true → 走 get_phy_device() 硬件读取！
    if (is_c45 || fwnode_get_phy_id(child, &phy_id))
        phy = get_phy_device(bus, addr, is_c45);   // 🔴 去硬件读 PHY_ID
    else
        phy = phy_device_create(bus, addr, phy_id, 0, NULL);  // 直接创建

    // 注册 PHY 设备 → device_add() → 触发驱动匹配 → phy_probe()
    fwnode_mdiobus_phy_device_register(bus, phy, child, addr);
}
```

> **结论**：你的 DTS 使用 `compatible = "ethernet-phy-ieee802.3-c22"`，PHY_ID **必须**通过 MDIO 总线从硬件寄存器读取。无法从 DT 获取。

### 3.3 PHY_ID 硬件读取：`get_phy_c22_id()`

`drivers/net/phy/phy_device.c:901-927`:

```c
static int get_phy_c22_id(struct mii_bus *bus, int addr, u32 *phy_id)
{
    int phy_reg;

    phy_reg = mdiobus_read(bus, addr, MII_PHYSID1);   // 读寄存器 0x02
    if (phy_reg < 0)
        return (phy_reg == -EIO || phy_reg == -ENODEV) ? -ENODEV : -EIO;

    *phy_id = phy_reg << 16;

    phy_reg = mdiobus_read(bus, addr, MII_PHYSID2);   // 读寄存器 0x03
    if (phy_reg < 0)
        return (phy_reg == -EIO || phy_reg == -ENODEV) ? -ENODEV : -EIO;

    *phy_id |= phy_reg;

    // 如果大部分位都是 1，判定为无设备
    if ((*phy_id & 0x1fffffff) == 0x1fffffff)
        return -ENODEV;

    return 0;
}
```

正常 88E1512 应读到：`PHYSID1 = 0x0141`, `PHYSID2 = 0x0dd1` → `phy_id = 0x01410dd1`

### 3.4 MDIO 底层读写：`stmmac_mdio_read()`

`drivers/net/ethernet/stmicro/stmmac/stmmac_mdio.c:218-273`:

```c
static int stmmac_mdio_read(struct mii_bus *bus, int phyaddr, int phyreg)
{
    ...
    value |= (phyaddr << mii.addr_shift) & mii.addr_mask;
    value |= (phyreg << mii.reg_shift) & mii.reg_mask;
    value |= (clk_csr << mii.clk_csr_shift) & mii.clk_csr_mask;
    if (priv->plat->has_gmac4) {
        value |= MII_GMAC4_READ;
    }

    // 写 MDIO 地址和数据寄存器到 GMAC 硬件
    writel(data, priv->ioaddr + mii_data);
    writel(value, priv->ioaddr + mii_address);

    // 等待 GMAC 完成 MDIO 协议交互，读取返回数据
    data = (int)readl(priv->ioaddr + mii_data) & MII_DATA_MASK;
    return data;
}
```

> **关键**：MDIO 协议是 GMAC 硬件自动完成的。即使 PHY 芯片无电，GMAC 端的寄存器读写不会报错——只是读回的数据是总线浮空电平（取决于上拉/下拉电阻）。

### 3.5 PHY 驱动匹配与 features 填充：`phy_probe()`

`drivers/net/phy/phy_device.c:3084-3176`:

```c
static int phy_probe(struct device *dev)
{
    struct phy_device *phydev = to_phy_device(dev);
    struct phy_driver *phydrv = to_phy_driver(drv);

    phydev->drv = phydrv;

    // 调用驱动的 probe
    if (phydev->drv->probe)
        phydev->drv->probe(phydev);      // marvell: m88e1510_probe()

    // 🔴 填充 supported link modes —— 三条路径：
    if (phydrv->features)
        linkmode_copy(phydev->supported, phydrv->features);  // 路径 A
    else if (phydrv->get_features)
        err = phydrv->get_features(phydev);                   // 路径 B
    else
        err = genphy_read_abilities(phydev);                  // 路径 C

    if (err)
        goto out;   // ← 如果失败，跳过所有后续操作

    of_set_phy_supported(phydev);       // 根据 DT max-speed 裁剪
    phy_advertise_supported(phydev);    // advertising ← supported

    // 添加 Pause 支持位
    linkmode_set_bit(ETHTOOL_LINK_MODE_Pause_BIT, phydev->supported);
    linkmode_set_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT, phydev->supported);

    phydev->state = PHY_READY;
    ...
}
```

**路径 A（Marvell 驱动匹配成功）**：

88E1512 的正常 PHY_ID 是 `0x01410dd1`，Marvell 驱动的匹配条件是：

```c
// marvell.c:3163-3184
{
    .phy_id = MARVELL_PHY_ID_88E1510,     // 0x01410dd0
    .phy_id_mask = MARVELL_PHY_ID_MASK,   // 0xfffffff0 (低4位掩码)
    .features = PHY_GBIT_FIBRE_FEATURES,  // 10/100/1000M + 光纤
    .probe = m88e1510_probe,
    .config_init = m88e1510_config_init,
    ...
}
```

`PHY_GBIT_FIBRE_FEATURES` 在 `features_init()` 中编译期静态构建，包含 Autoneg、TP、MII、FIBRE、10/100/1000M 等。**匹配成功时 `supported` 一定非空。**

**路径 B（genphy 驱动回退）**：

如果 `device_add()` 时无驱动匹配，`phydev->mdio.dev.driver` 为 NULL。后续 `phy_attach_direct()` 中会绑定 genphy：

```c
// phy_device.c:1480-1501
if (!d->driver) {
    d->driver = &genphy_driver.mdiodrv.driver;
    using_genphy = true;
}
if (using_genphy) {
    err = d->driver->probe(d);   // → phy_probe() with genphy
}
```

genphy 驱动：

```c
// phy_device.c:3282-3290
static struct phy_driver genphy_driver = {
    .get_features = genphy_read_abilities,   // 走路径 B → 读硬件寄存器
    .suspend = genphy_suspend,
    .resume = genphy_resume,
};
```

`genphy_read_abilities()` 从硬件读 BMCR 寄存器：

```c
int genphy_read_abilities(struct phy_device *phydev)
{
    // 先设置基础端口位 (Autoneg=6, TP=7, MII=9)
    linkmode_set_bit_array(phy_basic_ports_array, 3, phydev->supported);

    val = phy_read(phydev, MII_BMSR);    // 📖 读寄存器 0x01
    if (val < 0) return val;             // ← 供电异常时这里失败

    // 根据 BMSR 值设置 10/100M 能力
    linkmode_mod_bit(ETHTOOL_LINK_MODE_Autoneg_BIT, ..., val & BMSR_ANEGCAPABLE);
    ...

    // 如果设置了扩展状态位，再读 ESTATUS (0x0F) 获取 1000M 能力
    if (val & BMSR_ESTATEN) {
        val = phy_read(phydev, MII_ESTATUS);
        ...
    }
    return 0;
}
```

> **当 PHY 供电异常时**：`phy_read(MII_BMSR)` 返回错误 → `genphy_read_abilities()` 返回错误 → `phy_probe()` goto out → **pause bits 不会被添加** → `supported` 只含 bit6,7,9 → `advertising` 保持全 0。

---

## 4. 从报错到根因的逻辑推理

### 4.1 推理步骤

```
┌──────────────────────────────────────────────────────────────────┐
│ Step 1: supported = 0x6000 (只有 bit13=Pause, bit14=Asym_Pause) │
│         ↓                                                        │
│         正常的 Marvell 驱动会设置 full features                  │
│         genphy 也会设置 phy_basic_ports_array (bit6,7,9)         │
│         但这个 supported 连基本端口位都没有                       │
│         ↓                                                        │
│         说明：phy_probe() 没有正常完成                            │
└──────────────────────────────────────────────────────────────────┘
                              ↓
┌──────────────────────────────────────────────────────────────────┐
│ Step 2: phy_probe() 为什么会失败/异常？                          │
│         ↓                                                        │
│         可能原因：                                                │
│         (a) PHY_ID 读错了 → 没匹配到任何驱动 → genphy 回退        │
│              → genphy_read_abilities() 读 BMSR 失败               │
│         (b) PHY_ID 读对了 → Marvell 驱动匹配 → 但 probe 失败      │
│              → phy_device_register() 失败 → 设备未创建            │
│         ↓                                                        │
│         既然走到了 phylink_bringup_phy（说明设备存在且 attach     │
│         成功），更可能是 (a)：PHY_ID 错误 → genphy 回退            │
│         → genphy_read_abilities 部分执行                          │
└──────────────────────────────────────────────────────────────────┘
                              ↓
┌──────────────────────────────────────────────────────────────────┐
│ Step 3: 为什么 PHY_ID 会读错？                                    │
│         ↓                                                        │
│         get_phy_c22_id() 通过 stmmac_mdio_read() 读 MDIO 总线    │
│         └─ GMAC 发起 MDIO 帧，PHY 应返回 PHYSID1/PHYSID2         │
│         ↓                                                        │
│         如果 PHY 没有供电：                                        │
│         - MDIO 帧正常发出去（GMAC 端没问题）                       │
│         - PHY 不会驱动 MDIO 数据线                                 │
│         - 读回的是总线浮空电平（上拉 → 0xFFFF, 下拉 → 0x0000）     │
│         - 可能侥幸通过 "非全1" 校验，得到错误的 PHY_ID              │
└──────────────────────────────────────────────────────────────────┘
                              ↓
┌──────────────────────────────────────────────────────────────────┐
│ Step 4: 根因确认                                                  │
│         ↓                                                        │
│         88E1512 PHY 芯片供电异常 → MDIO 总线无法通信               │
│         → PHY_ID 读取错误 → 驱动匹配失败                          │
│         → supported 特征位图异常 → phylink_validate 失败           │
└──────────────────────────────────────────────────────────────────┘
```

### 4.2 两条可能的失败路径

#### 路径 A：PHY_ID 错误 → genphy 回退 → genphy_read_abilities 读 BMSR 失败

```
无电 PHY
  → get_phy_c22_id() 返回错误的 PHY_ID (非全1, 通过校验)
  → phy_device_create() 创建设备，wrong_id
  → device_add() → 无驱动匹配 → phydev->mdio.dev.driver = NULL
  → phy_probe() 未执行
  → supported = 0, advertising = 0

ifconfig up:
  → phy_attach_direct() → d->driver == NULL → 绑定 genphy
  → phy_probe() with genphy:
       genphy_read_abilities():
         set phy_basic_ports_array (bit6,7,9)
         phy_read(BMSR) → -EIO (无电!)
         return -EIO
       err = -EIO → goto out
       (pause bits 未设置, advertise 未执行)
  → phy_probe() returns -EIO
  → phy_attach_direct() detects error → attach 失败
  → ❌ 不会到达 phylink_bringup_phy
```

> 这条路径的最终结果是 **stmmac_open 直接失败**，不会打印 phylink_validate 错误。

#### 路径 B：PHY 供电间歇性异常（最可能）

```
Probe 时刻：供电正常
  → get_phy_c22_id() → PHY_ID = 0x01410dd1 ✓
  → Marvell 驱动匹配 → phy_probe() 成功
  → supported = PHY_GBIT_FIBRE_FEATURES (完整)
  → advertising = supported (完整)

ifconfig up 时刻：供电异常
  → phy_init_hw():
       soft_reset: 写 BMCR_RESET → 轮询读 BMCR → 可能超时或读到0
       config_init (m88e1510_config_init):
         marvell_set_page(0x00FF) → 写 reg22 → 无电 → no-op
         phy_write(reg17/16) ×4 → 无电 → no-op
         ...
         这些写操作在无电 PHY 上不产生效果，但也不会报错
         如果都不报错 → phy_init_hw 成功返回
  → phy_attach_direct 成功
  → phylink_bringup_phy():
       phy->supported 仍是 probe 时的值 (完整)
       phylink_validate 应该能通过...

  但这与 observed supported = 0x6000 矛盾！
```

> 所以路径 B 也无法解释 `supported` 只有 Pause bits 的现象。

#### 路径 C：PHY 完全没有被创建 → 使用了错误的 PHY 设备（不太可能）

---

## 5. 最终推断与验证方法

### 5.1 最可能的因果链

综合所有代码分析，最可能的因果链如下：

```
PHY 芯片供电异常（硬件问题）
  ↓
MDIO 物理层通信失败（PHY 不驱动数据线）
  ↓
get_phy_c22_id() 从浮空总线读到无效数据
  ├─ 可能1: 读到全 1 → 返回 -ENODEV → PHY 设备未创建 → stmmac_open 失败
  └─ 可能2: 读到随机数据（侥幸通过了"非全1"校验）→ 创建了带错误 PHY_ID 的设备
        ↓
        device_add() → 无驱动匹配 → phydev->drv = NULL
        ↓
        supported = 全 0, advertising = 全 0
        ↓
        ifconfig up: phy_attach_direct() 绑定 genphy
        ↓
        phy_probe() with genphy → genphy_read_abilities()
        ↓
        如果 phy_read(BMSR) 成功返回（浮空电平恰好给了"合法"值如 0x0000）
          → BMSR=0 → linkmode_mod_bit 清除 Autoneg
          → 保留 TP(bit7), MII(bit9)
          → phy_probe 成功！
          → supported = bits 7,9,13,14
          → BUT the error shows ONLY bits 13,14...
        
        如果 phy_read(BMSR) 失败 → phy_probe 失败 → attach 失败
    
    无论如何，supported 内容与正常 Marvell 驱动相差甚远
        ↓
        phylink_validate() 内 MAC validate 回调用 supported AND mask
        → supported 中无 10/100/1000M 能力 → mask 后的结果为空
        → 返回 -EINVAL
        ↓
        打印报错: "validation ... with support 00006000 ... failed: -EINVAL"
```

### 5.2 验证方法

要确认是 PHY_ID 读取问题，在以下关键位置添加日志：

```c
// 1. phy_device.c:906 — 确认 PHYSID1 原始值
phy_reg = mdiobus_read(bus, addr, MII_PHYSID1);
dev_info(&bus->dev, "PHY addr %d: PHYSID1 = 0x%04x\n", addr, phy_reg);

// 2. phy_device.c:915 — 确认 PHYSID2 原始值
phy_reg = mdiobus_read(bus, addr, MII_PHYSID2);
dev_info(&bus->dev, "PHY addr %d: PHYSID2 = 0x%04x\n", addr, phy_reg);

// 3. phy_device.c:1000 — 确认最终 phy_id
dev_info(&bus->dev, "PHY addr %d: final phy_id = 0x%08x\n", addr, phy_id);

// 4. phy_device.c:3117 — 确认驱动选择的路径
if (phydrv->features)
    dev_info(dev, "using static features from driver %s\n", phydrv->name);
else if (phydrv->get_features)
    dev_info(dev, "using get_features() from driver %s\n", phydrv->name);
else
    dev_info(dev, "using genphy_read_abilities()\n");

// 5. phy_device.c:3140 — 打印最终 supported 值
dev_info(dev, "final supported: %*pb\n",
         __ETHTOOL_LINK_MODE_MASK_NBITS, phydev->supported);
```

正常输出应该是：

```
PHY addr 1: PHYSID1 = 0x0141
PHY addr 1: PHYSID2 = 0x0dd1
PHY addr 1: final phy_id = 0x01410dd1
using static features from driver Marvell 88E1510
final supported: 00000000,00000000,...(大量 bits)
```

异常时可能看到：

```
PHY addr 1: PHYSID1 = 0xffff       ← 全 1，表示 PHY 未驱动总线
PHY addr 1: PHYSID2 = 0xffff
PHY addr 1: final phy_id = 0xFFFFFFFF  → get_phy_c22_id 返回 -ENODEV
```

或：

```
PHY addr 1: PHYSID1 = 0x0000       ← 浮空读为 0
PHY addr 1: PHYSID2 = 0x0000
PHY addr 1: final phy_id = 0x00000000   ← 通过校验但无驱动匹配
using get_features() from driver Generic PHY    ← genphy 回退
```

---

## 6. 总结

| 层级 | 现象 | 原因 |
|------|------|------|
| 用户可见 | `phylink_validate` 报错 `supported=00006000` | supported 位图中无实质链路能力 |
| 直接原因 | PHY 驱动的 `supported` features 未被正确填充 | `phy_probe()` 未通过 Marvell 驱动正常完成 |
| 根本原因 | PHY_ID 未被正确从硬件读取 | MDIO 总线通信失败 |
| 硬件根因 | **88E1512 PHY 芯片供电异常** | PHY 芯片无法响应 MDIO 读请求 |

逻辑链路总结：

```
PHY供电异常 → MDIO协议层无响应 → PHY_ID读取失败
                                    ↓
            ┌─ PHY_ID全1 → -ENODEV → PHY设备未创建 → stmmac_open失败
            │
            └─ PHY_ID为无效值 → PHY设备创建但无驱动匹配 → genphy回退
                                    ↓
                              supported features为空白/不完整
                                    ↓
                              phylink_validate() MAC校验失败
                                    ↓
                              内核打印报错日志
```
