做嵌入式Linux底层开发，核心肯定是驱动开发。而要学习驱动，肯定是绕不开Linux的设备模型的。

Linux的"设备-总线-驱动"是紧密相连的，在驱动学习的一开始我认为对Linux的这一套设计逻辑非常有必要。

# "设备-总线-驱动模型"

有非常多前人的智慧：

1. [深入探究Linux总线-设备-驱动模型](https://zhuanlan.zhihu.com/p/1991261805348139388)
2. [Linux总线设备驱动模型深度理解](https://blog.csdn.net/STCNXPARM/article/details/150657788)
3. [Linux总线-设备-驱动模型](https://blog.csdn.net/a8039974/article/details/146378570)
4. [一张图掌握Linux 平台设备驱动框架](https://blog.csdn.net/qq_16504163/article/details/118562670)

这些资料可以对Linux的"铁三角"有一个了解。

---

## Linux设备模型

sysfs虚拟文件系统的核心`kobject`和`kset`

设备模型的基类`struct device`



---

## Linux驱动

驱动的核心肯定是驱动的结构体`struct device_driver`，然后就是对应总线的子类，比如i2c的驱动`struct i2c_driver`。

去查看一个真正的驱动程序，可以发现其代码主体都是

* 驱动实现
* probe() / remove() 
* struct i2c_driver 
* module_init()

在probe中会注册驱动的具体实现，这是驱动与设备匹配之后的事情。`struct i2c_driver`则是内核管理该驱动的结构体

需要注意的是`module_init()`，资料中会告诉你一个驱动要被注册到总线上是调用的`device_register()`，特定的总线还会有自己的注册实例，比如`i2c_register_driver()`。同样也有对应的注销函数。可是资料不会告诉你驱动的实例中是在哪里调用了它。

实际上是`module_init()`把这个活给干了，在其内部会调用上述注册函数。不过内核可能提供了各种宏函数，将这个部分给包装过，阅读源码的时候注意。

**内核在加载该驱动模块后会自动去调用该模块中的`module_init()`函数来完成模块的注册，这是一个强制规定的接口。(由内核定义的)**



那么驱动是如何被使用的呢？

驱动在`probe()`中会将驱动结构体注册到已有的框架中，这些框架会提供一些标准的api，我们的驱动根据需要去实现这些api，然后初始化一个`*_ops`结构体，将api内部的寄存器操作封装成内核可见的接口。

这些成熟的内核框架会在`/sys`中提供设备节点，用户可以直接访问修改`sysfs`中的文件来达到配置驱动的效果。

内核本身也可以通过我们按照标准注册的驱动结构体和标准api来操作硬件。

当然有一些特殊的设备可能不具备这种框架，我们可能会需要自己手动在`sysfs`中创建，这一块之后遇到了再详细了解。

---

## 总线

`bus`在计算机系统中就是指用于不同部件之间传输数据的一组公共通信线路。而在Linux内核系统中，总线是这里这个"设备-总线-驱动"模型中的桥梁，它用于管理挂载在这根抽象(当然可能也有物理总线对应)总线下的设备，同时负责为设备和驱动进行配对。

在代码中，总线模型也有自己的基类`struct bus_type`。



---

## 驱动开发

在编写新的驱动的时候，如何知道有哪些内核现有的框架和代码宏可供使用呢？

### 1 明确硬件接口(确定总线)

首先就是搞清楚硬件是如何与系统连接的，这直接决定了驱动与硬件通信的基础：

* 物理总线：设备是否挂载在I2C，SPI，USB，PCI等标准总线上？这些总线会有相应的子系统进行管理
* 虚拟总线：对于直接集成在SoC内部，不通过外部总线连接的外设，通常挂在`platform`虚拟总线上，也有对应的子系统
* 设备树：ARM架构下，嵌入式Linux总是在设备树文件中描述设备。关注`compatible`属性，这是驱动与设备匹配的关键

### 2 明确核心子系统

在知道了设备是如何连接的之后，我们还要明确这个硬件是干什么的，**明确设备的功能**，这将决定驱动框架：

* 电源管理芯片，稳压器：`regulator`

* 传感器或者ADC/DAC：`IIO`(Industatrial I/O)

* 触摸屏、键盘等输入设备：`input`

* 网卡：`net`(通过`socket`接口与用户空间交互)

* 摄像头或者视频采集：`V4L2`(Video4Linux2)

* 声卡或音频Codec：`ALSA`(Advanced Linux Sound Architecture)

  等等

### 3 在源码中找同类项

直接在源码中看类似的驱动实现，这是最快的也是最好的路子。



# 设备驱动实例

## i2c设备-温度芯片

i2c总线设备算是驱动中较为简单的一类，从他入手。前辈本意是去学习SoC自带的eeprom驱动，可惜的是rk3568似乎不带eeprom，只好增加难度，去查看温度芯片(这个总有了吧)。

### 入手，查询芯片信息

如何找到温度芯片的信息呢？可以从设备树入手。当前项目使用的设备树为`rk3568-ok3568c.dts`，在其中查看`i2c`节点下的设备。芯片一般会有多个i2c控制器，我们在i2c2中找到了对应的芯片:

```c
&i2c2 {
        status = "okay";
        pinctrl-names = "default";
        pinctrl-0 = <&i2c2m1_xfer>;

	max6635: hwmon@48 {
		compatible = "national,max6635";
		reg = <0x48>;
		
		//硬件需要做更正，alert和critial中断线需要处于一个中断源，
		//因为一个中断源对应着一个时钟源，而一个普通监管设备一般只有一个时钟源
		//interrupt-names = "alert", "critical";
		//interrupts = <RK_PC2 IRQ_TYPE_LEVEL_LOW>, <RK_PA5 IRQ_TYPE_LEVEL_LOW>;
		#thermal-sensor-cells = <0>;
	};
};
```

 至于如何发现这个是温度芯片的，那就说来话长。因为在之前阅读bite源码，做温度检测时，读取的`sysfs文件中就有一个叫做`hwmon`的路径。

> `hwmon`是Linux内核中用于硬件监控的子系统(Hardware Monitoring)。它提供了一个统一的框架和接口，用于读取系统硬件传感器数据：
>
> * 温度(CPU、GPU、主板、硬盘等)
> * 电压(电源、核心电压等)
> * 风扇转速(CPU、机箱风扇)
> * 电流、功耗等

根据前辈的描述，这类温度芯片虽然是走的i2c总线，但是内核中对它们进行了抽象，而不是像其他i2c驱动一样，直接通过i2c来独写寄存器。(具体原因可能是温度芯片的寄存器交互逻辑过于复杂，涉及到数据转换和字符拼接，直接交互不太友好)。

先mark一下，等读到源码再去仔细探究。

### 驱动源码

学习了设备-总线-驱动模型之后我们明白，ARM架构下驱动和设备是靠`compatible`与驱动的`id`来匹配的。所以我们直接在源码中查询`national,max6635`。

`compatible`的字段一般是`<厂商名, 芯片型号>`的格式。具体的匹配流程应该还会复杂一点，比如不同厂商的相同芯片之类的，这里先不深入讨论，总之这里我们找到了匹配上的驱动代码：

```
kernel/drivers/hwmon/lm92.c
```

直接来到代码的底部，看看都有什么：

```c
/*
 * Module and driver stuff
 */

static const struct i2c_device_id lm92_id[] = {
	{ "lm92", lm92 },
	{ "max6635", max6635 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, lm92_id);

static struct i2c_driver lm92_driver = {
	.class		= I2C_CLASS_HWMON,
	.driver = {
		.name	= "lm92",
	},
	.probe_new	= lm92_probe,
	.id_table	= lm92_id,
	.detect		= lm92_detect,
	.address_list	= normal_i2c,
};

module_i2c_driver(lm92_driver);

MODULE_AUTHOR("Jean Delvare <jdelvare@suse.de>");
MODULE_DESCRIPTION("LM92/MAX6635 driver");
MODULE_LICENSE("GPL");
```

其中的核心就是这个`struct i2c_driver`，这个是内核i2c子系统中管理i2c驱动的核心结构体：

```c
struct i2c_driver {
	unsigned int class;

	/* Standard driver model interfaces */
	int (*probe)(struct i2c_client *client, const struct i2c_device_id *id);
	void (*remove)(struct i2c_client *client);

	/* New driver model interface to aid the seamless removal of the
	 * current probe()'s, more commonly unused than used second parameter.
	 */
	int (*probe_new)(struct i2c_client *client);

	struct device_driver driver;
	const struct i2c_device_id *id_table;

    /* Device detection callback for automatic device creation */
	int (*detect)(struct i2c_client *client, struct i2c_board_info *info);
	const unsigned short *address_list;
	struct list_head clients;
    
	u32 flags;
};
```

这里我只保留了一些我关注的成员，可以注意到这里有两个`probe()`，而这个函数也是驱动初始化的核心。这里为啥会有两个呢？根据资料，这是Linux内核在API迁移过程的过渡。

传统的`probe()`中，会多传入一个`const struct i2c_device_id **id*`参数，而在现代Linux的i2c驱动(使用设备树或ACPI枚举)根本不会使用这个参数。这是6.1内核的一个过渡，后续新版本内核最终会保留一个接口。

#### `lm92_probe()`

这里`lm92.c`中使用的就是新版的`probe`：

```c
static int lm92_probe(struct i2c_client *new_client)
{
	struct device *hwmon_dev;
	struct lm92_data *data;

	data = devm_kzalloc(&new_client->dev, sizeof(struct lm92_data),
			    GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->client = new_client;
	mutex_init(&data->update_lock);

	/* Initialize the chipset */
	lm92_init_client(new_client);

	hwmon_dev = devm_hwmon_device_register_with_groups(&new_client->dev,
							   new_client->name,
							   data, lm92_groups);
	return PTR_ERR_OR_ZERO(hwmon_dev);
}
```

一来就遇到了问题，这里使用`devm_kzalloc()`为`lm92_data`结构体申请了内存。不过具体的细节我没太看明白。不过看这里的结构，`i2c_client`是i2c子系统中定义的客户端设备结构体，类似`platform_device`一样，在里面包含了`struct device`基类。

这个结构体是内核子系统的标准入口，但是每个驱动需要维护自己的私有数据，比如这里会有一个`struct lm92_data`，它包含了`i2c_client`。`i2c_client`结构体的初始化应该是交给内核来做，然后调用驱动的`probe`，再自己的代码中完成`lm92_data`的初始化。

`devm_kzalloc()`为`lm92_data`分配内存，并将它与`i2c_client->dev`的生命周期进行绑定。

在`probe()`中还有另一个`struct device`结构体：`hwmon_dev`。该结构体由`devm_hwmon_device_register_with_groups()`分配内存，并且其`parent`指向`i2c_client->dev`。然后在函数内部(很深的地方)，由`dev_set_drvdata`让`hwmon_dev->driver_data`指向了`lm92_data`。

这里关系可能有一点绕，但是我们只需知道这一条路径：

```
hwmon_dev->driver_data:lm92_data->client:i2c_client->dev
```

从设计上来讲，一般只需要从设备结构体找到私有数据，然后找到i2c的客户端就好。

#### 两个`struct device`？分层解耦？

**这样我们的max6635芯片不就有两个`struct device`吗？具体的设备又是怎么管理的呢?**

> 的确一个温度芯片会在`sysfs`中对应两个`struct device`，从而产生两个目录：
>
> * I2C目录：/sys/bus/i2c/device/*，这是由`i2c_client->dev`产生
> * hwmon目录：/sys/class/hwmon/*，这是由`hwmon_dev`产生
>
> D老师告诉我这是为了分层解耦：
>
> 1. I2C层：负责底层总线通信(探测、地址、独写时序)。
> 2. hwmon层：负责提供标准化的温度传感器接口，不关心底层总线结构。
>
> 两者(`device`结构体)通过`parent`指针关联，在`sysfs`下的表现就是两个目录下只有对应的属性，I2C目录只关心设备树/总线相关的信息。而hwmon目录则是传感器的属性。

在了解设计目的后，我又有了新的困惑。我一直对`struct device`结构体的理解是，系统在解析设备树(嵌入式ARM)或者ACPI枚举(x86和ARM)时实时创建的(不过这个想法明显有问题，只是一直没有深究)，在这里却在`probe()`中新创建了`hwmon`子系统的设备结构体。另外就是这里为了解耦将`i2c`的底层实现与传感器的高层功能分开了，那我的驱动该怎么办呢？

让D老师来为我们一一解惑。

这两个`struct device`是谁创建的？`i2c_client->dev`毫无疑问是在解析设备树时发现了有这个节点，生成并注册的。然后到驱动匹配时触发了咱们驱动代码中的`probe()`，里面调用了`devm_hwmon_device_register_with_groups()`动态创建了`hwmon_dev`。它本质上就是一个软件功能抽象，可以认为是一个虚拟设备，由hwmon子系统进行管理。

那么既然又有了一个新的`dev`，它会在`sysfs`中有自己的路径，那么它的属性是谁决定的呢？答案还是`devm_hwmon_device_register_with_groups()`可以看到入参里有一个`lm92_groups`，这个结构体定义了暴露在`sysfs`目录下的属性。此外这个函数不仅创建了`hwmon_dev`，还将其注册了，可以说是一条龙服务。

```c
static struct attribute *lm92_attrs[] = {
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_temp1_crit.dev_attr.attr,
	&sensor_dev_attr_temp1_crit_hyst.dev_attr.attr,
	&sensor_dev_attr_temp1_min.dev_attr.attr,
	&dev_attr_temp1_min_hyst.attr,
	&sensor_dev_attr_temp1_max.dev_attr.attr,
	&sensor_dev_attr_temp1_max_hyst.dev_attr.attr,
	&dev_attr_alarms.attr,
	&sensor_dev_attr_temp1_crit_alarm.dev_attr.attr,
	&sensor_dev_attr_temp1_min_alarm.dev_attr.attr,
	&sensor_dev_attr_temp1_max_alarm.dev_attr.attr,
	NULL
};
ATTRIBUTE_GROUPS(lm92);
```



到底是如何解耦的呢...在LM92的驱动内容中可以看到，功能函数中最后仍然调用的是I2C子系统提供的接口。





