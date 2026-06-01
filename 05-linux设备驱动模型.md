- [1. "设备-总线-驱动模型"](#1-设备-总线-驱动模型)
  - [1.1. Linux设备模型](#11-linux设备模型)
  - [1.2. Linux驱动](#12-linux驱动)
    - [1.2.1. 驱动的完整声明周期](#121-驱动的完整声明周期)
  - [1.3. 总线](#13-总线)
    - [1.3.1. 骨架-`device`和`device_driver`如何注册](#131-骨架-device和device_driver如何注册)
    - [1.3.2. 外皮-具体的总线类型解析](#132-外皮-具体的总线类型解析)
    - [1.3.3. 理解-功能分层与抽象](#133-理解-功能分层与抽象)
  - [1.4. 驱动开发](#14-驱动开发)
    - [1.4.1. 1 明确硬件接口(确定总线)](#141-1-明确硬件接口确定总线)
    - [1.4.2. 2 明确核心子系统](#142-2-明确核心子系统)
    - [1.4.3. 3 在源码中找同类项](#143-3-在源码中找同类项)
- [2. 数据访问模型](#2-数据访问模型)
  - [2.1. 这种分类方式是否完备？](#21-这种分类方式是否完备)
  - [2.2. 有关主次设备号](#22-有关主次设备号)
  - [2.3. `sysfs`目录](#23-sysfs目录)


---

# 1. "设备-总线-驱动模型"

做嵌入式Linux底层开发，核心肯定是驱动开发。而要学习驱动，肯定是绕不开Linux的设备模型的。

Linux的"设备-总线-驱动"是紧密相连的，在驱动学习的一开始我认为对Linux的这一套设计逻辑非常有必要。

有非常多前人的智慧：

1. [深入探究Linux总线-设备-驱动模型](https://zhuanlan.zhihu.com/p/1991261805348139388)
2. [Linux总线设备驱动模型深度理解](https://blog.csdn.net/STCNXPARM/article/details/150657788)
3. [Linux总线-设备-驱动模型](https://blog.csdn.net/a8039974/article/details/146378570)
4. [一张图掌握Linux 平台设备驱动框架](https://blog.csdn.net/qq_16504163/article/details/118562670)

这些资料可以对Linux的"铁三角"有一个了解。

---

## 1.1. Linux设备模型

sysfs虚拟文件系统的核心`kobject`和`kset`

设备模型的基类`struct device`



---

## 1.2. Linux驱动

驱动的核心肯定是驱动的结构体`struct device_driver`，然后就是对应总线的子类，比如I2C的驱动`struct i2c_driver`。

去查看一个真正的驱动程序，可以发现其代码主体都是

* 驱动实现
* probe() / remove() 
* struct i2c_driver 
* module_init()

在probe中会注册驱动的具体实现，这是驱动与设备匹配之后的事情。`struct i2c_driver`则是内核管理该驱动的结构体

需要注意的是`module_init()`，资料中会告诉你一个驱动要被注册到总线上是调用的`device_register()`，特定的总线还会有自己的注册实例，比如`i2c_register_driver()`。同样也有对应的注销函数。可是资料不会告诉你驱动的实例中是在哪里调用了它。

实际上是`module_init()`把这个活给干了，在其内部会调用上述注册函数。不过内核可能提供了各种宏函数，将这个部分给包装过，阅读源码的时候注意。

**内核在加载该驱动模块后会自动去调用该模块中的`module_init()`函数来完成模块的注册，这是一个强制规定的接口。(由内核定义的)**，不同的总线类型在现在Linux中还会有自己定义的`module_*_init()`来注册。



那么驱动是如何被使用的呢？

驱动在`probe()`中会将驱动结构体注册到已有的框架中，这些框架会提供一些标准的api，我们的驱动根据需要去实现这些api，然后初始化一个`*_ops`结构体，将api内部的寄存器操作封装成内核可见的接口。

这些成熟的内核框架会在`/sys`中提供设备节点，用户可以直接访问修改`sysfs`中的文件来达到配置驱动的效果。

内核本身也可以通过我们按照标准注册的驱动结构体和标准api来操作硬件。

当然有一些特殊的设备可能不具备这种框架，我们可能会需要自己手动在`sysfs`中创建，这一块之后遇到了再详细了解。

### 1.2.1. 驱动的完整声明周期

上述描述中提到了两个用于初始化的步骤：`module_init`和`probe`。这两个肯定都有用，但是各自是干嘛的？先后顺序是怎么样的呢？这就涉及到了驱动的生命周期了。我们来看看一个驱动从最开始向内核注册，到最后从内核中卸载，要经历一些什么。

如果一个驱动被编译进内核了，那么它就不存在加载和卸载的过程，`module_init`会在内核初始化时自动执行，而`module_exit`则不会触发。所以我们这里以模块化的驱动为例：

```
模块加载（insmod 或内置启动）
  │
  ├─> module_init()
  │     └─> 注册 driver (platform_driver_register / i2c_add_driver)
  │           └─> 驱动进入“待岗”状态，等待匹配
  │
  ├─> [硬件设备出现] (设备树展开、总线扫描等)
  │     └─> 内核创建 device 并挂到对应总线上
  │           └─> 调用 bus->match()，匹配到该 driver
  │                 └─> driver->probe()
  │                       ├─> 硬件初始化（ioremap、中断注册）
  │                       ├─> 分配 cdev、注册设备号
  │                       ├─> device_create() 创建设备节点
  │                       └─> 初始化完成，设备可用
  │
  ├─> [设备被移除或驱动卸载]
  │     └─> driver->remove()
  │           ├─> 释放硬件资源
  │           ├─> device_destroy() 删除设备节点
  │           └─> cdev_del() 注销字符设备
  │
  └─> module_exit()
        └─> 注销 driver
```

这是一个真实的硬件驱动的完整生命周期。如果是一个编译进内核的驱动，那么`module_init`通过内核宏，会被放在一个特殊的位置，在内核初始化时一并运行。其余与模块一样。

但是如果我们想要实现一个虚拟硬件的驱动呢？比如我们去实现一块内存的驱动呢？我们的硬件没有对应的设备树节点，那就没有所谓的`compatible`，我们的驱动该怎么写呢？

这里其实涉及的问题是内核驱动的一个分段加载的模式。我们在上面注意到，一个标准的硬件驱动在加载时总是会先运行`module_init`，向内核进行驱动的注册，比如创建对应的`struct device_driver`。**请注意，不管后续是否有对应的硬件匹配，我们在`sysfs`下都是能够看到这个驱动的**。

内核会在初始化阶段已经热插拔事件时对硬件设备进行初始化。这个过程就会在对应的总线上查找驱动(用`match`匹配)，匹配到了就会触发`probe`，这里的`probe`有好几个，比如`bus_type`里有最外层的`probe`，然后一步步调用到我们自己写的驱动`probe`。在驱动的`probe`中才是完成初始化的后半段，与硬件绑定，向内核申请资源等等。

**标准驱动的初始化是分段的**：
* `module_init`只是让内核知道有这么个驱动。
* `probe`则是匹配设备后告诉内核我要干活了得给我准备资源。

但是如果没有硬件呢？我们要实现一个纯软件驱动呢？很直觉的就可以想到，我们不要`probe`了，将其中的内容直接移动到`module_init`中不就好了。这个想法是正确的。

> 不如说早期的内核驱动就是这样去实现的，甚至是对硬件设备的驱动也是这样，没有`probe`，一个`module_init`就搞定了。这样做简单粗暴但是问题多多：
> 
> * 内存浪费，即使没有硬件也会申请资源。
> * 硬件信息硬编码，所有的硬件信息需要直接写到内核驱动中，一旦硬件变动需要调整内核源码，非常麻烦。
> * 硬件与驱动的不同步，设备可能动态变化，也可能移除，而这种硬编码且固定的驱动无法感知。

现代Linux内核驱动几乎都使用设备树(arm)+分段式初始化来灵活地配置资源并且实现最小的改动适配多种板级硬件：

* 内存按需分配(有设备才申请资源)
* 信息动态获取(设备树/ACPI提供配置，驱动代码不用变)
* 生命周期动态管理(设备动态变化时，驱动能够正确做出反映)

这种合并式的驱动初始化，只有[**永远存在、永不变化、没有物理以来、纯虚拟**]的设备才会使用。(一般都是用于测试的驱动吧)

---

## 1.3. 总线

`bus`在计算机系统中就是指用于不同部件之间传输数据的一组公共通信线路。而在Linux内核系统中，总线是这里这个"设备-总线-驱动"模型中的桥梁，它用于管理挂载在这根抽象(当然可能也有物理总线对应)总线下的设备，同时负责为设备和驱动进行配对。

**总线类型的组织骨架是统一的**。在代码中，总线模型也有自己的基类`struct bus_type`。它就是整个组织的核心：

```c
struct bus_type {
    const char *name;           // 总线名称，如 "i2c"
    int (*match)(struct device *dev, struct device_driver *drv);
    int (*probe)(struct device *dev);
    // ... 其他函数指针
    struct subsys_private *p;   // 👈 关键指针！指向私有数据
};
```

结构体中有总线的基本信息，比如总线的名称`name`，用于匹配设备与驱动的`match`函数指针，以及设备连接时的`probe`回调。很关键的是指向私有数据的指针`p`。

`subsys_private`是真正保存具体总线上所有设备和驱动的地方：

```c
struct subsys_private {
    struct kset subsys;              // 该总线在 kobject 体系中的表示
    struct kset *devices_kset;       // 属于该总线的 device 的 kset
    struct kset *drivers_kset;       // 属于该总线的 driver 的 kset
    struct klist klist_devices;      // 👈 该总线上所有 device 的链表
    struct klist klist_drivers;      // 👈 该总线上所有 driver 的链表
    struct blocking_notifier_head bus_notifier;
    unsigned int drivers_autoprobe:1;
    struct bus_type *bus;            // 👈 回指指针！指回所属的 bus_type
};
```

`subsys_private`保存了驱动核心子系统(也就是具体的总线)的私有信息，每一个子系统都可以将私有数据存储在这个结构体中。其中`kset subsys`很重要，它就是该总线在内核`sysfs`中的表示。

来看看这些关键的指针：
* `bus_type->p`：指向该总线的`subsys_private`。
* `subsys_private->bus`：回指所属的`bus_type`。
* `subsys_private->klist_devices`：该总线上所有device的链表
* `subsys_private->klist_drivers`：该总线上所有driver的链表哦

注册总线(bus_register)时会分配并初始化结构体`subsys_private`，将总线与`kobject`基础设施关联起来，然后向内核注册该总线子系统。

---

### 1.3.1. 骨架-`device`和`device_driver`如何注册

当注册一个设备时(`device_register`)，内核会把这个设备加入到它所属总线的`subsys_private->klist_devices`链表中。同理，设备驱动注册时则会加入到`klist_drivers`链表。这个过程所有的总线都是一样的。

`struct device`中与总线关联的核心指针：
| 指针 | 含义 |
| ---- | ---- |
| `device->bus` | 指向该设备所属的`struct bus_type`，表明设备挂在哪条总线上。 |
| `device->driver` | 指向绑定到该设备的`struct device_driver` |
| `device->parent` | 指向父设备，形成物理拓扑 |
| `device->kobj` | 设备内嵌的`kobj`，是`sysfs`中`/sys/devices/...`节点的基础 |

`struct device_driver`中的核心指针：
| 指针 | 含义 |
| ---- | ---- |
| `driver->bus` | 指向该驱动所属的`struct bus_type` |
| `driver->klist_devices` | 该驱动绑定的所有device的链表 |
| `driver->p` | 指向`struct driver_private`，内涵`kobj`用于挂到sysfs |

Linux将所有的设备统一都抽象为了`struct device`结构，将所有的驱动都统一抽象为`struct device_driver`结构，他们俩作为两大模型的基类。

可以看到在这些基础结构中，设备-总线-驱动这三个模型之间都存在指针相互指向，**形成了整个模型的骨架**。

---

### 1.3.2. 外皮-具体的总线类型解析

而针对具体的总线类型，会在这些骨架上套一层皮。这里以I2C总线为例子：

> 注意，这里I2C总线管理的是I2C设备，但是控制I2C总线的控制器或者叫适配器，它虽然和I2C总线相关，但是它是挂在平台总线下的。后面会讲一讲整个Linux系统的设备拓扑结构。

```c
struct i2c_client {
    unsigned short flags;               // 标志位（读写）
    unsigned short addr;                // 7位的设备地址（低7位）
    char name[I2C_NAME_SIZE];           // 设备的名字，用来和 i2c_driver 匹配
    struct i2c_adapter *adapter;        // 👈 依附的适配器！指明属于哪条 I2C 总线
    struct device dev;                  // 👈 内嵌的通用 device 结构体！
    int irq;                            // 设备申请的中断号
    struct list_head detected;          // 已经被发现的设备链表
};
```

每一个I2C的从设备都需要用一个`struct i2c_client`结构体来描述，它对应真实的I2C物理设备。`i2c_client`结构体用于表示I2C设备的实例，存储了设备的地址、名称、适配器、中断请求等信息：

* `i2c_client->dev.bus` -> 指向`i2c_bus_type`
* `i2c_client->adapter` -> 指向该设备所连接的`i2c_adapter`
* `i2c_client->dev` -> 指向`struct device`基类的实例，它被加入`subsys_private->klist_devices`链表

```c
struct i2c_adapter {
    struct module *owner;
    unsigned int class;                 // 允许匹配的设备的类型
    const struct i2c_algorithm *algo;   // 👈 指向适配器的通信算法（怎么发数据）
    struct device dev;                  // 👈 内嵌的通用 device 结构体！
    char name[48];                      // 适配器的名字
    int nr;                             // 适配器编号（i2c-0, i2c-1, ...）
};
```

可以发现，`struct device`作为基类，更多是内核用来进行生命周期管理的工具，具体的设备功能和信息其实都是放在对应总线设备实例中的，这里就是`i2c_client`。

正如之前所说，`struct i2c_adapter`就是I2C总线适配器，即控制器。硬件上每一对I2C总线都对应一个适配器来控制它。每一个`i2c_adapter`对应一个物理上的I2C控制器，在**I2C总线驱动**的`probe`函数中动态创建：

* `i2c_adapter->dev.bus` -> 也指向`i2c_bus_type`(适配器本身也是一个物理设备)
* `i2c_adapter->algo` -> 指向`i2c_algorithm`，用于描述通信方法
* `i2c_adapter->dev` -> 指向一个`struct device`实例，也会被加入`subsys_private->klist_devices`链表

说人话就是I2C总线本身也是一个设备(一般都是平台总线设备)，当然也就需要这么一套对应的结构体和驱动。`i2c_adapter`就是I2C总线设备的实例，驱动当然就也有对应的。

```c
struct i2c_driver {
    int (*probe)(struct i2c_client *, const struct i2c_device_id *);
    int (*remove)(struct i2c_client *);
    void (*shutdown)(struct i2c_client *);
    struct device_driver driver;        // 👈 内嵌的通用 device_driver！
    const struct i2c_device_id *id_table;  // 用于匹配的设备 ID 表
    const unsigned short *address_list;    // 设备地址列表
    struct list_head clients;           // 该驱动管理的所有 client 链表
};
```

`i2c_driver`结构体用于表示I2C设备的驱动程序，里面储存了与驱动程序相关的各种函数指针，驱动结构体、标识符表等。其中的`struct device_driver`就是一个内嵌的驱动结构体基类。如果要使用设备树，需要设置`device_drive`的`of_match_table`变量，它的作用就是总线在执行`match`时会使用其中的字段与设备进行匹配：

* `i2c_driver->driver.bus` -> 指向`i2c_bus_type`
* `i2c_driver->driver` -> 内嵌的结构体，会被加入到`subsys_private->klist_drivers`中
* `i2c_driver->clients` -> 该驱动管理的所有`i2c_client`链表

D老师给了我一个指针指向关系的汇总图：
```
                      i2c_bus_type (struct bus_type)
                      name = "i2c"
                      match = i2c_device_match
                      probe  = i2c_device_probe
                            │
                            │ .p
                            ▼
              ┌─────────────────────────────┐
              │   subsys_private            │
              │   .bus ─────────────────────┼──→ 回指 i2c_bus_type
              │   .klist_devices ──────────┼──→ device 链表
              │   .klist_drivers ──────────┼──→ driver 链表
              └─────────────────────────────┘
                     │                    │
         ┌───────────┘                    └───────────┐
         ▼                                           ▼
   klist_devices                               klist_drivers
         │                                           │
         ▼                                           ▼
┌─────────────────────┐                  ┌──────────────────────┐
│  i2c_adapter        │                  │  i2c_driver          │
│  .dev.bus ───→ i2c_bus_type            │  .driver.bus ──→ i2c_bus_type
│  .algo ──→ i2c_algorithm               │  .driver ──→ 加入 driver 链表
│  .dev ──→ 加入 device 链表               │  .clients ──→ client 链表
└─────────────────────┘                  └──────────────────────┘
         │                                           │
         │  adapter 和 client 之间的                  │  driver 和 client 通过
         │  物理连接关系                               │  .driver 绑定
         ▼                                           ▼
┌─────────────────────┐                  ┌─────────────────────┐
│  i2c_client         │                  │  i2c_client         │
│  .addr = 0x50       │                  │  .addr = 0x50       │
│  .adapter ──→ adapter │                │  .dev.driver ──→ driver │
│  .dev.bus ──→ i2c_bus_type            │  .dev ──→ 加入 device 链表 │
│  .dev ──→ 加入 device 链表              │                     │
└─────────────────────┘                  └─────────────────────┘
```

根据这个关系，我们可以对照看一看初始化流程：

1. 新设备注册时，调用`register_device`(应该是)，`struct device`被加入到`subsys_private->klist_devices`链表中
2. 内核会遍历`subsys_private->klist_drivers`链表中的每一个驱动
3. 调用`bus_type->match(dev, drv)`，对I2C来说就是`i2c_device_match`
4. `i2c_device_match`内部通过`dev`和`drv`结构体反推得到`i2c_client`和`i2c_driver`(通过`i2c_verify_client()`和`to_i2c_driver()`)
5. 匹配成功后，调用`bus_type->probe` -> `i2c_device_probe` -> 最终调用 `i2c_driver->probe(i2c_client, id)`(现代的Linux对I2C设备可以不传入`id`了，指针为`probe_new`)

关于匹配，I2C总线会调用`i2c_device_match`函数判断I2C设备和I2C驱动是否匹配，如果匹配就调用`i2c_device_probe`函数，进而调用I2C驱动的`probe`函数。匹配规则依次尝试：设备树`OF`匹配、`ACPI`匹配`I2C`传统`ID`表匹配。

---

### 1.3.3. 理解-功能分层与抽象

在看源码时可以发现，I2C适配器与驱动其实都是挂在平台总线下的，`platform`是一种虚拟总线，其实就是直接与CPU/SoC引脚相连的设备(为了统一Linux内核模型而抽象的总线类型)。但是在上述流程中我们发现，`i2c_adapter->dev.bus = i2c_bus_type`，也就是说这里却把适配器的总线类型设置为了I2C。这样不会出现问题吗？(由平台总线管理，但是又用I2C)

D老师告诉我这又是一种模型的分层抽象：
* 物理世界(平台总线)：它作为平台总线设备被创建，由平台驱动管理硬件寄存器、时钟、中断。
* 逻辑世界(I2C总线)：它又作为I2C总线的"根设备"，挂载在`i2c_bus_type`下，用来承载`i2c_client`这些从设备。

要说清楚还挺麻烦的，但是我觉得核心还是**功能，硬件的分层与解耦**。Linux内核中把这些功能拆分成一个个小模块，我们的硬件可以将它们组装起来形成一个完整的功能实例，而无需针对这一个硬件设备去实现一个单独的全能的实例(这样做复用性很差，不优雅)。

所以这里也很好理解了，`i2c_adapter`本质上还是一个平台设备，所以生命周期是由平台驱动管理，功能也都在这一层。而将其注册到`i2c_bus_type`是为了在逻辑上与I2C从设备相联系(绑定与解绑)。这里D老师还花了一个关系图：

```
                  platform_bus_type
                       │
                       │  bus->p->klist_devices
                       ▼
           ┌─────────────────────────┐
           │  platform_device        │   (控制器物理设备)
           │  name = "xxx-i2c"       │
           │  dev.bus = platform_bus │
           │  dev.??? = ...         │
           └───────────┬─────────────┘
                       │ dev.parent   (由平台驱动在 probe 中设置)
                       ▼
           ┌─────────────────────────┐
           │  i2c_adapter            │   (逻辑 I2C 总线控制器)
           │  nr = 0                 │
           │  dev.bus = &i2c_bus_type│◄──── 挂在 I2C 总线下
           │  dev.parent = platform_dev │◄── 物理父设备
           │  algo = &i2c_xxx_algo   │
           └───────────┬─────────────┘
                       │ adapter->dev 被加入 i2c_bus 的 klist_devices
                       │ 同时作为 i2c_client 的父设备
          ┌────────────┼────────────┐
          ▼            ▼            ▼
  i2c_client    i2c_client    i2c_client
  addr=0x50     addr=0x68     addr=0x70
  dev.bus=i2c   dev.bus=i2c   dev.bus=i2c
  dev.parent=   dev.parent=   dev.parent=
    &adap->dev   &adap->dev   &adap->dev
```

在`sysfs`中我们也可以看到与其对应的拓扑结构：
```
/sys/devices/platform/xxx-i2c/          ← 平台设备
├── i2c-0                               ← i2c_adapter (目录)
│   ├── 0-0050                          ← i2c_client
│   ├── 0-0068
│   └── 0-0070
```

与此同时，在`sys/class/i2c/devices/`下还会有符号链接指向`i2c-0`等。所以将`i2c_adapter`加入到`i2c_bus_type`中还是为了`sysfs`中更直观地展示拓扑关系。

这里我们还注意到了在`i2c_adapter`处还有一次分层，代表I2C适配器的硬件`dev`是`i2c_adapter`的`dev.parent`。这个在`sysfs`中也有体现。也算是一个物理实体和功能实体的分离。


---

## 1.4. 驱动开发

在编写新的驱动的时候，如何知道有哪些内核现有的框架和代码宏可供使用呢？

### 1.4.1. 1 明确硬件接口(确定总线)

首先就是搞清楚硬件是如何与系统连接的，这直接决定了驱动与硬件通信的基础：

* 物理总线：设备是否挂载在I2C，SPI，USB，PCI等标准总线上？这些总线会有相应的子系统进行管理
* 虚拟总线：对于直接集成在SoC内部，不通过外部总线连接的外设，通常挂在`platform`虚拟总线上，也有对应的子系统
* 设备树：ARM架构下，嵌入式Linux总是在设备树文件中描述设备。关注`compatible`属性，这是驱动与设备匹配的关键

### 1.4.2. 2 明确核心子系统

在知道了设备是如何连接的之后，我们还要明确这个硬件是干什么的，**明确设备的功能**，这将决定驱动框架：

* 电源管理芯片，稳压器：`regulator`

* 传感器或者ADC/DAC：`IIO`(Industatrial I/O)

* 触摸屏、键盘等输入设备：`input`

* 网卡：`net`(通过`socket`接口与用户空间交互)

* 摄像头或者视频采集：`V4L2`(Video4Linux2)

* 声卡或音频Codec：`ALSA`(Advanced Linux Sound Architecture)

  等等

### 1.4.3. 3 在源码中找同类项

直接在源码中看类似的驱动实现，这是最快的也是最好的路子。

---

# 2. 数据访问模型

## 2.1. 这种分类方式是否完备？

按照总线类型对Linux的设备与驱动进行划分基本已经掌握了，并且这个分类方式是完备的，所有设备都一定挂载在一条具体的总线上的。并且`arm`架构下设备树的节点组织方式是与这个分类方式吻合的。

但是Linux内核中对设备驱动的分类方式不只这一种，同时这也意味着在总线分类的基础上，还会对不同类的设备进行不同的操作。想要理解这一块，我们还得深入。

看到这个就是因为我发现在Linux内核中有这么一个驱动`drivers/spi/spidev.c`。这个驱动很特殊，它是设备树纳入使用前的`spi`设备驱动，其操作逻辑与现代驱动的结构有些微的区别。这些倒是次要的，我关注的是驱动在注册时会有一个`register_chrdev()`，这个并不是我之前了解到 总线视角下的注册。

然后就引入了另一种分类方式：按照数据访问类型进行分类。

==注意==：**这种分类方式不完备**！

这种分类大致将设备分为了：字符设备，块设备和网络设备。它们每一个都对应了一些特殊的操作，也有内核对应的子系统。

**但是这种方式并不能将所有的设备和驱动囊括**！比如我们写的max6635温度传感器，它就不是以上任意一类。它只是一个`sysfs`下有属性文件的设备。

在我看来，这一个分类是独立于总线视角的。但是总线视角下的结构和流程是每一个设备都必须有的，因为它是基本的。而这里的字符设备，块设备所独有的结构体，比如字符设备的`struct cdev`，它与`struct device`就没有啥关系，但是对于一个具体的设备，这两者可以被同时包含在设备或驱动的私有结构体中，比如一个`struct i2c_device`中可以同时包含`device`和`cdev`。

这里需要给一个明确的说法：
==总线类型和`sysfs`是每一个设备驱动都有的，但是设备不一定是字符/块/网络设备中的一种！==

---

## 2.2. 有关主次设备号

阅读Linux内核相关书籍时，书中告诉我所有的驱动都会对应一个主设备号，然后预期关联的设备则对应一个次设备号。**但那时Linux早期的标准**，那时硬件的主要访问方式可能只有字符设备和网络设备。而它们的确都有主次设备号。(那些书都是2.x版本的内核，现在都6.x了)

但是现代内核中，有大量的子系统都使用`sysfs`属性文件来作为用户接口，它们直接暴露设备寄存器配置和状态，无需用户程序来处理二进制协议。

那么主次设备号真正的作用是什么呢？

主次设备号让`/dev`目录下的设备节点能够关联到内核中正确的驱动程序：
* 应用程序执行`open("/dev/xxx")`
* 内核根据该文件所在文件系统的`inode`，得知是一个设备文件，并提取其**设备号(`dev_t`)**
* 通过设备号在`cdev_map`(或块设备的`bdev_map`)哈希表中查找，寻找对应的`struct cdev`(或者`struct gendisk`)
* 调用该`cdev`的`file_operations->open()`

**只有需要在`/dev`下创建节点、通过`open/read/write`系统调用访问的设备，才需要设备号！**

---

## 2.3. `sysfs`目录

总而言之，设备可能有各种各样的分类方法，但是`kobject`，`struct device`，`struct device_driver`这些基本的结构体是一定有的！那么这就意味着，在`sysfs`下也会有对应的路径。

另外可以发现，从不同的视角去看，同一个设备也会在不同的路径下出现。总线视角是一个完备的分类结构，它直接与设备底层相关。而数据访问类型只涉及上层的访问方式，并不完备。但是它们都在`sysfs`下有着自己的拓扑结构：

* 物理拓扑`/sys/devices/`：它是设备在内核中的身份证。
* 功能分类`/sys/class`：按照功能(子系统)进行的分类。
* 设备号索引`/sys/dev`：这个视角下只有`block/`和`char/`两个分类。
* 兼容存在`/sys/block`：这是块设备的路径，但是只是用作兼容，现在的标准路径是`/sys/dev/block/`。

而所有有主次设备号的设备会在`/dev`下有自己的文件。