# I2C总线协议

光是知道如何使用I2C设备，在驱动中调用接口显然是不够的。还需要了解I2C总线的软硬件。

有博客讲解该协议：
* [I2C总线和通信协议详解](https://zhuanlan.zhihu.com/p/678229227)。

* [一文看懂I2C通信](https://blog.csdn.net/lidashent/article/details/150726465)

这里只讲我感兴趣的，更详细的内容参考博客。

---

## 简介

I2C(Inter-Integrated Circuit)通信总线。它只有两根信号线，总线上可挂载多个设备。其用途主要在低速，多设备的通信。

I2C总线技术最早由荷兰飞利浦半导体(现在的恩智浦NXP半导体)在1982年开发。最初是为了解决电视机内部电子系统复杂布线和降低制造成本的。后来则在嵌入式电子领域得到了广泛应用。从最初的标准模式(100kHz)到快速模式(400kHz)和高速模式(3.4MHz)。

---

## 基本概念

1. I2C只有两根信号线，分别是串行数据线`SDA`与串行时钟线`SCL`。
2. I2C支持多个主设备和多个从设备连接到同一根总线上，每个设备地址唯一。地址由7位或10位构成。
3. I2C是同步通信，由时钟信号`SCL`进行控制。
4. I2C支持阻塞传输，主设备可以在传输过程中控制总线，防止其他设备发送数据。

---

## 工作原理

1. 总线控制：
   * 一次通信由主设备通过在`SDA`线上生成特定的信号模式来开始和结束。
   * 每次通信开始时，主设备发送一个地址帧来指定通信的从设备。

2. 数据传输：
   * 主设备控制时钟信号，向从设备发送或接受数据。
   * 接收方会在每一个字节后发送一个应答位`ACK`或者非应答位`NACK`来告知发送方数据是否接收到。

---

## 数据传输流程

主要是看时序图了解，直接看博客就好。这里简单讲解一下。
数据是由字节为单位进行传输。
1. **开始条件**：`SCL`在高电平时`SDA`拉低。然后在每一个时钟脉冲期间传输1位数据。
* 完成一个8位数据后主设备会释放`SDA`线。此时`SDA` 线保持高位，从设备如果完成接收，则会主动拉低`SDA`线(即`ACK`)。如果没有拉低，则表示未响应(从设备忙或错误)，主设备会处理。
* 一般一次通信由三个字节构成：从设备地址+从设备寄存器地址+数据。(不是协议规范)
* 需要注意的是一般设备地址由7位构成，那么第一个字节的数据最后会选择`R/W`，这将决定数据帧的数据流向。

2. **停止条件**：`SCL`在高电平时`SDA`拉高。

---

## 细节解析

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

# SMBus协议

有博客专门讲解该协议(Linux API)：[SMBus协议概述](https://zhuanlan.zhihu.com/p/14697706607)

还有硬件波形相关的：[SMBus通信波形分析](https://blog.csdn.net/zhuoruya/article/details/125924704) 

SMBus全称System Management Bus(系统管理总线)，是一种二进制串行总线。它是I2C协议的一个子集。有许多设备都是用的是这个子集。如果可以的话，使用SMBus命令完成驱动，这样I2C适配器和SMBus适配器都能够使用(纯SMBus适配器无法处理通用的I2C命令)。

SMBus不需要增加额外的引脚，它工作在主/从模式：主设备提供始终，在它发起一次传输时提供一个起始位，终止时会有一个停止位。从设备拥有一个唯一的7或10位从设备地址。这个和标准I2C协议是一致的。

D老师告诉我：
> SMBus在物理层和基础协议上可以看作是I2C的子集，但是在系统功能和可靠性上，它又是对I2C的一种增强和约束。

---

## 历史渊源

* **SMBus 基于 I2C 发展而来**：SMBus 是由 Intel 在 1995 年基于 I2C 协议定义的。它的初衷是解决 PC 主板上低速设备(如温度、电压监控)的通信需求，提供一条标准化、低成本的系统管理通道。由于直接复用了 I2C 的物理层和基本通信机制，所以两者是同根同源。
* **在 Linux 内核中遵循 “SMBus优先” 原则**：Linux 内核对 I2C/SMBus 设备的驱动开发有明确的指导。首要原则是优先使用 SMBus 协议命令，因为对于只使用 SMBus 子集功能的 I2C 设备，这样做能保证驱动在纯 SMBus 适配器上也能工作。同时，内核也提供了良好的兼容层，如果设备确实需要使用超出 SMBus 范围的 I2C 高级特性，也可以使用最原始的 i2c_transfer 接口。

---

## I2C VS SMBus

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

## SMBus接口解析

在阅读驱动源码时，一般指看到了源码中的api调用层，没有继续往下探究了，我在看`lm92.c`时，里面的各种`i2c_smbus_*`接口并没有进去看，现在来补一补，了解一下I2C总线的电气特性是如何在软件层面实现的。

---

### 关于`swapped`

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

### `i2c_smbus_read_byte()`

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

#### I2C总线 锁

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

#### `__i2c_smbus_xfer()`

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

#### 追寻真正的硬件驱动函数

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

#### `i2c_smbus_xfer_emulated()`

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

#### `__i2c_transfer()`

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

#### 再次回到硬件驱动

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