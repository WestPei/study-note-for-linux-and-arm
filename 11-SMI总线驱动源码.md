- [1. 源码解析](#1-源码解析)
  - [1.1. 整体结构](#11-整体结构)
    - [1.1.1. `smi_bus` 驱动结构体](#111-smi_bus-驱动结构体)
    - [1.1.2. `sys_info` 全局结构](#112-sys_info-全局结构)
    - [1.1.3. `smi_data_t` 用户空间与内核交换数据的结构](#113-smi_data_t-用户空间与内核交换数据的结构)
    - [1.1.4. `cdev_info` 字符设备管理结构](#114-cdev_info-字符设备管理结构)
    - [1.1.5. 字符设备操作函数](#115-字符设备操作函数)
  - [1.2. 从 probe 入手](#12-从-probe-入手)
    - [1.2.1. 在 probe 之前](#121-在-probe-之前)
    - [1.2.2. 基本变量配置与设备树节点对应](#122-基本变量配置与设备树节点对应)
    - [1.2.3. 寄存器互斥锁初始化](#123-寄存器互斥锁初始化)
    - [1.2.4. MDIO 总线对象解析](#124-mdio-总线对象解析)
    - [1.2.5. 字符设备注册](#125-字符设备注册)
  - [1.3. 字符设备操作](#13-字符设备操作)

---

# 1. 源码解析

这是岩哥写的一个将交换芯片的驱动(MDIO 总线控制)给封装成一个字符设备的驱动，用于学习 MDIO 相关内容，以及字符设备驱动的写法。

> D老师的概述：这个驱动是为 Marvell 88E6390 等 MV88E6xxx 系列交换芯片 写的。它通过 Linux 内核的 **MDIO** (Management Data Input/Output) 子系统访问交换芯片的内部寄存器（SMI 接口，即 Serial Management Interface，本质上就是 MDIO）。
> 同时，它在 `/dev/` 下创建了一个字符设备（`/dev/smi_bus`），用户空间的程序可以通过 `ioctl` 来：
> * 读写 MDIO 设备（phyAddr）上的指定寄存器（regAddr）
> * 加锁 / 解锁一个“寄存器锁”，用于保证多线程或多进程访问时的原子性
> 
> 驱动采用 platform_driver 模型，通过设备树（Device Tree）来获取 MDIO 总线信息。

svn的路径:
* [设备树节点](https://192.168.1.77/svn/svn/Cabin_SW/trunk/PLATFORM/ARM_PLATFORM/EDC/00-code/kernel/arch/arm/boot/dts)
  * `imx6sx.dtsi`
  * `imx6sx-sdb.dtsi`
* [驱动源码](https://192.168.1.77/svn/svn/Cabin_SW/trunk/PLATFORM/ARM_PLATFORM/EDC/00-code/kernel/drivers/smibus)
  * `smi_bus.c`
  * `smi_bus.h`

D老师顺便给了一个概览，让我在学习了基础的设备驱动后快速对比上手。我们先尝试编写了一个 I2C 总线的驱动，并且不涉及字符/块设备，就是简单的寄存器配置。这里D老师帮忙进行了对比：
* **总线类型不同**：SMI 总线驱动操作的是 MDIO 总线，通常用于以太网 PHY 或者交换机。
* **该驱动不直接驱动交换芯片的各个功能**：而是提供了**原始寄存器访问通道**给用户空间，让用户空间程序(如交换机配置工具)自己去独写芯片寄存器。
* **该驱动是 platform 驱动**，而不是 I2C/SPI 驱动，因为 MDIO 总线在设备树中是一个独立的总线节点，需要通过 phandle 引用。



---

## 1.1. 整体结构

### 1.1.1. `smi_bus` 驱动结构体

首先这还是一个驱动模块，其结构与其他的驱动当然也是基本一致的，直接拉到代码的最底部，这里一般会有这个驱动模块的结构体定义和注册，从这里可以知晓一些关于这个模块的基本信息。

```c
static struct platform_driver smi_bus = {
    .probe = smi_bus_probe,
    .remove = smi_bus_remove,
    .driver = {
        .name = "marvell 88e6390",
        .of_match_table = smi_bus_of_match,
    },
};
module_platform_driver(smi_bus);
```

这是一个标准的平台设备驱动，适配 marvell 88e6390交换芯片。注意最后的 `module_platform_driver(*)`，这是一个内核提供的用于注册平台设备驱动的宏，如果我们没有在模块初始化时做些什么特殊的操作(现在Linux驱动应该都不太会涉及)，就可以直接使用该宏来完成驱动模块的初始化(`init` 操作，注意与 `probe` 区分)

```c
static const struct of_device_id smi_bus_of_match[] = {
    {
        .compatible = "marvell,smi_bus",
    },
    { /* sentinel */ },
};
```

这是该驱动用于 `match` 时的匹配字段，与设备树节点中的 `compatible` 字段进行匹配，同时也方便我们在庞大的文件中快速查找对应的节点。这里该字段为 `marvell,smi_bus`。

### 1.1.2. `sys_info` 全局结构

该结构是在头文件中定义的：

```c
typedef struct {
    mii_bus_t*   smi_bus[SMI_BUS_MAX];   // 最多支持 2 条 MDIO 总线
    recursive_mutex reg_lock[SMI_DEV_NUM]; // 每个设备一个锁（目前只支持 1 个）
} sys_info;
sys_info sys;
```

### 1.1.3. `smi_data_t` 用户空间与内核交换数据的结构

```c
typedef struct {
    u8 busID;      // 使用哪条 MDIO 总线（0 或 1）
    u8 devNum;     // 设备号（用于锁，目前固定为 0）
    u8 phyAddr;    // MDIO 设备地址（PHY 地址）
    u8 regAddr;    // 寄存器地址（5 位或更宽，取决于芯片）
    u16 value;     // 要写入的值 / 读出的值
} smi_data_t;
```

### 1.1.4. `cdev_info` 字符设备管理结构

```c
typedef struct {
    const char*         name;      // 设备名 "smi_bus"
    cdev_t              cdev;      // 内核字符设备结构
    dev_t               devno;     // 设备号（主+次）
    class_t*            devclass;  // 设备类（用于自动生成 /dev 节点）
    const file_ops_t*   ops;       // file_operations 指针
} cdev_info;
```

### 1.1.5. 字符设备操作函数

```c
static int smi_open(...) { try_module_get(THIS_MODULE); return 0; }
static int smi_release(...) { module_put(THIS_MODULE); return 0; }
```

`open` 与 `release` 负责增加/减少模块的引用计数，这是标准操作。

而关于 `smi_ioctl()` 这个字符设备的核心交互函数，会在后续用单独的章节进行解析：

[字符设备操作函数详解](#13-字符设备操作)

---

## 1.2. 从 probe 入手

在了解了模块的基本结构后，我们就可以先从 `probe` 函数入手，了解该驱动在初始化时到底都做了哪些事情。

### 1.2.1. 在 probe 之前

在内核解析设备树时，会为设备树的各个节点创建对应的设备结构体，这里咱们使用的是平台设备，对应平台驱动，驱动结构体定义中会有各个函数指针：

```c
/* include/linux/platform_device.h */

struct platform_driver {
	int (*probe)(struct platform_device *);

	int (*remove)(struct platform_device *);
	void (*remove_new)(struct platform_device *);

	void (*shutdown)(struct platform_device *);
	int (*suspend)(struct platform_device *, pm_message_t state);
	int (*resume)(struct platform_device *);
	struct device_driver driver;
	const struct platform_device_id *id_table;
	bool prevent_deferred_probe;

	bool driver_managed_dma;
};
```

内核会为设备节点创建对应的总线类型的设备结构体子类(内含一个`struct device`)，这里自然就是 `platform_device`。我们可以对应去设备树文件中找一找对应 `compatible` 字段的节点，这样我们就知道具体是在操作哪一个节点了。这样也会让我们对 `probe` 代码理解更加清晰。这里我们找到的是：

```c
/ {
	smi-dev {
		compatible = "marvell,smi_bus";
		cpu_port = <&fec2>;
		mdio_bus1 = <&mdio1>;
		mdio_bus2 = <&mdio2>;
	};

    ...
}
```

它是直接挂在根节点下的平台设备，这样就对上了。我们继续来看 `probe` 的源码。

### 1.2.2. 基本变量配置与设备树节点对应

我们将源码拆解，逐步进行分析。

```c
static int smi_bus_probe(struct platform_device *pdev){
    u8 cnt = 0;
    int retval = 0;
    struct device_node *smi_node;
    char smi_bus_name[SMI_BUS_NAME_MAX];

    struct device_node *switch_node = of_parse_phandle(pdev->dev.of_node, "cpu_port", 0);
    ...
}
```

首先是一些基本的变量定义，然后是从设备树节点中获取 `switch_node`，它是通过在 `smi-dev` 节点下去寻找名为 `cpu-port` 的一个 `phandle`。可以看到整个 `phandle` 指向 `fec2`。而 `fec2` 我们在设备树中查找到：

```c
/ {
    aliases {
        ...
        ethernet1 = &fec2;
    }

    fec2: ethernet@021b4000 {
				compatible = "fsl,imx6sx-fec", "fsl,imx6q-fec";
				reg = <0x021b4000 0x4000>;
				interrupts = <GIC_SPI 102 IRQ_TYPE_LEVEL_HIGH>,
					     <GIC_SPI 103 IRQ_TYPE_LEVEL_HIGH>;
				clocks = <&clks IMX6SX_CLK_ENET>,
					 <&clks IMX6SX_CLK_ENET>,
					 <&clks IMX6SX_CLK_ENET_PTP>,
					 <&clks IMX6SX_CLK_ENET2_REF_125M>,
					 <&clks IMX6SX_CLK_ENET_PTP>;
				clock-names = "ipg", "ahb", "ptp",
					      "enet_clk_ref", "enet_out";
				fsl,num-tx-queues=<3>;
				fsl,num-rx-queues=<3>;
				stop-mode = <&gpr 0x10 4>;
				fsl,wakeup_irq = <0>;
				status = "disabled";
			};
}
```

`fec2` 指向了这个 `ethernet@021b4000`节点。这是 `fec2` 以太网控制器的基础节点设置，在具体的板级配置中可能会有额外的配置，由于这里涉及交换芯片，配置复杂，先不给出。这里的重点是看 `probe` 函数。

同时 `switch_node` 也就指向了这个以太网控制器。

### 1.2.3. 寄存器互斥锁初始化

```c
    for (cnt = 0; cnt < SMI_DEV_NUM; cnt++)
    {
        retval = smi_bus_chip_reg_lock_init(switch_node, cnt);
        if (retval != 0)
            goto probe_delay;
    }
```

从名字来看应该是对 smi_bu设备的寄存器锁进行初始化，看看函数内部。

```c
static int smi_bus_chip_reg_lock_init(struct device_node *node, int dev_num)
{
    struct net_device *switch_dev = of_find_net_device_by_node(node);
    
    if (switch_dev == NULL)
        return -EFAULT;

    struct dsa_port *cpu_dp = switch_dev->dsa_ptr;
    if (cpu_dp == NULL)
        return -EFAULT;
    
    struct dsa_switch *ds = cpu_dp->ds->dst->ds[dev_num];
    if (ds == NULL)
        return -EFAULT;
    
    struct mv88e6xxx_chip *chip = ds->priv;
    if (chip == NULL)
        return -EFAULT;
    
    mutex_t *semid = &(chip->reg_lock);
    if (semid == NULL)
        return -EFAULT;
    
    sys.reg_lock[dev_num].semid = semid;
    sys.reg_lock[dev_num].ref   = 0;

    return 0;
}
```

`of_find_net_device_by_node(switch_node)` 会根据 `fec2` 节点找到预期关联的 `struct net_device` 。

我再往函数内部看，发现就是在`net_class` 这个设备类中寻找我们的 `fec2` 设备，看看他有没有注册成为网络设备。所以这里要求我们的 GMAC 芯片首先得完成设备的注册。

后续的步骤我没太看懂，D老师告诉我这是 通过 DSA 子系统的数据结构链(`dsa-ptr`->`ds`->`dst`->`ds[dev_num]`->`priv`)来找到 Marvell 交换芯片驱动中要控制的寄存器锁 `reg_lock` (互斥锁)的地址。

然后这个锁会被保存在 `sys.reg_lock[dev_num].semid` 中，供 `smi_ioctl` 中的 `SMI_LCOK_REG/SMI_UNLOCK_REG` 来使用。

### 1.2.4. MDIO 总线对象解析

```c
    for (cnt = 0; cnt < SMI_BUS_MAX; cnt++)
    {
        sprintf(smi_bus_name, "mdio_bus%d", cnt+1);
        smi_node = of_parse_phandle(pdev->dev.of_node, smi_bus_name, 0);
        printk("[%s:%d]smi_node->fullname = %s smi_node->name = %s\n",
                __func__, __LINE__, smi_node->full_name, smi_node->name);
        
        if (smi_node == NULL)
        {
            printk(KERN_ERR "of_parse_phandle %s fail\n", smi_bus_name);
            retval = -EINVAL;
            goto out;
        }
        
        sys.smi_bus[cnt] = of_mdio_find_bus(smi_node);
        if (sys.smi_bus[cnt] == NULL) {
            retval = -ENODEV;
            goto out;
        }
    }
```

从设备树节点中可以看到有两个MDIO 总线设备，这里硬编码为

```c
/* drivers/smibus/smi_bus.h */

#define SMI_BUS_MAX                     2
```

还是通过 `of_parse_phandle` 将得到的 MDIO 总线对象解析并保存到 `smi_node` 之中。在设备树文件中查找对应的 `phandle`：

```c
&fec1 {
    pinctrl-names = "default";
    pinctrl-0 = <&pinctrl_enet1>;
    phy-mode = "rgmii-id";
    phy-handle = <&ethphy1>;
    status = "okay";
    
    mdio1: mdio@1{
        ...
    };
};

&fec2 {
    ...

    mdio2: mdio@2 {
        #address-cells = <1>;
        #size-celss = <0>;

        switch1: sw@1 {
            ...
        };

        switch2: switch@2 {
            ...
        };
    };
};
```

这里找到的两个 MDIO 总线设备节点分别在两个网络控制器下。`of_mdio_find_bus(smi_node)` 根据我们找到的 MDIO 总线设备节点找到对应的 `struct mii_bus *` (MDIO 总线对象)。这些对象一般都在 `fec1` 和 `fec2` 驱动在 `probe` 时进行注册。

### 1.2.5. 字符设备注册

```c
    for (cnt = 0; cnt < sizeof(char_dev)/sizeof(char_dev[0]); cnt++)
    {
        alloc_chrdev_region(&char_dev[cnt].devqno, 0, 1, char_dev[cnt].name);
        cdev_init(&char_dev[cnt].cdev, char_dev[cnt].ops);
        retval = cdev_add(&char_dev[cnt].cdev, char_dev[cnt].devno, 1);
        if (retval) {
            printk(KERN_ERR "Unable to get chr_dev major\n");
            unregister_chrdev_region(char_dev[cnt].devno, 1);
            goto out;
        }
        
        char_dev[cnt].devclass = class_create(THIS_MODULE, char_dev[cnt].name);
        if (IS_ERR(char_dev[cnt].devclass))
        {
            printk(KERN_ERR "Create switch_dev class fail!\n");
            unregister_chrdev_region(char_dev[cnt].devno, 1);
            goto out;
        }
        
        device_create(char_dev[cnt].devclass, NULL, char_dev[cnt].devno, 0, char_dev[cnt].name);
    }
```

这一块应该是一个比较标准的字符设备驱动的注册流程，可以说是独立于 MDIO 总线设备之外的一个单独的流程。在开发其他字符设备驱动时，也是可以借鉴的：

1. 动态分配设备号： `alloc_chrdev_region` 让内核自动分配主设备号。
2. 初始化 `cdev`：`cdev_init`绑定 `smi_ops` (包含 .open/.release/.unlocked_ioctl)。
3. 添加到内核：`cdev_add`。
4. 创建设备类：`class_create`在`/sys/class`下创建目录。
5. 创建新字符设备：`device_create`会在`/dev`下生成`smi_bus`节点，同时触发`uevent`。





---

## 1.3. 字符设备操作



---