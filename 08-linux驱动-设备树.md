# 设备树初看

---


# Linux设备树时钟配置解析：以RK3568 CRU为例

## 1. 时钟生产者与消费者模型

在Linux设备树中，时钟子系统采用“生产者/消费者”模型：

- **时钟生产者 (Provider)**：提供时钟信号的节点，例如晶振 (fixed-clock)、PLL、时钟控制器 (CRU, Clock Reset Unit) 等。设备树通过特定属性声明其能输出哪些时钟，以及如何引用这些输出。
- **时钟消费者 (Consumer)**：使用时钟信号的节点，例如UART、I2C、GMAC等外设。它们通过phandle+参数的方式，引用上游生产者提供的时钟输入。

一个节点可以同时承担两种角色：既从上级时钟源获取输入 (消费者)，又向下级设备提供多路时钟输出 (生产者)。

---

## 2. CRU节点的关键属性

以RK3568的CRU节点为例：

```c
    cru: clock-controller@fdd20000 {
        compatible = "rockchip,rk3568-cru";
        reg = <0x0 0xfdd20000 0x0 0x1000>;
        rockchip,grf = <&grf>;
        #clock-cells = <1>;
        #reset-cells = <1>;

        assigned-clocks =
            <&pmucru CLK_RTC_32K>, <&cru ACLK_RKVDEC_PRE>,
            <&cru CLK_RKVDEC_CORE>, <&pmucru PLL_PPLL>,
            <&pmucru PCLK_PMU>, <&cru PLL_CPLL>,
            <&cru CPLL_500M>, <&cru CPLL_333M>,
            <&cru CPLL_250M>, <&cru CPLL_125M>,
            <&cru CPLL_100M>, <&cru CPLL_62P5M>,
            <&cru CPLL_50M>, <&cru CPLL_25M>,
            <&cru PLL_GPLL>,
            <&cru ACLK_BUS>, <&cru PCLK_BUS>,
            <&cru ACLK_TOP_HIGH>, <&cru ACLK_TOP_LOW>,
            <&cru HCLK_TOP>, <&cru PCLK_TOP>,
            <&cru ACLK_PERIMID>, <&cru HCLK_PERIMID>,
            <&cru PLL_NPLL>, <&cru ACLK_PIPE>,
            <&cru PCLK_PIPE>, <&cru CLK_I2S0_8CH_TX_SRC>,
            <&cru CLK_I2S0_8CH_RX_SRC>, <&cru CLK_I2S1_8CH_TX_SRC>,
            <&cru CLK_I2S1_8CH_RX_SRC>, <&cru CLK_I2S2_2CH_SRC>,
            <&cru CLK_I2S2_2CH_SRC>, <&cru CLK_I2S3_2CH_RX_SRC>,
            <&cru CLK_I2S3_2CH_TX_SRC>, <&cru MCLK_SPDIF_8CH_SRC>,
            <&cru ACLK_VOP>;
        assigned-clock-rates =
            <32768>, <300000000>,
            <300000000>, <200000000>,
            <100000000>, <1000000000>,
            <500000000>, <333000000>,
            <250000000>, <125000000>,
            <100000000>, <62500000>,
            <50000000>, <25000000>,
            <1188000000>,
            <150000000>, <100000000>,
            <500000000>, <400000000>,
            <150000000>, <100000000>,
            <300000000>, <150000000>,
            <1200000000>, <400000000>,
            <100000000>, <1188000000>,
            <1188000000>, <1188000000>,
            <1188000000>, <1188000000>,
            <1188000000>, <1188000000>,
            <1188000000>, <1188000000>,
            <500000000>;
        assigned-clock-parents =
            <&pmucru CLK_RTC32K_FRAC>, <&cru PLL_GPLL>,
            <&cru PLL_GPLL>;
    };
```

---

### 2.1 `#clock-cells`

- **作用**：声明引用该节点时需要提供的参数个数。例如，`#clock-cells = <1>` 表示消费者在`clocks`属性中引用该节点时，必须带一个u32索引参数，用来指定具体使用该控制器的哪一路时钟输出。**该属性只有生产者才会有，且必须有**。
- **含义对比**：
  - `#clock-cells = <0>`：该节点只提供一个时钟输出，引用时仅使用phandle，不需要额外参数。例如一个固定频率晶振：
    
        osc: oscillator {
            compatible = "fixed-clock";
            #clock-cells = <0>;
            clock-frequency = <32678>;
        };
    
    消费者引用：`clocks = <&osc>;`
  - `#clock-cells = <1>`：提供多个时钟输出，引用时需要索引参数。RK3568的CRU就是典型例子，消费者引用：`clocks = <&cru SCLK_GMAC1_RX_TX>;`，其中`SCLK_GMAC1_RX_TX`是驱动定义的一个宏（数值ID）。

---

### 2.2 `assigned-clocks`、`assigned-clock-parents`、`assigned-clock-rates`

这三个属性是可选的，用于在设备树中对特定的时钟进行默认配置，而不必等驱动程序动态设置。它们共同描述了一个“时钟预配置”列表。**这些属性主要是用于在驱动之前配置相关时钟，一般都是消费者会配置**。
另外，从代码的角度，`assigned-clocks`可以在任意节点中出现并配置。但是从逻辑上讲，一般只有关注特定时钟的节点才会去用`assigned-clocks`来配置该时钟，**这一般也意味着该节点是时钟的消费者**。不过在设备树中可以经常看到，这些节点可能并不会用`clocks`属性来声明它会使用该时钟。因为在驱动代码中也有办法拿到时钟。

> 这里引申一下为什么会出现这种情况。这里直觉不应该是在设备树节点中用`<clocks>`来消费时钟就好吗。
> 实际上涉及新老驱动的历史问题。老版本没有设备树时，是直接通过驱动在全局符号中寻找时钟，然后使用它。而设备树带来了新的方法，也就是用`<clocks>`来显式指定消费时钟，然后驱动中直接从设备树中读取时钟消费。这种方法更直观，但是与老的方法相比改动会非常大。因此老驱动在移植时一般就不做更改了，对应的节点设备树也仍然保留原来那一套全局获取的方法。


- **`assigned-clocks`**：列出需要预配置的时钟引用，格式与消费者`clocks`属性完全相同，每个条目都是`<&provider 参数1 ...> `，必须按顺序严格对应后面两个属性数组的元素。
- **`assigned-clock-parents`**：为`assigned-clocks`中列出的每个时钟指定其父时钟。如果某个时钟不需要设置父时钟，该位置可以写`<0>`或者省略（但顺序必须与`assigned-clocks`对齐）。
- **`assigned-clock-rates`**：为`assigned-clocks`中列出的每个时钟指定目标输出频率，单位为Hz。同样按顺序对应。

示例分析：

```c
    assigned-clocks = <&cru ACLK_BUS>, <&cru PCLK_BUS>;
    assigned-clock-rates = <150000000>, <100000000>;
```

含义：将`ACLK_BUS`时钟默认频率设为150MHz，将`PCLK_BUS`时钟设为100MHz。这两个时钟都不改变父时钟（在`assigned-clock-parents`中没有对应条目，或对应位置为`<0>`）。

```c
    assigned-clocks = <&pmucru CLK_RTC_32K>;
    assigned-clock-parents = <&pmucru CLK_RTC32K_FRAC>;
    assigned-clock-rates = <32768>;
```

含义：将`CLK_RTC_32K`时钟的父时钟设置为`CLK_RTC32K_FRAC`，并设定输出频率为32768Hz。

**注意**：`assigned-clocks`中的时钟数量可以多于`assigned-clock-parents`或`assigned-clock-rates`，对于未提供对应条目的时钟，不会进行相应配置。但为了方便阅读和维护，通常三个列表的长度会保持一致（未指定的位置用空项或`<0>`占位）。

---

## 3. 为什么CRU节点没有逐一列出所有时钟，却仍能被引用？

这是初学者最常感到困惑的地方。CRU作为系统主时钟控制器，提供上百个时钟输出，但它并没有在设备树中使用类似`clock-output-names`或者逐个声明时钟ID。其他节点仍然可以通过`<&cru SOME_CLK_ID>`的方式引用，原因在于：

1. **时钟的具体定义位于驱动程序**。当内核启动时，`rockchip,rk3568-cru`驱动会注册一个`clock provider`，并在内部根据芯片手册创建所有时钟对象（包括门控、分频器、多路选择器等）。这些时钟的层级关系和ID（即索引号）由驱动中的静态表（`rk3568_clk_branches[]`等）决定。

2. **设备树只负责“引用描述”**。驱动注册完后，设备树中的`<&cru ID>`语法实质上是获取该provider下的第`ID`个时钟。这个ID是一个整数，由对应的宏定义（位于`include/dt-bindings/clock/rk3568-cru.h`）提供。例如`SCLK_GMAC1_RX_TX`就是一个整数宏。设备树本身不负责定义这些ID的具体含义，只是通过宏将符号名映射为数字，最终传递给驱动。

3. **供应商自定义clock-cells策略**。对于固定频率晶振（`fixed-clock`）这类简单provider，它只有一路输出，并且频率固定，所以在设备树中使用`clock-frequency`直接描述。而对于CRU这类复杂控制器，数百个时钟的参数和拓扑都由硬件寄存器动态决定，无法也不应该在设备树中静态枚举，因此采用“驱动注册+设备树索引”的方案。`assigned-clocks`仅仅是允许对其中少数时钟进行默认配置，不代表所有时钟都必须在此列出。

---

## 4. 完整引用示例：GMAC时钟配置

以下是一个典型的消费者节点（千兆以太网控制器）引用CRU时钟的例子：

```c
    &gmac1 {
        assigned-clocks = <&cru SCLK_GMAC1_RX_TX>, <&cru SCLK_GMAC1>;
        assigned-clock-parents = <&cru SCLK_GMAC1_RGMII_SPEED>, <&cru CLK_MAC1_2TOP>;
        assigned-clock-rates = <0>, <125000000>;
    };
```

解读：
- **`<&cru SCLK_GMAC1_RX_TX>`**：引用CRU提供的`SCLK_GMAC1_RX_TX`时钟，其宏定义值在驱动中对应特定的时钟路径。
- 将该时钟的父时钟设置为`SCLK_GMAC1_RGMII_SPEED`（也是一个CRU输出的时钟），速率设置为`<0>`，即不改变其速率，由父时钟速率和分频系数决定。
- **`<&cru SCLK_GMAC1>`**：引用GMAC1的主工作时钟，设置父时钟为`CLK_MAC1_2TOP`，速率设置为125MHz。

这些引用之所以能够解析，就是因为`cru`节点已经通过`#clock-cells = <1>`声明了提供者身份，并且对应的时钟ID在内核的时钟驱动中真实存在。

---

## 5. 总结

- 设备树中，时钟提供者的核心属性是`#clock-cells`，它决定了消费者引用时的参数格式。
- 复杂时钟控制器（如CRU）不枚举所有时钟输出，其具体定义位于驱动程序内部。设备树仅通过phandle+ID进行轻量级引用。
- `assigned-clocks/assigned-clock-parents/assigned-clock-rates`三个属性用于系统启动时对指定时钟进行预配置（设置父时钟、频率），但不是必须的，且不需要覆盖所有时钟。
- 开发者在设备树中添加新外设时钟时，只需知道正确的时钟ID宏（来自dt-bindings头文件）以及对应的provider节点引用即可，无需修改provider节点本身。

掌握以上概念，即可解开关于RK3568 CRU节点以及其他类似时钟控制器节点配置的绝大部分迷惑。


