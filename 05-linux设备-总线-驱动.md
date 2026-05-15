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

驱动的核心肯定是驱动的结构体`struct device_driver`，然后就是对应总线的子类，比如I2C的驱动`struct i2c_driver`。

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

---

# 设备驱动实例

## I2C设备-温度芯片

I2C总线设备算是驱动中较为简单的一类，从他入手。前辈本意是去学习SoC自带的eeprom驱动，可惜的是rk3568似乎不带eeprom，只好增加难度，去查看温度芯片(这个总有了吧)。

### 入手，查询芯片信息

如何找到温度芯片的信息呢？可以从设备树入手。当前项目使用的设备树为`rk3568-ok3568c.dts`，在其中查看`I2C`节点下的设备。芯片一般会有多个I2C控制器，我们在i2c2中找到了对应的芯片:

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

根据前辈的描述，这类温度芯片虽然是走的I2C总线，但是内核中对它们进行了抽象，而不是像其他I2C驱动一样，直接通过I2C来独写寄存器。(具体原因可能是温度芯片的寄存器交互逻辑过于复杂，涉及到数据转换和字符拼接，直接交互不太友好)。

先mark一下，等读到源码再去仔细探究。

---

### 驱动源码概览

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

其中的核心就是这个`struct i2c_driver`，这个是内核I2C子系统中管理I2C驱动的核心结构体：

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

传统的`probe()`中，会多传入一个`const struct i2c_device_id **id*`参数，而在现代Linux的I2C驱动(使用设备树或ACPI枚举)根本不会使用这个参数。这是6.1内核的一个过渡，后续新版本内核最终会保留一个接口。

---

### `lm92_probe()`

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

一来就遇到了问题，这里使用`devm_kzalloc()`为`lm92_data`结构体申请了内存。不过具体的细节我没太看明白。不过看这里的结构，`i2c_client`是I2C子系统中定义的客户端设备结构体，类似`platform_device`一样，在里面包含了`struct device`基类。

这个结构体是内核子系统的标准入口，但是每个驱动需要维护自己的私有数据，比如这里会有一个`struct lm92_data`，它包含了`i2c_client`。`i2c_client`结构体的初始化应该是交给内核来做，然后调用驱动的`probe`，再自己的代码中完成`lm92_data`的初始化。

`devm_kzalloc()`为`lm92_data`分配内存，并将它与`i2c_client->dev`的生命周期进行绑定。

在`probe()`中还有另一个`struct device`结构体：`hwmon_dev`。该结构体由`devm_hwmon_device_register_with_groups()`分配内存，并且其`parent`指向`i2c_client->dev`。然后在函数内部(很深的地方)，由`dev_set_drvdata`让`hwmon_dev->driver_data`指向了`lm92_data`。

这里关系可能有一点绕，但是我们只需知道这一条路径：

```
hwmon_dev->driver_data:lm92_data->client:i2c_client->dev
```

从设计上来讲，一般只需要从设备结构体找到私有数据，然后找到I2C的客户端就好。

---

### 两个`struct device`的`sysfs`分层解耦

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

在了解设计目的后，我又有了新的困惑。我一直对`struct device`结构体的理解是，系统在解析设备树(嵌入式ARM)或者ACPI枚举(x86和ARM)时实时创建的(不过这个想法明显有问题，只是一直没有深究)，在这里却在`probe()`中新创建了`hwmon`子系统的设备结构体。另外就是这里为了解耦将`I2C`的底层实现与传感器的高层功能分开了，那我的驱动该怎么办呢？

让D老师来为我们一一解惑。

这两个`struct device`是谁创建的？`i2c_client->dev`毫无疑问是在解析设备树时发现了有这个节点，生成并注册的。然后到驱动匹配时触发了咱们驱动代码中的`probe()`，里面调用了`devm_hwmon_device_register_with_groups()`动态创建了`hwmon_dev`。它本质上就是一个软件功能抽象，可以认为是一个虚拟设备，由hwmon子系统进行管理(这里的`hwmon_dev`是`struct device`，hwmon子系统自己的结构体是`struct hwmon_device`，这个需要在注册函数内部去查看，结构与其他子系统类似，内部有一个`struct device`结构体指向`hwmon_dev`)。

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

(这里我们还没有注意到这个结构体里的具体内容是什么，等一下和hwmon一起说)



所以到底是如何解耦的呢...在LM92的驱动内容中可以看到，功能函数中最后仍然调用的是I2C子系统提供的接口。

D老师告诉我，其实hwmon子系统并没有提供具体的接口函数，它只提供了一套数据结构，和注册接口。比如我们在`probe()`中调用的`devm_hwmon_device_register_with_groups()`就是hwmon子系统定义的。它可以为硬件设备模型抽象一个功能层(`hwmon_dev`)。**在我看来就是I2C子系统暴露在`sysfs`中的数据不是人看的，通过hwmon将寄存器值转换成人看得懂的属性暴露在`sysfs`中。这个就是hwmon子系统最大的功能。**

在驱动代码中我们可以看到，hwmon子系统的用处就是：

* `probe`阶段初始化一个`hwmon_dev`，与`i2c_client->dev`绑定(`hwmon_dev->parent`指向`i2c_client->dev`)

* 提供宏创建`hwmon`设备的属性(`lm92_attrs`，暴露在`sysfs`中)，并绑定属性修改函数(`show`，`store`)。

第一个部分源码就在`probe`中，感兴趣的可以去看看。

第二个部分主要是：

```c
/* hwmon子系统提供的宏,创建属性并绑定独写属性的函数 */
static SENSOR_DEVICE_ATTR_RO(temp1_input, temp, t_input);
static SENSOR_DEVICE_ATTR_RW(temp1_crit, temp, t_crit);
static SENSOR_DEVICE_ATTR_RW(temp1_crit_hyst, temp_hyst, t_crit);
static SENSOR_DEVICE_ATTR_RW(temp1_min, temp, t_min);
static DEVICE_ATTR_RO(temp1_min_hyst);
static SENSOR_DEVICE_ATTR_RW(temp1_max, temp, t_max);
static SENSOR_DEVICE_ATTR_RO(temp1_max_hyst, temp_hyst, t_max);
static DEVICE_ATTR_RO(alarms);
static SENSOR_DEVICE_ATTR_RO(temp1_crit_alarm, alarm, 2);
static SENSOR_DEVICE_ATTR_RO(temp1_min_alarm, alarm, 0);
static SENSOR_DEVICE_ATTR_RO(temp1_max_alarm, alarm, 1);

/* 将上述注册的属性放入结构体，probe时注册到子系统中 */
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

宏定义在`include/linux/hwmon-sysfs.h`中。可以看到hwmon子系统只是提供了抽象层的框架：`device`结构体的创建与绑定，`attr`属性创建与绑定。**但是这些属性具体有哪些，如何去独写修改属性都是由驱动自己去实现的，只是命名规范需要遵循hwmon子系统**。这个很好去看，去查看两个核心的`attr`结构体定义：`struct sensor_device_attribute()`(hwmon子系统定义)与`struct device_attribute`(这个是sysfs的标准定义)。

这样就联系起来了。用户肯定是希望在`sysfs`中看到的是一个直观的数据，而不是一个二进制寄存器值。所以Linux内核设计了hwmon子系统来在内部进行转换。这个框架很灵活，硬件设备要在`sysfs`中暴露哪些属性自己决定。并且hwmon也不规定总线类型，不管你是I2C还是SPI，都无所谓。因为与硬件交互的代码由驱动自己写。比如在`temp_show()`中，如果芯片是I2C，那就调用I2C子系统的接口；如果是SPI，那就调用SPI子系统的接口。

而I2C子系统会提供与硬件的通信接口，这个我们在代码中可以看到，比如`i2c_smbus_write_word_swapped()`。

---

### hwmon子系统 属性注册

上面虽然给了hwmon子系统在驱动中的角色，这里我们还是来看一看代码吧。

常规的属性注册是：

```c
static DEVICE_ATTR_RO(alarms);
```

```c
/* include/linux/device.h */
#define DEVICE_ATTR_RO(_name) \
	struct device_attribute dev_attr_##_name = __ATTR_RO(_name)
```

```c
/* include/linux/sysfs.h */
#define __ATTR_RO(_name) {						\
	.attr	= { .name = __stringify(_name), .mode = 0444 },		\
	.show	= _name##_show,						\
}
```

而hwmon子系统规定了自己的属性注册：

```c
static SENSOR_DEVICE_ATTR_RO(temp1_input, temp, t_input);
```

```c
/* include/linux/hwmon-sysfs.h */
#define SENSOR_DEVICE_ATTR_RO(_name, _func, _index)		\
	SENSOR_DEVICE_ATTR(_name, 0444, _func##_show, NULL, _index)

#define SENSOR_DEVICE_ATTR(_name, _mode, _show, _store, _index)	\
struct sensor_device_attribute sensor_dev_attr_##_name		\
	= SENSOR_ATTR(_name, _mode, _show, _store, _index)

#define SENSOR_ATTR(_name, _mode, _show, _store, _index)	\
	{ .dev_attr = __ATTR(_name, _mode, _show, _store),	\
	  .index = _index }
```

```c
/* include/linux/sysfs.h */
#define __ATTR(_name, _mode, _show, _store) {				\
	.attr = {.name = __stringify(_name),				\
		 .mode = VERIFY_OCTAL_PERMISSIONS(_mode) },		\
	.show	= _show,						\
	.store	= _store,						\
}
```

hwmon在`struct device_attribute`下创建了一个子类`struct sensor_device_attribute`：

```c
struct sensor_device_attribute{
	struct device_attribute dev_attr;
	int index;
};
```

多了一个`index`，这里温度传感器的值首先会被保存到`lm92_data->temp`中，然后还要与`sysfs`中的文件对应，所有有一个索引值。

---

### `probe`后续：`hwmon_dev`的创建

这里跟一下`probe`中，hwmon子系统是如何创建`struct hwmon_dev`的。

在`lm92_probe()`中：

```c
static int lm92_probe(struct i2c_client *new_client)
{
    hwmon_dev = devm_hwmon_device_register_with_groups(&new_client->dev,
							   new_client->name,
							   data, lm92_groups);
}
```

现在来看看`devm_hwmon_device_register_with_groups()`的函数细节：

```c
struct device *
devm_hwmon_device_register_with_groups(struct device *dev, const char *name,
				       void *drvdata,
				       const struct attribute_group **groups)
{
	struct device **ptr, *hwdev;

	if (!dev)
		return ERR_PTR(-EINVAL);

	ptr = devres_alloc(devm_hwmon_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	hwdev = hwmon_device_register_with_groups(dev, name, drvdata, groups);
	if (IS_ERR(hwdev))
		goto error;

	*ptr = hwdev;
	devres_add(dev, ptr);
	return hwdev;

error:
	devres_free(ptr);
	return hwdev;
}
EXPORT_SYMBOL_GPL(devm_hwmon_device_register_with_groups);
```

第一个困惑肯定就是`devres_alloc()`这是个啥？对应的可以看到还有`devres_add()`和`devres_free()`？这一系列`devres_*()`函数是干什么用的？

```c
ptr = devres_alloc(devm_hwmon_release, sizeof(*ptr), GFP_KERNEL);

#define devres_alloc(release, size, gfp) \
	__devres_alloc_node(release, size, gfp, NUMA_NO_NODE, #release)
```

```c
void *__devres_alloc_node(dr_release_t release, size_t size, gfp_t gfp, int nid,
			  const char *name)
{
	struct devres *dr;

	dr = alloc_dr(release, size, gfp | __GFP_ZERO, nid);
	if (unlikely(!dr))
		return NULL;
	set_node_dbginfo(&dr->node, name, size);
	return dr->data;
}
EXPORT_SYMBOL_GPL(__devres_alloc_node);
```

```c
struct devres {
	struct devres_node		node;
	/*
	 * Some archs want to perform DMA into kmalloc caches
	 * and need a guaranteed alignment larger than
	 * the alignment of a 64-bit integer.
	 * Thus we use ARCH_KMALLOC_MINALIGN here and get exactly the same
	 * buffer alignment as if it was allocated by plain kmalloc().
	 */
	u8 __aligned(ARCH_KMALLOC_MINALIGN) data[];
};
```

可以看到`ptr`本身的类型是`struct device **`，其值是一个地址，该地址的值为`struct device *`，而这里利用`devres_alloc()`分配了一个`devres->data`。说人话就是让`ptr`指向了用于**资源管理的结构体内部元素**。

> `devres`是一个内核用于管理设备资源的机制，在为设备分配资源时会记录一个小账本，这个账本保存了对应资源的一些信息，包括如何释放该资源。
>
> 很直观地我们看到`devm_hwmon_device_register_with_groups`中调用`devres_alloc`时传入了释放资源的函数`devm_hwmon_release`。

然后我们可以看到如果`hwdev`创建成功，那么就会让`*ptr = hwdev`，这里实际上就是

```c
ptr = (struct devres *)dr->data
*ptr = hwdev 等价于 *(dr->data)=hwdev 
就是将创建好的hwdev纳入资源管理中
```

这里还进行了一步操作：

```c
devres_add(dev, ptr);
```

```c
void devres_add(struct device *dev, void *res)
{
	struct devres *dr = container_of(res, struct devres, data);
	unsigned long flags;

	spin_lock_irqsave(&dev->devres_lock, flags);
	add_dr(dev, &dr->node);
	spin_unlock_irqrestore(&dev->devres_lock, flags);
}
EXPORT_SYMBOL_GPL(devres_add);
```

```c
static void add_dr(struct device *dev, struct devres_node *node)
{
	devres_log(dev, node, "ADD");
	BUG_ON(!list_empty(&node->entry));
	list_add_tail(&node->entry, &dev->devres_head);
}
```

最后其实就是将`hwdev`对应的资源管理结构体加入到其父节点`i2c_client->dev->devres_head`，**根本作用就是将`hwdev`的声明周期与其父设备绑定**，当`i2c_client`被销毁时，会遍历其`devres_head`，然后销毁对应的资源。

从逻辑上也很好理解，`hwmon`子系统的设备实例本身就是依附于父节点的(这里就是温度芯片的设备树实例，I2C设备)，不可能单独存在，其生命周期也应该与之匹配。

由于`devres`先于资源结构体创建，如果资源结构体`hwdev`创建失败了我们也需要对应销毁这个`devres`，所以这里的`ptr`**才会是指针的指针`struct device **`**，确保`ptr`指向`devres`(指向的其实是内部的`data`)。这样在`hwdev`创建失败后，进入`error`分支时，才可以通过`devres_free`来清理内存。不然就会内存泄漏了。

```c
void devres_free(void *res)
{
	if (res) {
		struct devres *dr = container_of(res, struct devres, data);

		BUG_ON(!list_empty(&dr->node.entry));
		kfree(dr);
	}
}
EXPORT_SYMBOL_GPL(devres_free);
```

而`ptr`本身作为函数内部的局部变量，函数执行完毕后就从内存清理掉了。



这样`devm_hwmon_device_register_with_groups`的整体流程我们就清楚了，它主要是构建了资源管理结构`devres`将`hwmondev`的声明周期于父节点进行绑定，并且确保不会出现内存泄漏。而`hwdev`的创建则放在了`hwmon_device_register_with_groups`中。

---

现在进入`hwmon_device_register_with_groups`内部来看一看：

```c
struct device *
hwmon_device_register_with_groups(struct device *dev, const char *name,
				  void *drvdata,
				  const struct attribute_group **groups)
{
	if (!name)
		return ERR_PTR(-EINVAL);

	return __hwmon_device_register(dev, name, drvdata, NULL, groups);
}
EXPORT_SYMBOL_GPL(hwmon_device_register_with_groups);
```

在`hwmon_device_register_with_groups`中只是对`name`做了判断，设备不能没有名字！主要的逻辑放在了`__hwmon_device_register`中：

```c
static struct device *
__hwmon_device_register(struct device *dev, const char *name, void *drvdata,
			const struct hwmon_chip_info *chip,
			const struct attribute_group **groups)
{
	struct hwmon_device *hwdev;
	const char *label;
	struct device *hdev;
	struct device *tdev = dev;
	int i, err, id;

	/* Complain about invalid characters in hwmon name attribute */
	if (name && (!strlen(name) || strpbrk(name, "-* \t\n")))
		dev_warn(dev,
			 "hwmon: '%s' is not a valid name attribute, please fix\n",
			 name);

	id = ida_alloc(&hwmon_ida, GFP_KERNEL);
	if (id < 0)
		return ERR_PTR(id);

	hwdev = kzalloc(sizeof(*hwdev), GFP_KERNEL);
	if (hwdev == NULL) {
		err = -ENOMEM;
		goto ida_remove;
	}

	hdev = &hwdev->dev;

	if (chip) {
		struct attribute **attrs;
		int ngroups = 2; /* terminating NULL plus &hwdev->groups */

		if (groups)
			for (i = 0; groups[i]; i++)
				ngroups++;

		hwdev->groups = kcalloc(ngroups, sizeof(*groups), GFP_KERNEL);
		if (!hwdev->groups) {
			err = -ENOMEM;
			goto free_hwmon;
		}

		attrs = __hwmon_create_attrs(drvdata, chip);
		if (IS_ERR(attrs)) {
			err = PTR_ERR(attrs);
			goto free_hwmon;
		}

		hwdev->group.attrs = attrs;
		ngroups = 0;
		hwdev->groups[ngroups++] = &hwdev->group;

		if (groups) {
			for (i = 0; groups[i]; i++)
				hwdev->groups[ngroups++] = groups[i];
		}

		hdev->groups = hwdev->groups;
	} else {
		hdev->groups = groups;
	}

	if (dev && device_property_present(dev, "label")) {
		err = device_property_read_string(dev, "label", &label);
		if (err < 0)
			goto free_hwmon;

		hwdev->label = kstrdup(label, GFP_KERNEL);
		if (hwdev->label == NULL) {
			err = -ENOMEM;
			goto free_hwmon;
		}
	}

	hwdev->name = name;
	hdev->class = &hwmon_class;
	hdev->parent = dev;
	while (tdev && !tdev->of_node)
		tdev = tdev->parent;
	hdev->of_node = tdev ? tdev->of_node : NULL;
	hwdev->chip = chip;
	dev_set_drvdata(hdev, drvdata);
	dev_set_name(hdev, HWMON_ID_FORMAT, id);
	err = device_register(hdev);
	if (err) {
		put_device(hdev);
		goto ida_remove;
	}

	INIT_LIST_HEAD(&hwdev->tzdata);

	if (hdev->of_node && chip && chip->ops->read &&
	    chip->info[0]->type == hwmon_chip &&
	    (chip->info[0]->config[0] & HWMON_C_REGISTER_TZ)) {
		err = hwmon_thermal_register_sensors(hdev);
		if (err) {
			device_unregister(hdev);
			/*
			 * Don't worry about hwdev; hwmon_dev_release(), called
			 * from device_unregister(), will free it.
			 */
			goto ida_remove;
		}
	}

	return hdev;

free_hwmon:
	hwmon_dev_release(hdev);
ida_remove:
	ida_free(&hwmon_ida, id);
	return ERR_PTR(err);
}
```

在流程之外我对函数内部局部变量的声明周期有一些疑问，这里主要是`hdev`和`hwdev`。这里捋一下

> 函数内部通过`kzalloc()`为`struct hwmon_device *hwdev`实例在堆空间上分配了内存，该内存生命周期不归这个函数管了，需要单独使用`free`或者`unregister`来销毁。
>
> 而`struct *hdev`作为一个函数内部的局部变量，它指向的是`hwdev->dev`，也就是堆空间中结构体中的元素。函数返回时`hdev`这个指针会被销毁，但是该指针指向的地址并未销毁，能够被正常传递出去。

下面继续看代码，这里又看到了一个新东西：`ida_alloc`与`hwmon_ida`：

```c
static DEFINE_IDA(hwmon_ida);
#define DEFINE_IDA(name)	struct ida name = IDA_INIT(name)
```

```c
static inline int ida_alloc(struct ida *ida, gfp_t gfp)
{
	return ida_alloc_range(ida, 0, ~0, gfp);
}
```

从函数内容来看，就是分配了一个ID号，但是有啥用呢？D老师告诉我它是hwmon子系统的设备实例编号，具体来说就是在`sysfs`中我们看到的`hwmon_dev`对应的设备目录就会叫做`hwmon%d`。

hwmon子系统自己维护了一个位表，用来分配和记录这些编号(ida的内部逻辑，我没有深究)。`ida_alloc_range`分配了ID后，会在设备名字后添加这个编号：

```c
id = ida_alloc(&hwmon_ida, GFP_KERNEL);

dev_set_name(hdev, HWMON_ID_FORMAT, id);
```

`dev_set_name`会将hwmon标准格式名与ID拼接在一起，给到`hdev->kobj-name`中，最后在`sysfs`中看到的目录名就是这个。

```c
#define HWMON_ID_PREFIX "hwmon"
#define HWMON_ID_FORMAT HWMON_ID_PREFIX "%d"
```

由于在`hwmon_device_register_with_groups()`中定义：

```c
return __hwmon_device_register(dev, name, drvdata, NULL, groups);
```

`__hwmon_device_register`的入参`chip`为`NULL`，所以源码中直接

```c
hdev->groups = groups;
```

将`lm92_groups`赋给了`hdev`。直觉上讲也没有任何问题。



`hwdev`会从`dev`(设备树节点生成的`struct device`)中获取一个可用的`label`(如果有的话，没有就跳过了)：

```c
if (dev && device_property_present(dev, "label")) {
    err = device_property_read_string(dev, "label", &label);
    if (err < 0)
        goto free_hwmon;

    hwdev->label = kstrdup(label, GFP_KERNEL);
    if (hwdev->label == NULL) {
        err = -ENOMEM;
        goto free_hwmon;
    }
}
```

进行`hdev`和`hwdev`的初始化，将传入的指针传给它们：

```c
    hwdev->name = name;
    hdev->class = &hwmon_class;
    hdev->parent = dev;
    while (tdev && !tdev->of_node)
        tdev = tdev->parent;
    hdev->of_node = tdev ? tdev->of_node : NULL;
    hwdev->chip = chip;
    dev_set_drvdata(hdev, drvdata);
    dev_set_name(hdev, HWMON_ID_FORMAT, id);
    err = device_register(hdev);
    if (err) {
        put_device(hdev);
        goto ida_remove;
    }
```

中间唯一有一点疑问的是这个：
```c
    while (tdev && !tdev->of_node)
        tdev = tdev->parent;
    hdev->of_node = tdev ? tdev->of_node : NULL;
```

这看起来像是沿着`i2c_client->dev`向上寻找一个具有有效设备树节点的设备(非hwmon子系统这种虚拟设备)。至于为啥需要找到这个设备树节点，D老师告诉我是为了：

* 通过设备树进行电源管理（如 `runtime PM` 关联）
* 在 `/sys/class/hwmon/hwmonX/` 下提供 `of_node` 符号链接，方便用户空间定位设备树节点。
* 热管理子系统（`hwmon_thermal`）可以通过 `hdev->of_node` 查找对应的 thermal zone 配置。

反正也没太看懂，先mark吧。不过这里这个`hwmon_thermal`在接下来的代码中就有：

```c
	if (hdev->of_node && chip && chip->ops->read &&
	    chip->info[0]->type == hwmon_chip &&
	    (chip->info[0]->config[0] & HWMON_C_REGISTER_TZ)) {
		err = hwmon_thermal_register_sensors(hdev);
		if (err) {
			device_unregister(hdev);
			/*
			 * Don't worry about hwdev; hwmon_dev_release(), called
			 * from device_unregister(), will free it.
			 */
			goto ida_remove;
		}
	}
```

D老师告诉我，这个是hwmon框架与Linux热管理(Thermal)子系统的集成，它会将温度传感器注册为`thermal zone`的设备。这里的`hwmon_thermal_register_sensors()`会读取设备树中`thermal zone`的相关配置，并将传感器与其关联。这样传感器就可以作为系统热管理的温度源，用于触发CPU降频，控制风扇转速等。

因为咱们这里的hwmon子系统注册使用的方式入参中`chip`始终是`NULL`，所以这段代码不会被触发。

不过hwmon子系统是有其他的注册方式的，对于新的驱动，使用类似`devm_hwmon_device_register_with_info`来注册时就会传入`chip`信息。

---

### 驱动功能详解

网上可以查阅到MAX6635芯片的数据手册。[MAX6635数据手册](https://www.analog.com/cn/products/max6635.html)

根据手册再对应驱动可以发现，这里的驱动更多是针对lm92的，只是兼容MAX6635，因此很多6635的高级功能并没有实现。只是做了基础的功能。比如：

* 读取当前温度和阈值：`temp_show`
* 设置高温报警/低温报警阈值以及过热关断阈值：`temp_store`
* 设置迟滞(这个值决定了报警后要高于/低于阈值多少后才会消除报警)：`temp_hyst_store`
* 读取过温以及低温报警实际消除温度：`temp_hyst_show`，`temp_min_hyst_show`
* 以及读取报警标志位：`alarms_show`，`alarm_show`
* 初始化时推出关断模式(启用温度传感器)：`lm92_init_client`中设置`LM92_REG_CONFIG`的`bit0`为0。

更复杂的故障队列，配置寄存器等功能该驱动中其实都并没有实现。

到这里我们对lm92的驱动源码应该有了一个比较全面的了解。
