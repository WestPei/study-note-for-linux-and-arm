- [1. I2C总线协议](#1-i2c总线协议)
	- [1.1. 简介](#11-简介)
	- [1.2. 基本概念](#12-基本概念)
	- [1.3. 工作原理](#13-工作原理)
	- [1.4. 数据传输流程](#14-数据传输流程)
	- [1.5. 细节解析](#15-细节解析)
- [2. I2C控制器: 从设备树到内核](#2-i2c控制器-从设备树到内核)
	- [2.1. I2C 的设备地址](#21-i2c-的设备地址)
		- [2.1.1. I2C 控制器地址](#211-i2c-控制器地址)
		- [2.1.2. I2C 从设备地址](#212-i2c-从设备地址)
	- [2.2. 设备树节点解析](#22-设备树节点解析)
	- [2.3. I2C 控制器设备匹配驱动](#23-i2c-控制器设备匹配驱动)
		- [2.3.1. `match`](#231-match)
		- [2.3.2. `probe`](#232-probe)
	- [2.4. I2C 控制器驱动匹配设备](#24-i2c-控制器驱动匹配设备)
		- [2.4.1. 驱动源码](#241-驱动源码)
		- [2.4.2. 向内核注册](#242-向内核注册)
		- [2.4.3. 控制器驱动初始化](#243-控制器驱动初始化)
	- [2.5. 小结](#25-小结)
- [3. I2C 从设备: 注册与匹配](#3-i2c-从设备-注册与匹配)
	- [3.1. 设备树到内核节点](#31-设备树到内核节点)
	- [3.2. 从设备 probe](#32-从设备-probe)
- [4. SMBus协议](#4-smbus协议)
	- [4.1. 历史渊源](#41-历史渊源)
	- [4.2. I2C VS SMBus](#42-i2c-vs-smbus)
	- [4.3. SMBus接口解析](#43-smbus接口解析)
		- [4.3.1. 关于`swapped`](#431-关于swapped)
		- [4.3.2. `i2c_smbus_read_byte()`](#432-i2c_smbus_read_byte)
			- [4.3.2.1. I2C总线 锁](#4321-i2c总线-锁)
			- [4.3.2.2. `__i2c_smbus_xfer()`](#4322-__i2c_smbus_xfer)
			- [4.3.2.3. 追寻真正的硬件驱动函数](#4323-追寻真正的硬件驱动函数)
			- [4.3.2.4. `i2c_smbus_xfer_emulated()`](#4324-i2c_smbus_xfer_emulated)
			- [4.3.2.5. `__i2c_transfer()`](#4325-__i2c_transfer)
			- [4.3.2.6. 再次回到硬件驱动](#4326-再次回到硬件驱动)
- [5. MMIO：CPU 与 外设寄存器的交互](#5-mmiocpu-与-外设寄存器的交互)
	- [5.1. 回顾一下](#51-回顾一下)
		- [5.1.1. 啥是 MMIO](#511-啥是-mmio)
		- [5.1.2. 为什么会使用 MMIO](#512-为什么会使用-mmio)
	- [5.2. 小结](#52-小结)


---

# 1. I2C总线协议

光是知道如何使用I2C设备，在驱动中调用接口显然是不够的。还需要了解I2C总线的软硬件。

有博客讲解该协议：
* [I2C总线和通信协议详解](https://zhuanlan.zhihu.com/p/678229227)。

* [一文看懂I2C通信](https://blog.csdn.net/lidashent/article/details/150726465)

这里只讲我感兴趣的，更详细的内容参考博客。

---

## 1.1. 简介

I2C(Inter-Integrated Circuit)通信总线。它只有两根信号线，总线上可挂载多个设备。其用途主要在低速，多设备的通信。

I2C总线技术最早由荷兰飞利浦半导体(现在的恩智浦NXP半导体)在1982年开发。最初是为了解决电视机内部电子系统复杂布线和降低制造成本的。后来则在嵌入式电子领域得到了广泛应用。从最初的标准模式(100kHz)到快速模式(400kHz)和高速模式(3.4MHz)。

---

## 1.2. 基本概念

1. I2C只有两根信号线，分别是串行数据线`SDA`与串行时钟线`SCL`。
2. I2C支持多个主设备和多个从设备连接到同一根总线上，每个设备地址唯一。地址由7位或10位构成。
3. I2C是同步通信，由时钟信号`SCL`进行控制。
4. I2C支持阻塞传输，主设备可以在传输过程中控制总线，防止其他设备发送数据。

---

## 1.3. 工作原理

1. 总线控制：
   * 一次通信由主设备通过在`SDA`线上生成特定的信号模式来开始和结束。
   * 每次通信开始时，主设备发送一个地址帧来指定通信的从设备。

2. 数据传输：
   * 主设备控制时钟信号，向从设备发送或接受数据。
   * 接收方会在每一个字节后发送一个应答位`ACK`或者非应答位`NACK`来告知发送方数据是否接收到。

---

## 1.4. 数据传输流程

主要是看时序图了解，直接看博客就好。这里简单讲解一下。
数据是由字节为单位进行传输。
1. **开始条件**：`SCL`在高电平时`SDA`拉低。然后在每一个时钟脉冲期间传输1位数据。
* 完成一个8位数据后主设备会释放`SDA`线。此时`SDA` 线保持高位，从设备如果完成接收，则会主动拉低`SDA`线(即`ACK`)。如果没有拉低，则表示未响应(从设备忙或错误)，主设备会处理。
* 一般一次通信由三个字节构成：从设备地址+从设备寄存器地址+数据。(不是协议规范)
* 需要注意的是一般设备地址由7位构成，那么第一个字节的数据最后会选择`R/W`，这将决定数据帧的数据流向。

2. **停止条件**：`SCL`在高电平时`SDA`拉高。

---

## 1.5. 细节解析

1. 总线空闲与忙碌：
   * 总线上的传输流程都由`START(S)`开始，并由`STOP(P)`结束。这两个条件都由主设备产生。在`S`之后，总线被认为是忙碌的，在`P`之后总线被认为是空闲的。
   可能出现重复的`S`而没有`P`，这就是`Sr`，它与`S`在功能上是一样的，此时总线保持忙碌。
   * `SDA`与`SCL`在总线空闲时都是**拉高状态**。通过开始条件和停止条件我们可以发现这也是对应上的。需要注意的是`SCL`时钟信号也只在总线忙碌时才有脉冲(由主设备给)。
   也就是说从设备和主设备如果不去操作信号线，**信号线默认是拉高的**。

	* `MSB`和`LSB`：
   这是在总线协议中经常见到的概念。`MSB`(Most Significant Bit/Byte)是指权重最大的位，一般就是一个8位二进制数的最高位，它对数值的影响最大。
   而`LSB`(Least Significant Bit/Byte)则与之相反，是指权重最小的位，一般就是`bit0`。

2. 确认位`ACK`与`NACK`：
   * 确认发生在每一个字节之后。确认位允许接收端向发送端发出信号。**主设备产生所有的时钟信号，包括确认位使用的第九个时钟脉冲**。
	* 确认位定义为：发送端 在确实时钟脉冲期间释放`SDA`线，这样接收端就可以拉低`SDA`，并且在该时钟脉冲期间保持稳定。这样就表示接收端成功接收到了这一字节的数据。
	* `NACK`就是接收端没有做动作。造成这样的原因有多种，这里就不一一举例了。

3. 时钟同步：
	* 总线上存在多主设备时需要进行时钟同步和仲裁，来决定哪一个主设备控制总线并进行传输。单主设备系统中是不需要这两个机制的。
	* 简单来说`SCL`线由最先拉低该线的主设备拉低，由最后拉高`SCL`线的主设备拉高。`SCL`线上的时钟脉冲，`LOW`周期由`LOW`周期最长的主设备决定，`HIGH`周期由`HIGH`周期最长的主设备决定。
	* **注意**，最后得到的`SCL`的时钟脉冲周期与任何一个主设备自己的时钟频率都对不上的，会比他们都慢(这个结果很显然)。但是达到了所有主设备的时钟同步的要求。

4. 仲裁：
	* 当在最小保持时间内(这里怎么理解？)有多个设备产生了`S`，那么就会触发仲裁来决定究竟哪一个主设备进行传输。
	* `SDA`线上只要有一个主设备将其拉低，那他就会保持拉低。而主设备会在`SCL`周期内去检查`SDA`的店铺是否与它所发送的相匹配。一旦不匹配该主设备就失去仲裁并关闭`SDA`输出。(主设备怎么做到向`SDA`发送数据的同时检测`SDA`数据?)
	* 可以看到，I2C总线没有中心主设备，不存在优先级。仲裁结果完全由主设备发送的地址和数据决定。
	* 不过可以看到这种仲裁方式存在一些未定义的情况，这里不细究，感兴趣的可以看看。

5. 时钟拉伸：
	* 通过将`SCL`线持续拉低来暂停传输。比如从设备正在处理实时任务时无法继续接收数据时可以拉低`SCL`来暂停传输。当然这需要从设备具备`SCL`驱动。

6. I2C地址：
	* 一般我们使用的一主多从的I2C系统下，主设备(一般就是我们的SoC)是不需要配置地址的，它用于向传感器等从设备发出指令进行独写，可以看作是一种"单向"控制器。
	* 从设备的地址一般处于半固定状态。不同的I2C设备其地址可配置程度不同。一般都是由`芯片制造商固定部分(高四位或更多)+允许用户配置的可变部分(低三位)`组成。也存在通过I2C命令修改内部寄存器或EEPROM从而改变地址的情况(复杂设备)。

还有一些稍微复杂一些的10位寻址等等，可以看博客自行了解。



---


# 2. I2C控制器: 从设备树到内核

## 2.1. I2C 的设备地址

在硬件层面我们知道，SoC 比如我们的 rk3568 自带有 I2C 控制器，可以通过查看芯片手册来确定 I2C 控制器的内存地址。然后硬件方面设计会将 I2C 设备挂在 I2C 控制器下，也会有自己的地址。

### 2.1.1. I2C 控制器地址

以 rk3568 为例，我们直接查看官方的技术手册，一般会有一章 Address Mapping，里面会给出各类硬件的地址映射情况。我们这里去寻找 I2C2 这个控制器的地址。

| Module | Start Address | Size |
| ------ | ------------- | ---- |
| I2C2   | 0xFE5B0000 | 64 KB |

然后我们对应去看看设备树的节点：

```c
i2c2: i2c@fe5b0000 {
		compatible = "rockchip,rk3568-i2c", "rockchip,rk3399-i2c";
		reg = <0x0 0xfe5b0000 0x0 0x1000>;
		interrupts = <GIC_SPI 48 IRQ_TYPE_LEVEL_HIGH>;
		clocks = <&cru CLK_I2C2>, <&cru PCLK_I2C2>;
		clock-names = "i2c", "pclk";
		pinctrl-0 = <&i2c2m0_xfer>;
		pinctrl-names = "default";
		#address-cells = <1>;
		#size-cells = <0>;
		status = "disabled";
	};
```

注意看节点的地址部分，是不是对应上了。一方面是节点名字中的 `i2c@fe5b0000`，另一个就是节点属性中的 `reg`。不过这里一共有四个地址。

这里的地址是 SoC 访问片上 I2C 控制器的关键，这4个数组成了一个完整的64位的物理地址段：

1. 第一个数 `0x0`：物理基地址的高32位。
2. 第二个数 `0xfe5b0000`：物理基地址的低32位。
3. 第三个数 `0x0`：地址长度的高32位。
4. 第四个数 `0x1000`：地址长度的低32位。

组合起来就是：
* I2C2 控制器的寄存器其实物理地址为 `0x00000000 fe5b0000 = 0xfe5b0000`。
* 它占用的地址空间长度为 `0x00000000 00001000 = 0x1000 字节 = 4096 KB`。


### 2.1.2. I2C 从设备地址

这里需要注意的是 I2C 从设备使用的是 7 位地址(少数使用 10 位)，这个地址一般是由 **芯片制造商**和**硬件电路设计者**共同决定。

就拿我们使用的 max6635 温度传感器芯片为例。阅读芯片手册中关于地址选择的部分得知，max6635 的 7 位地址有 2 位可编程，这两位是后两位。具体引脚是：

| PIN | NAME | FUNCTION |
| --- | ---- | -------- |
|  6  |  A1  | Address Pin |
|  7  |  A0  | Address Pin |

然后对应的具体硬件地址就有四个，这意味着同一根总线下可以通过可编程地址位实现 4 个传感器连接。

| A1 | A0 | ADDRESS |
| -- | -- | ------- |
| GND | GND | 1001 000 |
| GND | Vcc | 1001 001 |
| Vcc | GND | 1001 010 |
| Vcc | Vcc | 1001 011 |

我们将这 7 位地址转换为 16 进制就可以知道 max6635可以使用的地址为 `0x48`，`0x49`，`0x4a`，`0x4b`。

我们同样的如果要在 I2C2 控制器下增加这么一个 max6635 设备节点，其对应的地址就应该是这四个中的一个，至于是哪一个需要根据硬件原理图来确定。

```c
&i2c2 {
	max6635: hwmon@48 {
		compatible = "national,max6635";
		reg = <0x48>;
		
		//硬件需要做更正，alert和critial中断线需要处于一个中断源，
		//因为一个中断源对应着一个时钟源，而一个普通监管设备一般只有一个时钟源
		//interrupt-names = "alert", "critical";
		//interrupts = <RK_PC2 IRQ_TYPE_LEVEL_LOW>, <RK_PA5 IRQ_TYPE_LEVEL_LOW>;
		#thermal-sensor-cells = <0>;
	};
}
```

这里是一个已经写好的地址，可以看到这里配置好的地址为 `0x48`。符合硬件的要求。

## 2.2. 设备树节点解析

Bootloader 阶段会将编译好的设备树二进制文件(.dtb)的地址通过寄存器传递给内核。我们在自己使用 `booti` 启动内核时会将设备树加载到内存，然后作为参数调用 `booti`。

而在内核早期的启动阶段，大概是这样的：

```c
start_kernel()	/* init/main.c */
	→ setup_arch()	/* arch/arm64/kernel/setup.c */
		→ unflatten_device_tree() /* drivers/of/fdt.c */
```

这个函数会将结构化的二进制`.dtb`文件展开成内核可以理解的一系列 `struct device_node` 数据结构。

在之后，ARM64架构下，内核会通过一个 `arch_initcal_sync` 级别的函数来触发设备树向平台设备(`platform_device`)的转换。核心入口在 `drivers/of/platform.c` 中的 `of_platform_populate_init()`。

```c
arch_initcall_sync(of_platform_default_populate_init);
```

> `arch_initcall_sync` 是 Linux 内核初始化过程的一个重要机制，用于标记并组织那些与体系结构相关，且需要在同步阶段执行的初始化函数。它确保了内核组件以预定的顺序完成初始化。
> 内核启动时 `do_initcalls()` 会按顺序遍历每一个 `initcall` 级别中的函数，同级别的所有普通函数执行完之后，就会执行同一级别的同步函数 `*_sync`

我们先不管这个 `arch_initcall_sync` 具体的调用情况，但是它肯定是发生在设备树文件解析之后的。我们接下来直接看平台设备的解析。

```c
// 文件: drivers/of/platform.c
of_platform_default_populate_init()
    └─> of_platform_default_populate(NULL, NULL, NULL)
        └─> of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL)
            └─> of_platform_bus_create()
                └─> of_platform_device_create_pdata()
                    └─> of_device_alloc()
                        └─> platform_device_alloc()
                        └─> of_device_add_properties()
                    └─> platform_device_add()
                        └─> device_add()
```

这个过程中最重要的两个函数是：

* `of_platform_bus_create()`：递归创建设备树节点对应的平台设备。

* `of_platform_device_create_pdata()`：将单个节点转换为 platform_device。

最终，当遍历到你的 i2c2 节点时，系统会根据节点信息(如 `reg`、`interrupts`)填充 `struct resource` 结构体，并最终调用 `platform_device_add()`。

这个过程会在 `/sys/devices/platform/` 下产生一个类似 `fe5b0000.i2c` 的目录项，表示该硬件对应的抽象平台设备已经建立。

## 2.3. I2C 控制器设备匹配驱动

完成了 I2C 控制器的设备树节点解析后，它就已经成为了一个在内核中拥有 `platform_device` 结构体，并且在 `sysfs` 中能够找到对应路径的平台设备了。

接下来就是总线的驱动匹配了。我们的驱动是提前编译进内核的，所以应该是设备进行注册，然后去匹配。而我们在前面的流程中可以注意到，触发总线匹配的核心是 `device_add`。这一步完成设备的注册和与驱动的绑定。

我们可以将这一步分成两个部分， `match` 和 `probe`。

### 2.3.1. `match`

其中会调用 `bus_probe_device()`：

```c
void bus_probe_device(struct device *dev)
{
	struct bus_type *bus = dev->bus;
	struct subsys_interface *sif;

	if (!bus)
		return;

	if (bus->p->drivers_autoprobe)
		device_initial_probe(dev);

	mutex_lock(&bus->p->mutex);
	list_for_each_entry(sif, &bus->p->interfaces, node)
		if (sif->add_dev)
			sif->add_dev(dev, sif);
	mutex_unlock(&bus->p->mutex);
}
```

然后就是这样一个调用顺序：
```c
device_initial_probe()
	__device_attach()
		bus_for_each_drc()
			__device_attach_driver()
				driver_match_device()
```

在 `driver_match_driver()` 中会触发驱动对应总线的 `match`函数，完成设备与驱动的匹配，然后就是运行 `probe` 了。

### 2.3.2. `probe`

在 `__device_attach_driver()` 中，完成了 `match` 后，会触发:

```c
driver_probe_device()
	__driver_probe_device()
		really_probe()
			call_driver_probe()
```

在 `call_driver_probe()` 中：

```c
/* struct device *dev, struct device_driver *drv */
if (dev->bus->probe)
	ret = dev->bus->probe(dev);
else if (drv->probe)
	ret = drv->probe(dev);
```

可以看到有两条路：

* 如果设备总线有对应的 `probe`，那就调用它。
* 如果没有，那就调用对应的 `drv` 的 `probe`。

而总线的 `probe` 函数比较好查阅，一般都是实现总线的基本初始化，然后找到对应的设备驱动(就是 `struct device_driver` 的子类，比如我们自己写的设备驱动 `i2c_driver`)，调用我们写的 `i2c_driver->probe`。这条路还是比较清晰的，总线先进行通用的初始化，然后调用对应总线设备驱动的特定初始化。

不过可以看到这里还有另一条路子，这条路子其实不太常见，因为它是 `dev->bus->probe` 不存在的时候，现代 Linux 的总线基本都有自己的 `probe`，这条路子更像是做兼容，可能有一些设备或者驱动并没有总线对应，所以 `probe` 直接挂在 `drv` 下而不是总线下。

其实到这里，基本上就完成了一个设备的注册流程了。我们这里假定驱动先就绪，设备后来并触发匹配注册流程。大致跟踪了一下源码了解了流程。

## 2.4. I2C 控制器驱动匹配设备

实际上 设备-总线-驱动 模型的匹配注册是可以双向的。我们看了从设备节点出发的注册流程，我们也可以从驱动出发来看看是怎么与设备节点匹配的。

### 2.4.1. 驱动源码

如何查找设备树中的设备对应的驱动源码？如果是一个已经写好的设备树，我们可以直接找到设备节点的 `compatible` 属性：

```c
i2c2: i2c@fe5b0000 {
	compatible = "rockchip,rk3568-i2c", "rockchip,rk3399-i2c";
	...
}
```

现代 Linux 大多都是通过匹配 `compatible` 的字段来实现设备与驱动的关联的。因为我们可以直接在 `drivers` 目录下查找这里的两个字段(分别找)，最后找到了：

```c
/* drivers/i2c/busses/i2c-rk3x.c */
static const strcut of_device_id rk3x_i2c_match[] = {
	...
	{
		.compatible = "rockchip,rk3399-i2c",
		.data = &rk3399_soc_data
	},
}
```

这里 `drivers/i2c/busses/i2c-rk3x.c` 就是驱动源码的所在。直接看源码的末尾，找到对应驱动结构体的定义：

```c
static struct platform_driver rk3x_i2c_driver = {
	.probe   = rk3x_i2c_probe,
	.remove  = rk3x_i2c_remove,
	.driver  = {
		.name  = "rk3x-i2c",
		.of_match_table = rk3x_i2c_match,
		.pm = &rk3x_i2c_pm_ops,
	},
};
module_platform_driver(rk3x_i2c_driver);
```

这里定义了 I2C 控制器的基本信息和初始化函数。

### 2.4.2. 向内核注册

内核中提供了平台驱动的通用注册宏 `module_platform_driver(x)`：

```c
#define module_platform_driver(__platform_driver) \
	module_driver(__platform_driver, platform_driver_register, \
			platform_driver_unregister)
```

实际的平台驱动注册为 `platform_driver_register`，它内部调用了 `__platform_driver_register`：

```c
/* drivers/base/platform.c */
int __platform_driver_register(struct platform_driver *drv,
				struct module *owner)
{
	drv->driver.owner = owner;
	drv->driver.bus = &platform_bus_type;

	return driver_register(&drv->driver);
}
```

几乎所有的总线驱动注册都是类似的，设置 `drv->driver.owner` 和 `drv->driver.bus`，然后调用 `driver_register`。

```c
/* drivers/base/driver.c */
int driver_register(struct device_driver *drv)
{
	int ret;
	struct device_driver *other;

	if (!drv->bus->p) {
		pr_err("Driver '%s' was unable to register with bus_type '%s' because the bus was not initialized.\n",
			   drv->name, drv->bus->name);
		return -EINVAL;
	}

	if ((drv->bus->probe && drv->probe) ||
	    (drv->bus->remove && drv->remove) ||
	    (drv->bus->shutdown && drv->shutdown))
		pr_warn("Driver '%s' needs updating - please use "
			"bus_type methods\n", drv->name);

	other = driver_find(drv->name, drv->bus);
	if (other) {
		pr_err("Error: Driver '%s' is already registered, "
			"aborting...\n", drv->name);
		return -EBUSY;
	}

	ret = bus_add_driver(drv);
	if (ret)
		return ret;
	ret = driver_add_groups(drv, drv->groups);
	if (ret) {
		bus_remove_driver(drv);
		return ret;
	}
	kobject_uevent(&drv->p->kobj, KOBJ_ADD);
	deferred_probe_extend_timeout();

	return ret;
}
```

核心就是将驱动给注册到总线上：`bus_add_driver` 与 `driver_add_groups`。同时会创建 `sysfs` 下的目录和项。

而在 `bus_add_drvier` 中，会触发 `driver_attach` -> `__driver_attach`。是不是长得很眼熟，因为在设备侧也有一个 `__device_attach`。

与`__device_attach`的行为非常相似，在 `__driver_attach` 中同样也是调用了 `driver_match_device` 来进行驱动与设备的匹配，然后调用 `driver_probe_device` 来进行初始化。这下是真的殊途同归了。无论从设备侧还是驱动侧都完成了同样的注册、匹配与初始化。

### 2.4.3. 控制器驱动初始化

在完成了平台设备与驱动的匹配和基本的初始化后，就会进入到我们的具体驱动中运行特定的 `probe`。

根据 [probe](#232-probe) 小结，在对应的总线 `probe` 中，会调用具体驱动的 `driver->probe`。这里面才是 I2C 控制器驱动自己的初始化。

查看 `drivers/i2c/busses/i2c-rk3x.c` 中的 `probe` 部分，可以看到内核分配好控制器的私有结构 `rk3x_i2c` 后，就是从硬件解析中获取信息，比如：

```c
	/* 匹配芯片数据 */
	i2c->soc_data = (struct rk3x_i2c_soc_data *)device_get_match_data(&pdev->dev);

	/* 获取I2C总线ID号 */
	ret = rk3x_i2c_acpi_get_bus_id(&pdev->dev, i2c);
	if (ret < 0) {
		ret = rk3x_i2c_of_get_bus_id(&pdev->dev, i2c);
		if (ret < 0)
			return ret;
	}

	i2c->adap.nr = ret;
	
	/* 解析I2C的时序参数 */
	i2c_parse_fw_timings(&pdev->dev, &i2c->t, true);
```

**匹配芯片数据**：这里，我们先不去究细节。由于内核驱动一般可以匹配多个不同的芯片，所以这里其实就是在 `probe` 时让本次驱动与芯片型号对应上，然后获取与芯片选相关的结构体，我们可以在这个源文件中看到当初的 `<compatible>` 字段还有一段：

```c
	{
		.compatible = "rockchip,rk3399-i2c",
		.data = &rk3399_soc_data
	},
```

这里这个 `rk3399_soc_data` 就是我们 rk3568 匹配到驱动后找到的兼容数据结构体。具体是如何去实现的，内核提供了一个比较复杂的机制，因为设备因为使用 ACPI/设备树时，会解析成不同的结构体，所以内核基础设施中给了一个通用的接口 `device_get_match_data` 来获取这个芯片信息。

**获取总线 ID 号**：一般一个 SoC 中不只有一个 I2C 控制器，那么自然就需要为多个控制器进行编号，这个在设备树中是能够看到的。然后会将这个编号赋给 真实总线控制器对应的数据结构 `i2c_adapter.nr` 中。  

**解析时序参数**：`i2c_parse_fw_timings` 是一个内核辅助函数，它会从系统固件中读取 I2C 总线的时序参数，并且填充到 `struct i2c_timings` 结构体中，这个结构体被保存在 I2C 控制器的私有数据中。这个函数会从 ACPI 或设备树节点中获取一些标准属性，如果没有指定的话，通过传入的 `use_default == true` 来填入一个缺省值，保证时序。

接着是 I2C 适配器的初始化。这个我认为就是 I2C 控制器在内核中的功能抽象，一个物理 I2C 控制器就会对应一个 `struct i2c_adapter`。

```c
	strscpy(i2c->adap.name, "rk3x-i2c", sizeof(i2c->adap.name));
	i2c->adap.owner = THIS_MODULE;
	i2c->adap.algo = &rk3x_i2c_algorithm;
	i2c->adap.retries = 3;
	i2c->adap.dev.of_node = pdev->dev.of_node;
	i2c->adap.algo_data = i2c;
	i2c->adap.dev.parent = &pdev->dev;
	i2c->adap.dev.fwnode = fw;
```

一些与外设驱动类似的地方我们就掠过了，我们来看一看新东西：`ioremap`。似乎这又是一个比较核心的概念，单独开一个章节吧：[CPU 与外设寄存器的交互](#5-mmiocpu-与-外设寄存器的交互)。

**中断的获取和配置**：

```c
/* 中断号获取 */
	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;
	i2c->irq = irq;
```

这里的第二个参数 `0` 是一个中断索引，表示获取该平台设备的 第 `<index>` 个中断。因为一个设备可能会有多个中断，用来处理不同的事物。

```c
/* 注册中断处理函数 */
ret = devm_request_irq(&pdev->dev, irq, rk3x_i2c_irq,
			       0, dev_name(&pdev->dev), i2c);
	if (ret < 0) {
		dev_err(&pdev->dev, "cannot request IRQ\n");
		return ret;
	}
```

**复位控制器获取**：

```c
i2c->reset = devm_reset_control_get(&pdev->dev, "i2c");
```

具体的原理我也没太搞懂，但是获取复位控制器后，驱动可以通过这个 `handle` 来软复位设备。

和时钟的配置和 适配器 `adapter` 的注册。时钟这一块我还不太了解，先不去深究了。

关于 `adapter` 的注册，这里使用的是 `i2c_add_numbered_adapter`，他会静态地指定总线编号，因为这个控制器是 rk3568 片上自带的，已经实现编号了，比如是 I2C2，所以要直接使用这个编号而不是由内核动态地分配总线编号。


---

## 2.5. 小结

看到这里可能又会犯一些迷糊。

我们在这一章讲的是 SoC 上自带的 I2C 控制器，**不是 I2C 从设备**。在内核视角中 I2C 控制器是一个平台总线设备(`platform_device`)，它对应的驱动也是一个平台驱动(`platform_driver`)。

而我们的 I2C 从设备是一个 I2C 总线设备，它在内核中对应的设备结构体是 `i2c_client`。对应的驱动也是叫做 `i2c_driver`。

而它们之间有一个很重要的结构体是 `i2c_adapter`，它是控制器中负责与从设备打交道的核心，这也是实际进行 I2C 通信的关键，在 [SMBus接口解析](#43-smbus接口解析) 中我们会进一步去解析它。

我们完成了 I2C 控制器的设备驱动的初始化，肯定还得完成从设备与驱动的初始化，其实流程与 平台设备的也很类似了，但是总是会有一些自己特有的操作。

# 3. I2C 从设备: 注册与匹配

## 3.1. 设备树到内核节点

其实我们完成了 I2C 控制器的流程解析后，再来看从设备的流程会清晰不少。还是从设备树入手，这里以我们现在最熟悉的 max6635 温度传感器说起吧，前辈已经配置好了设备树内的节点，不过似乎是有一些小问题的， max6635 支持温度超过阈值后向内核发送中断请求，这需要硬件连接，目前没有实现。

```c
max6635: hwmon@48 {
	compatible = "national,max6635";
	reg = <0x48>;
	
	//硬件需要做更正，alert和critial中断线需要处于一个中断源，
	//因为一个中断源对应着一个时钟源，而一个普通监管设备一般只有一个时钟源
	//interrupt-names = "alert", "critical";
	//interrupts = <RK_PC2 IRQ_TYPE_LEVEL_LOW>, <RK_PA5 IRQ_TYPE_LEVEL_LOW>;
	#thermal-sensor-cells = <0>;
};
```

**注意**，这个节点是挂在 I2C2 节点之下的，这是由硬件电路图决定的。设备树的节点解析步骤应该和控制器类似，不过从设备是 I2C 总线设备而不是平台总线设备，

我们已经知道了那些直接挂在平台总线的设备，如 I2C 控制器和各类总线控制器，都是在内核初始化阶段去解析 `of_node` 从而得到 `platform_device`。而在此之前内核已经将所有的设备树节点按照其结构解析成了一个 `of_node` 树。

这时候所有的平台设备都被解析，并且注册到了内核中，有了自己的 `device` 结构，**那么那些挂在总线上的各个总线设备又是什么时候被解析并注册的呢**？

答案就在各个总线控制器驱动的 `probe` 中。在 [控制器驱动初始化](#243-控制器驱动初始化) 中，我们有提到在最后会调用 `i2c_add_numbered_adapter()`，它是在为控制器分配一个静态总线序号，并且向内核注册控制器。

如果我们在设备树中给定了控制器序号，那么内部会调用 `__i2c_add_numbered_adapter()`。这个函数除了会分配 `idr` ，还会调用 `i2c_register_adapter()`。这个函数就是关键。

```c
static int i2c_register_adapter(struct i2c_adapter *adap)
{
	int res = -EINVAL;
	...
	dev_set_name(&adap->dev, "i2c-%d", adap->nr);
	adap->dev.bus = &i2c_bus_type;
	adap->dev.type = &i2c_adapter_type;
	res = device_register(&adap->dev);
	...
	/* create pre-declared device nodes */
	of_i2c_register_devices(adap);
	...
}
```

这个函数前面是在向内核注册 `adapter` 本身，而 `of_i2c_register_devices()` 就是在尝试向内核注册挂在该 I2C 控制器下的设备节点了。

```c
void of_i2c_register_devices(struct i2c_adapter *adap)
{
	struct device_node *bus, *node;
	struct i2c_client *client;

	/* Only register child devices if the adapter has a node pointer set */
	if (!adap->dev.of_node)
		return;

	dev_dbg(&adap->dev, "of_i2c: walking child nodes\n");

	bus = of_get_child_by_name(adap->dev.of_node, "i2c-bus");
	if (!bus)
		bus = of_node_get(adap->dev.of_node);

	for_each_available_child_of_node(bus, node) {
		if (of_node_test_and_set_flag(node, OF_POPULATED))
			continue;

		client = of_i2c_register_device(adap, node);
		if (IS_ERR(client)) {
			dev_err(&adap->dev,
				 "Failed to create I2C device for %pOF\n",
				 node);
			of_node_clear_flag(node, OF_POPULATED);
		}
	}

	of_node_put(bus);
}
```

这段代码应该还是比较清晰的，在 `of_i2c_register_device()` 中将控制器下的 I2C 从设备注册到内核，并创建一个对应的 `i2c_client` 结构体，这不就连上了嘛。

然后在 `device_register() -> device_add()` 中进行同样的步骤，向内核注册，并且出发对应总线的 `probe`，再触发设备驱动的 `probe`。这一套流程与平台总线是一样的，甚至调用的接口函数都是一样的。

## 3.2. 从设备 probe

设备的 `probe` 都是通过 `device_add()` 这个核心接口调用的。它本身是设备向内核注册的第二步，而完成注册后自然就是去寻找与之匹配的设备驱动咯。这一点在前文中也是反复提到了，具体可以去看 [设备与驱动的匹配](#23-i2c-控制器设备匹配驱动)。

作为驱动开发者，一般比较关心的就是实际设备的驱动部分，而其中的核心就有设备层面的 `probe`。它一般是在总线层 `probe` 之后被调用。此时内核已经完成了该设备内核结构体的创建，并且传入到 `probe` 中。我们要做的就是为设备定义私有结构体，并与从设备的内核结构体绑定，比如 I2C 的就是 `struct i2c_client`。

在 `probe` 中除了在内核中创建私有数据结构体以外，比较重要的就是设备本身的初始化了，这个随设备变化。

到这里我们基本就了解了一个总线从控制器到从设备是如何一步步在内核中初始化注册的了。

---

# 4. SMBus协议

有博客专门讲解该协议(Linux API)：[SMBus协议概述](https://zhuanlan.zhihu.com/p/14697706607)

还有硬件波形相关的：[SMBus通信波形分析](https://blog.csdn.net/zhuoruya/article/details/125924704) 

SMBus全称System Management Bus(系统管理总线)，是一种二进制串行总线。它是I2C协议的一个子集。有许多设备都是用的是这个子集。如果可以的话，使用SMBus命令完成驱动，这样I2C适配器和SMBus适配器都能够使用(纯SMBus适配器无法处理通用的I2C命令)。

SMBus不需要增加额外的引脚，它工作在主/从模式：主设备提供始终，在它发起一次传输时提供一个起始位，终止时会有一个停止位。从设备拥有一个唯一的7或10位从设备地址。这个和标准I2C协议是一致的。

D老师告诉我：
> SMBus在物理层和基础协议上可以看作是I2C的子集，但是在系统功能和可靠性上，它又是对I2C的一种增强和约束。

---

## 4.1. 历史渊源

* **SMBus 基于 I2C 发展而来**：SMBus 是由 Intel 在 1995 年基于 I2C 协议定义的。它的初衷是解决 PC 主板上低速设备(如温度、电压监控)的通信需求，提供一条标准化、低成本的系统管理通道。由于直接复用了 I2C 的物理层和基本通信机制，所以两者是同根同源。
* **在 Linux 内核中遵循 “SMBus优先” 原则**：Linux 内核对 I2C/SMBus 设备的驱动开发有明确的指导。首要原则是优先使用 SMBus 协议命令，因为对于只使用 SMBus 子集功能的 I2C 设备，这样做能保证驱动在纯 SMBus 适配器上也能工作。同时，内核也提供了良好的兼容层，如果设备确实需要使用超出 SMBus 范围的 I2C 高级特性，也可以使用最原始的 i2c_transfer 接口。

---

## 4.2. I2C VS SMBus

D老师为我总结了一个表格，将二者进行了一个对比：

| 特性维度                | I2C                                               | SMBus                                                        |
| :---------------------- | :------------------------------------------------ | :----------------------------------------------------------- |
| **电压范围**            | 较宽，甚至可高达 12V                              | 严格定义为 1.8V 至 5V                                        |
| **逻辑电平 (VIL, VIH)** | 依赖 VDD (如 VIL=0.3VDD, VIH=0.7VDD)              | 固定电平 (如 VIL=0.8V, VIH=2.1V)                             |
| **最小时钟频率**        | 无要求，甚至可以支持直流 (DC) 操作                | 有要求，不能低于 10kHz，以便维持超时检测                     |
| **最大时钟频率**        | 标准模式 100kHz, 快速模式 400kHz, 高速模式 3.4MHz | 通常为 10kHz 至 100kHz                                       |
| **总线超时**            | 无规定，设备可能无限期拉低时钟线，锁死总线        | 强制要求。如果时钟线低电平超过 35ms，设备必须复位            |
| **时钟拉伸**            | 允许，且时长无限制                                | 允许，但定义了最大时间值(限制更严)                         |
| **设备地址应答**        | 没有强制要求设备必须应答自己的地址                | 强制要求必须应答，以便让主机感知设备状态                     |
| **数据传输格式**        | 只定义如何传输数据，但数据具体格式由设备自定义    | 明确定义了多种标准数据格式(如字节、字、块传输)             |
| **错误检测**            | 无内置硬件错误检测机制                            | 可选支持包错误检查 (PEC)，使用 CRC-8 校验增加通信可靠性      |
| **分组协议**            | 不支持                                            | 支持，允许向一组设备同时发送命令，而无需多次单独发送         |
| **心跳检测**            | 不支持                                            | 支持，通过主机发送心跳包，以检测总线上的从机是否处于活动状态 |


可以发现SMBus在协议层面定义更加完整，开发者使用SMBus来编写驱动会方便一些(直接使用内核定义的API)，几乎无需在协议层做过多的操作，只需定义数据帧内部的含义即可。
而标准I2C作为一个完整的总线协议，其可扩展性自然也是强大不少，具有更多的高级功能。开发者可以基于这些高级功能完成复杂系统。
不过对于温度传感器这类简单的设备，SMBus的基本独写功能就足以支持驱动了。

---

## 4.3. SMBus接口解析

在阅读驱动源码时，一般指看到了源码中的api调用层，没有继续往下探究了，我在看`lm92.c`时，里面的各种`i2c_smbus_*`接口并没有进去看，现在来补一补，了解一下I2C总线的电气特性是如何在软件层面实现的。

---

### 4.3.1. 关于`swapped`

在这里有两个稍微特殊一点的接口：`i2c_smbus_read_word_swapped()`和`i2c_smbus_write_word_swapped()`。

```c
// include/linux/i2c.h

static inline s32
i2c_smbus_read_word_swapped(const struct i2c_client *client, u8 command)
{
	s32 value = i2c_smbus_read_word_data(client, command);

	return (value < 0) ? value : swab16(value);
}

static inline s32
i2c_smbus_write_word_swapped(const struct i2c_client *client,
			     u8 command, u16 value)
{
	return i2c_smbus_write_word_data(client, command, swab16(value));
}
```

可以看到其实本质上里面是在直接调用对应的另一个标准接口函数。只是使用`swab16()`做了一个转换。这是在干啥？

`swab16()`是内核提供的一个用于做`word`字的字节序转换的宏。说到字节序自然就是那个所谓的"大端序"和"小端序"。

> 这里的字节序都是针对多字节数据的。
> 大端序指数据存储时，高位的数据放在低位地址处。
> 小端序则是，地位的数据放在低位地址处。
> 咱们一般使用的都是小端序(小端序便于计算机读取数据)。

目前我们使用的`arm`和`x86`架构的cpu都是使用小端序。而I2C_SMBUS协议也是默认数据传输采用小端序(先传入低字节数据，再传高字节数据，这样构造出来的数据顺序也没有问题)。但是有大量传感器和设备的数据采用大端序传输。如果我们不进行调整，得到的`word`的两个字节就是反的。所以这里的两个`*_swapped`函数就是用于兼容这样的设备的。显然我们的`max6635`就是这样的设备。

---

### 4.3.2. `i2c_smbus_read_byte()`

现在来正式看看这些协议实现，首先从I2C_SMBUS读一个字节开始，就是`i2c_smbus_read_byte()`：

```c
// drivers/i2c/i2c-core-smbus.c

s32 i2c_smbus_read_byte(const struct i2c_client *client)
{
	union i2c_smbus_data data;
	int status;

	status = i2c_smbus_xfer(client->adapter, client->addr, client->flags,
				I2C_SMBUS_READ, 0,
				I2C_SMBUS_BYTE, &data);
	return (status < 0) ? status : data.byte;
}
EXPORT_SYMBOL(i2c_smbus_read_byte);
```

没啥东西，就是把`i2c_client`中的成员传入`i2c_smbus_xfer()`。成功则返回`data.byte`，失败则返回错误码。不过这里面有一个结构体应该非常关键：`client->adapter`：

```c
struct i2c_adapter {
	struct module *owner;
	unsigned int class;		  /* classes to allow probing for */
	const struct i2c_algorithm *algo; /* the algorithm to access the bus */
	void *algo_data;

	/* data fields that are valid for all devices	*/
	const struct i2c_lock_operations *lock_ops;
	struct rt_mutex bus_lock;
	struct rt_mutex mux_lock;

	int timeout;			/* in jiffies */
	int retries;
	struct device dev;		/* the adapter device */
	unsigned long locked_flags;	/* owned by the I2C core */

	...
};
```

这个结构体的官方注释写的有点绕，但是可以这么理解。咱们目前使用的设备中的I2C总线都是具备一个控制器的，然后具体的传感器设备是作为从设备挂在控制器下的，I2C要干活全靠这个控制器`adapter`。里面比较重要的是`const struct i2c_algorithm *algo;`，这个结构体中放着I2C控制器的访问函数，用来驱动正经的硬件。

不过这里先了解一下，我们先接着看代码。继续看`i2c_smbus_xfer()`函数内部：

```c
// drivers/i2c/i2c-core-smbus.c

s32 i2c_smbus_xfer(struct i2c_adapter *adapter, u16 addr,
		   unsigned short flags, char read_write,
		   u8 command, int protocol, union i2c_smbus_data *data)
{
	s32 res;

	res = __i2c_lock_bus_helper(adapter);
	if (res)
		return res;

	res = __i2c_smbus_xfer(adapter, addr, flags, read_write,
			       command, protocol, data);
	i2c_unlock_bus(adapter, I2C_LOCK_SEGMENT);

	return res;
}
EXPORT_SYMBOL(i2c_smbus_xfer);
```

这里的代码逻辑倒是好理解：
1. `__i2c_lock_bus_helper()`：获取I2C总线的锁
2. `__i2c_smbus_xfer()`：业务函数
3. `i2c_unlock_bus()`：释放I2C总线的锁

---

#### 4.3.2.1. I2C总线 锁

这里咱们进`__i2c_lock_bus_helper()`里看看：

```c
// drivers/i2c/i2c-core.h

static inline int __i2c_lock_bus_helper(struct i2c_adapter *adap)
{
	int ret = 0;

	if (i2c_in_atomic_xfer_mode()) {
		WARN(!adap->algo->master_xfer_atomic && !adap->algo->smbus_xfer_atomic,
		     "No atomic I2C transfer handler for '%s'\n", dev_name(&adap->dev));
		ret = i2c_trylock_bus(adap, I2C_LOCK_SEGMENT) ? 0 : -EAGAIN;
	} else {
		i2c_lock_bus(adap, I2C_LOCK_SEGMENT);
	}

	return ret;
}
```

```c
static inline bool i2c_in_atomic_xfer_mode(void)
{
	return system_state > SYSTEM_RUNNING &&
	       (IS_ENABLED(CONFIG_PREEMPT_COUNT) ? !preemptible() : irqs_disabled());
}
```

这里我反正是看迷糊了，有点不太知道在干啥，问了问D老师。D老师告诉我，`__i2c_lock_bus_helper()`就是一个I2c总线锁的"小帮手"，它会查询当前当前系统是否处于原子态(调用`i2c_in_atomic_xfer_mode()`)：
1. 首先通过`system_state`判断是否处于一些**非正常运行时**(大于`SYSTEM_RUNNING`的值都是不正常运行时)。
2. 然后是一个`?:`语法。看看系统是否支持抢占(`IS_ENABLED(CONFIG_PREEMPT_COUNT)`)，支持的话就看抢占计数，不为零则处于原子态，**不可睡眠**。没有配置抢占的话就看硬件中断是否关闭了，如果关了说明此时也不能睡眠。

简单来说这个函数就是判断当前系统是否可睡眠，从而选择获取锁的方式。我们再回`__i2c_lock_bus_helper()`中可以看到，如果我们的`i2c_in_atomic_xfer_mode()`返回`true`，那么意味着此时系统处于不可睡眠的状态，自然就要判断一下I2C控制器是否有原子函数，没有就告警一下。然后就用`i2c_trylock_bus()`来尝试获取I2C的锁，失败了直接返回失败，不会陷入睡眠。

如果此时是正常状态，那么就是用普通的`i2c_lock_bus()`来获取锁：

```c
static inline void
i2c_lock_bus(struct i2c_adapter *adapter, unsigned int flags)
{
	adapter->lock_ops->lock_bus(adapter, flags);
}
```

这里面的`lock_bus()`是一个函数指针。D老师告诉我，Linux内核为大部分I2C适配器提供了一个标准的、基于`rt_mutex`的锁操作实现，并作为`lock_ops`的默认值：

```c
// drivers/i2c/i2c-core-base.c

static const struct i2c_lock_operations i2c_adapter_lock_ops = {
	.lock_bus =    i2c_adapter_lock_bus,
	.trylock_bus = i2c_adapter_trylock_bus,
	.unlock_bus =  i2c_adapter_unlock_bus,
};
```

在`i2c_register_adapter()`中讲`i2c_adapter`中的指针初始化：

```c
static int i2c_register_adapter(struct i2c_adapter *adap)
{
	...
	if (!adap->lock_ops)
		adap->lock_ops = &i2c_adapter_lock_ops;
	...
}
```

这里面的具体获取锁的函数实现可以先mark，之后再来深究。

---

#### 4.3.2.2. `__i2c_smbus_xfer()`

现在回过头去看`i2c_smbus_xfer()`中的真正实现功能的函数`__i2c_smbus_xfer()`。

```c
// drivers/i2c/i2c-core-smbus.c

s32 __i2c_smbus_xfer(struct i2c_adapter *adapter, u16 addr,
		     unsigned short flags, char read_write,
		     u8 command, int protocol, union i2c_smbus_data *data);
```

函数源码非常长，就不在这里贴出来了，直接去看源码好了，这里针对里面的重要逻辑来看。

函数内部除了核心逻辑以外，还有两个值得关注的东西：
1. `__i2c_check_suspended()`查看适配器是否被挂起
2. `trace_smbus_*()`，开启`CONFIG_TRACING`时使用的内核tracepoint(跟踪点)。在调试时可以用特定的工具(比如`perf`)抓取这些trace，来实时查看总线上的消息。**并不影响I2C传输逻辑**。

然后就是函数的主逻辑。
和之前获取锁时一样，我们先用`i2c_in_atomic_xfer_mode()`判断当前系统是否可进入睡眠。不可以的话尝试使用`*_atomic()`方法，如果没有该方法则`xfer_func`指针直接为`NULL`。

然后咱们就按照逻辑执行数据传递(有规定重试次数)。

这里还有一个分支，就是如果适配器不支持原生的SMBus操作的话(`xfer_func`指针是空的)，就采用I2C来模拟SMBus总线操作：

```c
	if(xfer_func){
		...
	}
	res = i2c_smbus_xfer_emulated(adapter, addr, flags, read_write,
				      command, protocol, data);
```

结果主逻辑中又是调用了一个函数指针`xfer_func`。我们还得找找这个操作到底在哪里定义的。直到这里其实都可以认为是I2C子系统封装/抽象好的接口，根本没有与真正的I2C控制器驱动打交道。

---

#### 4.3.2.3. 追寻真正的硬件驱动函数

我们可能得找一找咱们的I2C控制器的驱动。从设备树中找吧。这里我们一步一步查，在
* rk3568-ok3568c.dts
* rk3568.dtsi
* rk356x.dtsi
中查找`i2c2`这个节点的定义，最后在`rk356x.dtsi`中找到了：

```c
	i2c2: i2c@fe5b0000 {
		compatible = "rockchip,rk3568-i2c", "rockchip,rk3399-i2c";
		reg = <0x0 0xfe5b0000 0x0 0x1000>;
		interrupts = <GIC_SPI 48 IRQ_TYPE_LEVEL_HIGH>;
		clocks = <&cru CLK_I2C2>, <&cru PCLK_I2C2>;
		clock-names = "i2c", "pclk";
		pinctrl-0 = <&i2c2m0_xfer>;
		pinctrl-names = "default";
		#address-cells = <1>;
		#size-cells = <0>;
		status = "disabled";
	};
```

可以看到`compatible`字段中有两个匹配项，在源码中查找这两个匹配项，发现`rockchio,rk3568-i2c`没有找到，但是在`drivers/i2c/busses/i2c-rk3x.c`中找到了第二个字段，那说明这个源码就是使用的驱动了。

在源码中查找`i2c_algorithm`的定义：

```c
static const struct i2c_algorithm rk3x_i2c_algorithm = {
	.master_xfer		= rk3x_i2c_xfer,
	.master_xfer_atomic	= rk3x_i2c_xfer_polling,
	.functionality		= rk3x_i2c_func,
};
```

这不就找到了我们实际使用的与硬件打交道了驱动函数了嘛！
不过观察这里的`i2c_algorithm`可以发现，它没有SMBus协议的访问函数。我们再回到`__i2c_smbus_xfer()`中可以看到，如果I2C适配器没有对应的SMBus协议驱动函数，那么就是用标准I2C协议来模拟：

```c
	if (xfer_func) {
		...
		/*
		 * Fall back to i2c_smbus_xfer_emulated if the adapter doesn't
		 * implement native support for the SMBus operation.
		 */
	}

	res = i2c_smbus_xfer_emulated(adapter, addr, flags, read_write,
				      command, protocol, data);
```

所以实际上我们得去看这个`i2c_smbus_xfer_emulated()`：

---

#### 4.3.2.4. `i2c_smbus_xfer_emulated()`

```c
/*
 * Simulate a SMBus command using the I2C protocol.
 * No checking of parameters is done!
 */
static s32 i2c_smbus_xfer_emulated(struct i2c_adapter *adapter, u16 addr,
				   unsigned short flags,
				   char read_write, u8 command, int size,
				   union i2c_smbus_data *data)
{
	{
	/*
	 * So we need to generate a series of msgs. In the case of writing, we
	 * need to use only one message; when reading, we need two. We
	 * initialize most things with sane defaults, to keep the code below
	 * somewhat simpler.
	 */
	 ...
}
```

这个函数简单理解就是SMBus的I2C软实现。这里面就是如何用标准I2C协议来拼一个高级协议SMBus。因为SMBus的高级功能在底层需要进行分解实现，所以有这么一个玩意儿。

注释告诉我们，通过一系列I2C消息的组合就可以模拟SMBus协议：
1. 如果是SMBus写，那么只需要一个消息
2. 如果是SMBus读，则需要两个消息
为了简化，模拟时尽量用最简单的默认值来进行初始化了。

我们在源码中可以关注的有：

```c
	unsigned char msgbuf0[I2C_SMBUS_BLOCK_MAX+3];
	unsigned char msgbuf1[I2C_SMBUS_BLOCK_MAX+2];
```
这是放置消息内容的缓冲buf，如果是读去消息，那么I2C从设备传输回来的值会放进buf；如果是往从设备写，那么就将要写的值放进buf。

```c
	struct i2c_msg msg[2] = {
		{
			.addr = addr,
			.flags = flags,
			.len = 1,
			.buf = msgbuf0,
		}, {
			.addr = addr,
			.flags = flags | I2C_M_RD,
			.len = 0,
			.buf = msgbuf1,
		},
	};
```
这个是用来模拟SMBus的一个消息队列。在后面的就会根据`size`来确定这个消息队列到底该怎么使用。
> 在最开始是将`protocol`传入的，也就是`I2C_SMBUS_BYTE`这些，而在注释中午我们可以发现这些不同的协议内容其实到模拟中就是消息队列的内部有所不同，特别的读/写的消息队列分别是1和2，所以这里用`size`表示

可以看到代码中的很大一块都是`switch (size) {}`，也就是根据情况初始化我们的消息队列`struct i2c_msg msg[2]`。这里我们可以看看我们最关注的`I2C_SMBUS_BYTE_DATA`：

```c
	case I2C_SMBUS_BYTE_DATA:
		if (read_write == I2C_SMBUS_READ)
			msg[1].len = 1;
		else {
			msg[0].len = 2;
			msgbuf0[1] = data->byte;
		}
		break;
```

`I2C_SMBUS_BYTE_DATA`就是需要进行一个字节的读写。当是读的时候，设置消息队列`msg[1]`的长度为1(初始化时`msg[1].len = 0`，相当于没有数据)，这也符合读时消息队列为2.
当是写的时候，设置`msg[0].len = 2`，然后将`data->byte`中的数据放到缓冲中。
这里需要注意的是，我们的缓冲`msgbuf0[0] = command`，被**初始化为了从设备的寄存器地址的**。这是标准`I2C`的协议要求，所以实际上缓冲中包含了两个字节的内容：

```c
	msgbuf0[0] = command;
	msgbuf0[1] = data->byte;
```

完成了根据`size`的`switch-case`之后，会有一个报错误校验的环节(如果有的话)：

```c
	bool wants_pec = ((flags & I2C_CLIENT_PEC) && size != I2C_SMBUS_QUICK
			  && size != I2C_SMBUS_I2C_BLOCK_DATA);

	if (wants_pec) {
		...
	}
```

大概就是如果开启了这个校验，在每一次传输后都会跟一个校验码。咱们就先不管这个。直接看后面，接着就是正式的I2C传输了：

```c
	status = __i2c_transfer(adapter, msg, nmsgs);
```

---

#### 4.3.2.5. `__i2c_transfer()`

这里没有放完整的代码，只放了一些代码片段。

```c
int __i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	if (!adap->algo->master_xfer) {
		dev_dbg(&adap->dev, "I2C level transfers not supported\n");
		return -EOPNOTSUPP;
	}

	if (WARN_ON(!msgs || num < 1))
		return -EINVAL;

	ret = __i2c_check_suspended(adap);
	if (ret)
		return ret;

	if (adap->quirks && i2c_check_for_quirks(adap, msgs, num))
		return -EOPNOTSUPP;

	if (static_branch_unlikely(&i2c_trace_msg_key)) {}

	/* Retry automatically on arbitration loss */
	orig_jiffies = jiffies;
	for (ret = 0, try = 0; try <= adap->retries; try++) {
		if (i2c_in_atomic_xfer_mode() && adap->algo->master_xfer_atomic)
			ret = adap->algo->master_xfer_atomic(adap, msgs, num);
		else
			ret = adap->algo->master_xfer(adap, msgs, num);

		if (ret != -EAGAIN)
			break;
		if (time_after(jiffies, orig_jiffies + adap->timeout))
			break;
	}
```

在这个函数内部可以结构和`__i2c_smbus_xfer()`很像，它的核心就是那一段通过`jiffies`来重试和超时的部分。其余就是：
* 判断I2C适配器是否有可用的I2C协议操作(就是那个定义的`adap->algo->master_xfer`)
* 判断消息队列是否合法
* 判断I2C控制器有没有挂死
* 检查当前I2C设备是否支持这种传输`i2c_check_for_quirks()`(似乎是和硬件有关)
* 还有用于调试的`tracepoint`

最后就是尝试调用`adap->algo->master_xfer()`了，那么我们就又回到了实际的I2C总线驱动了，这个就是我们之前通过设备树的`compatible`字段找到的`i2c-core-smbus.c`

---

#### 4.3.2.6. 再次回到硬件驱动

其实也是可预见的，当我们发现没有对应的SMBus的驱动函数，要用软件模拟的时候，最后肯定就是用`i2c_algorithm`结构体中的标准I2C协议驱动来模拟咯，只是外面套了一层实现SMBus协议的壳子。现在我们就来看看操作函数指针`master_xfer)_`对应的`rk3x_i2c_xfer`吧。

```c
static int rk3x_i2c_xfer(struct i2c_adapter *adap,
			 struct i2c_msg *msgs, int num)
{
	return rk3x_i2c_xfer_common(adap, msgs, num, false);
}
```

这是一个非原子操作版的I2C传输函数，给`rk3x_i2c_xfer_common`传递的`polling`字段为`false`(如果是源自操作就是`true`)。

`rk3x_i2c_xfer_common()`代码比较长，这里就不贴了，只讲我感兴趣的。

首先就是一个

```c
	struct rk3x_i2c *i2c = (struct rk3x_i2c *)adap->algo_data;
```

给我干的有点懵，仔细看看其实是将`adap->algo_data`这个指针类型转换为`struct rk3x_i2c *`，至于为啥这么干，其实就是为了基于这个I2C适配器获取我们硬件驱动的私有数据。在`i2c-rk3x.c`驱动的`probe`中可以看到：

```c
	struct rk3x_i2c *i2c;
	i2c = devm_kzalloc(&pdev->dev, sizeof(struct rk3x_i2c), GFP_KERNEL);
	i2c->adap.algo_data = i2c;
```

这个`i2c`就是硬件I2C驱动的私有结构体(用于表示I2C控制器的相关信息)，他与I2C适配器的结构体(表示一条I2C总线)可以相互找到。

这里就是利用`adapter`找到`i2c`的过程。因为实际传输时需要操作I2C控制器，需要使用到`i2c`这个结构体。

在传输消息前会用`spin_lock_irqsave()`来获取自旋锁，关闭终端并且保存现场状态，放置死锁。然后`clk_enable()`使能I2C时钟。

完成这些操作后就是处理和发送我们的消息，它被包在一个`for`循环中：
```c
	for (i = 0; i < num ; i += ret) {
		
	}
```

看起来是rk的I2C驱动会对消息做一些额外的处理，这里是说一次性可以处理消息队列里的多条信息：

```c
	/*
	 * Process msgs. We can handle more than one message at once (see
	 * rk3x_i2c_setup()).
	 */

	ret = rk3x_i2c_setup(i2c, msgs + i, num - i);
	if (ret < 0) {
		dev_err(i2c->dev, "rk3x_i2c_setup() failed\n");
		break;
	}
```
这个`setup`的作用就是将要进行的操作翻译给I2C控制器硬件。然后在整个驱动中就是通过软件进行中断、时钟、锁的使能等配置，然后将数据给到I2C控制器，控制器来负责将这些数据转换成符合I2C电气特性的信号发送和接收。

如果感兴趣的话也可以继续往`rk3x_i2c_setup()`中看，以及看`rk3x_i2c_xfer_common()`本身。

不过到这里我觉得已经差不多了。

---

# 5. MMIO：CPU 与 外设寄存器的交互

## 5.1. 回顾一下

### 5.1.1. 啥是 MMIO

这里我们需要先回顾一下一个完整的计算机的物理地址与虚拟地址空间。还记得设备树中的设备节点结构吗：

```c
	<dev>@<addr>
```

其中我们的 I2C2 控制器这一个 SoC 自带的外设节点是这样的：

```c
	i2c2: i2c@fe5b0000 {
		compatible = "rockchip,rk3568-i2c", "rockchip,rk3399-i2c";
		reg = <0x0 0xfe5b0000 0x0 0x1000>;
		interrupts = <GIC_SPI 48 IRQ_TYPE_LEVEL_HIGH>;
		clocks = <&cru CLK_I2C2>, <&cru PCLK_I2C2>;
		clock-names = "i2c", "pclk";
		pinctrl-0 = <&i2c2m0_xfer>;
		pinctrl-names = "default";
		#address-cells = <1>;
		#size-cells = <0>;
		status = "disabled";
	};
```

这里的这个 `fe5b0000` 其实是 SoC 的**物理总线地址**，这是 SoC 的芯片手册中规定的(当然还有挂在总线下的其他设备，这些设备没有直接挂在 SoC 上，CPU 访问时也是走总线控制器访问，它们的节点地址表示的是在对应总线上的地址，但是这也是真实的地址)。

对 SoC 来说，本身在封装时就有很多设备连接在核心上，核心想要访问这些设备时实际上就是使用这个**物理总线地址**。当然这是最底层的，架构指令集在访问设备时的操作。

但是内核中不是这么简单地使用这些地址的，它在开启页表后使用的都是虚拟地址，这个虚拟地址空间对于 32 位机有 4GB，而对于 64 位机就很大了。虚拟地址会通过 mmu 进行翻译，进而操作对应的硬件，比如 RAM 或者实际的外设。

而对于这些平台总线设备，它们的地址就是 SoC 的物理总线地址，同样也会被翻译成虚拟地址，不过会被单独放在一个区域内，就叫做 MMIO。通过将硬件的地址映射到虚拟地址空间中(统一的地址空间，被内核都视作内存地址)，CPU可以直接像访问普通内存一样使用 load/store 与这些映射过来的硬件进行交互。

x86 体系下似乎有一个专门的 I/O 空间，它们是一个独立的地址空间，与虚拟地址空间独立。它们拥有单独的访问指令。而在 arm 架构下似乎都被统一了，使用 MMIO 统一成内存访问了。

讲了这么多其实就是想说，对于这些片上外设(平台总线设备)，它们都会通过 MMIO 将地址映射，与内存统一在一个地址空间下！其实现代设备基本都是用这一套思路了。

### 5.1.2. 为什么会使用 MMIO

我自己其实有一点迷糊，想要理清一下思路。最早的计算机可以看作就是一个 CPU + 内存，数据不断地从内存中存取，由 CPU 进行计算。所以最早的所谓地址，就指的是内存地址。CPU 从内存的一个具体地址处取出/存放数据。

但是随着计算机发展，会有外围设备接入计算机，计算机 CPU 想要访问这些外设，肯定得需要一个统一的方式，总不能一个设备一套指令吧。大致的雏形就是访问和读写外设的寄存器。那么这个寄存器要访问肯定也需要有地址吧。

x86 架构下对于 I/O 设备有一套单独的地址空间 + 操作指令，这样就将内存地址空间与设备地址空间给分开了，自己有自己的玩法。

而现代 arm 架构下也不知是为了什么(可能为了指令集精简或者别的什么原因)，使用了一个统一的地址空间。通过将外设的地址映射到内存空间中，像操作普通内存一样直接直接操作外设的寄存器。

这里我想要强调的是：这个空间本身是内存，也就是 RAM 独有的！虽然也经过了虚拟地址的中转，但是本质上一个内存地址对应的就是 RAM 的一个内存区域。

而通过 MMIO 统一的外设地址，虽然也是在虚拟地址空间下有了自己的虚拟地址，但是这个虚拟地址指向的是设备的寄存器，核心访问的也是寄存器。

## 5.2. 小结

如果没有太看明白就直接看结论就好。MMIO 就是现代计算机的一种外设访问方式。传统的外设有自己的一套方式：I/O 空间和对应的操作指令(架构层面的)。现代计算机基本摈弃了这一套，而是将外设的寄存器地址空间直接映射到内存地址空间中(反正内存自己也用不完这个地址， 64位地址呢！)，并且可以直接通过访问内存的指令来访问外设，显然这也会简化编译(原来是两套，现在合并到一起了)。

因此在高层视角下，所有的设备都可以在一个地址空间中寻址，并且都使用页表和 MMU 进行寻址。MMIO 区域稍显特殊，上层还是直接用访问内存的方式操作，但是底层则会进行翻译(似乎会直接涉及到硬件)。