# Bash

yocto工程的初始化不可避免地会使用Bash脚本，因此有必要掌握Bash脚本的基本语法。

## `$`，`()`，`[]`与`{}`

美元符号`$`与各种括号在脚本中有着强大的功能，这里给一个表格

| 符号组合              | 主要用途            | 简要说明与示例                                               |
| :-------------------- | :------------------ | :----------------------------------------------------------- |
| **`$var`/ `${var}`**  | **变量引用**        | 获取变量值。花括号主要用于明确变量名的边界，例如 `echo ${file}s`可以清晰地区分变量 `file`和字母 `s`。 |
| **`$(command)`**      | **命令替换**        | 执行 `command`并将其标准输出结果替换到当前位置。例如 `current_date=$(date)`。 |
| **`$((expression))`** | **算术运算**        | 计算整数算术表达式 `expression`。例如 `result=$(( 5 + 3 ))`。 |
| **`()`**              | **子Shell中执行**   | 在**新的子Shell进程**中执行括号内的命令。括号内的变量更改不会影响父Shell。例如 `(cd /tmp && pwd)`。 |
| **`{}`**              | **当前Shell中执行** | 在**当前Shell进程**中执行括号内的命令（命令需用分号`;`隔开，左括号后需有空格）。括号内的变量更改会影响当前Shell。例如 `{ var="hello"; echo $var; }`。 |
| **`{1..10}`**         | **序列扩展**        | **不**需要`$`，直接生成一个序列。例如 `echo {1..3}`输出 `1 2 3`，`touch file{01..03}.txt`创建三个文件。 |
| **`[ condition ]`**   | **条件测试**        | `test`命令的另一种形式，用于条件判断。注意括号内的条件表达式需要遵循特定规则，例如字符串比较用 `=`，整数比较用 `-eq`。 |
| **`[[ condition ]]`** | **增强的条件测试**  | 比 `[]`更强大、更安全，支持模式匹配（`==`）和正则匹配（`=~`）。例如 `if [[ "$name" == u* ]]`。 |



# Linux/U-boot源码阅读指南

## Linux内核源码阅读

### file.list+SI

前辈提供的脚本+yocto时间戳判断哪些文件参与编译的方法似乎因为内核版本/yocto版本不同而失效了，所以要使用Source Insight去查看代码可能就需要手动去剔除不需要的源码文件，工作量有些大。

我换了一种思路，通过解析yocto `${WORKDIR}/build`目录中的`.o.cmd`文件来得到编译所需要的全部文件的列表(生成基于源码顶层目录的相对路径)，然后再拷贝一个干净的源码目录作为阅读用，在SI中通过该`file.list`只加入所需要的源码实现阅读。

1. 首先需要yocto完整构建一次`virtual/kernel`(其实只需要`bitbake -c compile`)，得到`${WORKDIR}/build`目录，然后使用制作好的脚本`make-file-list(kernel).sh`在`build`目录下运行，就能够得到内核源码编译时所需要的全部源文件/头文件的列表。
2. 将`build`目录下的`.config`和脚本生成的`kernel-file.list`一同拷贝的我们的源码副本根目录下。因为我们还需要生成一些编译中间文件，为了不污染源码，最好是单独拷贝一个副本来用作阅读。
3. (可选)为了使得`.config`中的宏生效并展开，更方便阅读，可以在源码副本中执行`make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- prepare`来生成对应的一些`generated`目录和`autoconf.h`(最终编译使用的配置宏定义)，注意`ARCH`和`CROSS_COMPILE`的选择，这里由于yocto自己的`aarch64-poky-linux-`本地是没有的，所以选了一个常用的`gnu`交叉编译器，由于不是真正的编译，所以不影响。
4. 用SI创建项目，源码目录选择源码副本(包含`kernel-file.list`)，然后我们在添加文件时选择`add from list`就好，然后就是同步和条件解析(用于解析`#ifdef`)。

这样基本实现了SI阅读源码，不过`#include`这样的头文件跳转似乎还是有点问题，不过无伤大雅，之后有机会在解决。



不过这种方式似乎存在一定问题，可能会因为内核版本的不同，`.o.cmd`的格式存在差异而导致脚本提取文件失败，比较可行的方法是将想要阅读的源码的`.o.cmd`格式发给AI，让他帮你写一个脚本来提取。

### 基于compile_commands.json+VScode clangd插件

而yocto的构建系统，由于不是使用本地编译器，并且隔离了源码和构建目录，因此即使能够使用`bitbake -c devshell`，并且生成`compile_commands.json`，也没有办法使用VScode+Clangd来实现很好地代码阅读。(主要还是yocto构建系统不兼容，都能拿到`compile_commands.json`了...)。正常情况下本地`make`会将编译结果一同放在源码目录下，然后compile_commands.json就完全是基于源码+构建二合一目录的，我们在使用`clangd`插件时，VScode也只需要打开源码目录就可以被`clangd`服务器解析并实现跳转。而yocto则采用了源码/编译产物隔离的策略，将所有的构建产物全部放在`${WORKDIR}/build`中，就连`.config`和`compile_commands.json`也一样，这样导致`compile_commands.json`中的路径：

* 源码路径是`${WORKDIR}/kernel`的绝对路径
* 构建目录是`${WORKDIR}build`的绝对路径

两者被分离了，会导致`clangd`解析出现问题，想要整改的话非常麻烦，并且还有一个严重问题就是使用`devshell`的确能够复现yocto的构建指令，但是生成的`compile_commands.json`是完全基于`${WORKDIR}`绝对路径的，我们想要用它来解析本地的源码需要做非常大的改动，非常麻烦。所以这个方案也被放弃了。



然后我换了一个思路，yocto可以制作镜像的sdk，也就是说可以在本地安装我自己定义的`image`的SDK，当然也就包含有编译工具链，那么我就可以直接在本地使用`make`来生成本地目录对应的`compile_commands.json`了！

需要注意的是，为了使得插件能够正确地识别我们的编译宏，我们需要：

```bash
make defconfig # 基于defconfig生成.config，这是编译的必要工作
make prepare # 生成generated目录和比较关键的autoconf.h，其中包含配置宏
make compile_commands -j${nproc} # 也可以根据实际情况调整 -j n
```

这样就会在源码根目录下生成`compile_commands.json`文件。

然后稍微配置一下VScode，比如在源码目录下的`.vscode/`下利用`settings.json`来编写当前文件夹的配置。另外由于`clangd`可能不能识别`gcc`的一些编译选项，所以可以在根目录下创建`.clangd`：

```bash
CompileFlags:
  Remove:
    - -mabi=*
    - -fconserve-stack
    - -fno-var-tracking-assignments
    - -fno-ipa-sra
    - -fno-ipa-cp
# 也可以根据具体的信息来忽略一些声明
```

完成配置或者修改配置后，记得`shift+ctrl+P: clangd restart language server`来重启`clangd`服务器使得配置生效。然后我们就可以正常跳转了，不过需要注意的是，如果跳转不了，很有可能是当前文件并没有参与编译。



## U-boot源码阅读

如果是比较新版本的U-boot，那么它本身也支持`make compile_commands.json`，那么操作方法就和`kernel`一致。

但是这里rk3568是一个比较老版本的，虽然也可以生成`compile_commands.json`，但是`clangd`对它兼容性非常差(相比起`kernel`)，不过我们暂时也没有大幅度改动和阅读U-boot源码的需求，所以也就暂时搁置了。

不过有了`compile_commands.json`，即使是因为`clangd`兼容不了，也可以直接使用`C/C++`插件来实现跳转，只是每一次打开都需要重新加载，速度会比较慢，不过能用了。以后更换高版本的U-boot了再说吧。

# Rockchip烧写指南

rockchip在windows端和linux端都提供自己的烧写工具。

win端的rkdevtool工具存在版本问题，经过验证v2.84版本没有问题。可以整体烧录`wic`镜像

~~而v2.96不能烧写wic镜像，分开烧写又存在一些其他问题，比如boot.img镜像在wks中以文件系统格式制作，但是得到的boot.img没有文件系统，导致分开烧写时uboot无法识别内核。(boot.img是kernel和dtb打包后的镜像)~~，v2.96存在一些问题，初始的扇区禁止烧录，很麻烦，遂弃用。



另外注意波特率，出现乱码一般是波特率的问题，uboot和内核的波特率需要分别进行修改...(前辈通过打patch包的方式来得到)，注意第一次编译时需要将所有的补丁和配置文件打入，不然可能会出现各种问题。

另外就是如果需要自己手动修改，那可能需要分别修改uboot和内核的defconfig文件。具体修改方式自行查找。

## 添加uboot-env分区

为了方便调试，可以在单独划分一个区域来储存uboot的环境变量，一般将这个分区放在uboot分区后。如果采用工具烧写，可能可以直接修改一个分区表文件。

这只是在烧写过程中预留了一块地址，但是还需要让uboot知道我可以在这个地方保存环境变量。这个需要去修改uboot源码。

直接修改uboot源码目录下`configs/<your_arch>.defconfig`，在末尾添加：
```bash
##############################################
# 允许储存环境变量
CONFIG_ENV_IS_IN_MMC=y
# 储存环境变量的设备号 这里0是emmc
CONFIG_SYS_MMC_ENV_DEV=0
# 储存环境变量的分区号 根据分区表来 这里是2
CONFIG_SYS_MMC_ENV_PART=2
# 扇区大小为512字节
# 分区表中uboot-env 偏移为0x4000扇区 就是8MB偏移 0x800000
CONFIG_ENV_OFFSET=0x800000
# 大小是0x1000扇区 也就是2MB 0x200000
CONFIG_ENV_SIZE=0x200000
##############################################
```

这里是一个示例，需要注意的是，emmc一般以扇区为最小单位，一般是512字节一个扇区。而我们在烧写的时候可能是以扇区为单位的！需要换算

但是在这里我们定义uboot-env分区的偏移量和大小则是要给出其正常的地址，小小换算一下。

完成后，重新编译烧写uboot，进入uboot命令行后我们就可以使用`saveenv`命令了。

**注意**：这里的做法其实是早期的一个非常不成熟的做法，比较标准的做法还是使用`menuconfig`进入可视化菜单，通过选中相关选项来实现更改配置(因为可能U-boot的版本不同，具体的配置宏名字有差异，手写可能会出现配置不生效的情况)。



## Boot分区烧写指南

### 带文件系统的启动

rockchip的uboot引导方式似乎比较特别，在uboot命令行中可以查看`bootcmd`，然后一步步去查询其引导内核的过程。这里采用的是利用`extlinux.conf`的方式进行引导，`extlinux`的引导方式可以在yocto的`machine`配置中设置，并且给出一些基本的配置，这样生成的`extlinux`文件直接包含这些配置。如果要修改的话，最好是去修改`machine`配置，比如我这里的`rk3568-avod.conf`中有关`UBOOT_EXTLINUX`有关的项。

该种引导方式根据uboot命令行可以发现，需要为`boot.img`也就是`kernel+dtb`打包的镜像套一层文件系统，uboot通过进入该文件系统的`extlinux`目录找到`extlinux.conf`文件，读取其配置来引导内核和根文件系统。

而使用`wks`文件制作`wic`时会发现，只有最终生成的`.wic`大镜像中的`boot`分区是带文件系统的。在`deploy/images`目录下生成的中间组件镜像时，`boot.img`镜像是不带文件系统的，仅仅是内核+设备树的二进制文件打包并且加了一个`Android`头。这个头使得uboot需要使用特别的启动命令`boot_android`来启动内核。另外还需要单独配置根文件系统的启动，按理说可以在yocto中配置(暂时没搞明白)，但是在调试时可以直接在uboot命令行中修改`bootargs`来完成。

另一个解决方案是从`wic`镜像中使用`dd`命令拆出带文件系统的`boot`分区镜像：

```bash
# 使用fdisk -l 查看.wic镜像的分区信息
fdisk -l *.wic
    Disk avod-image-minimal-rk3568-avod.rootfs.wic：5.33 GiB，5721048064 字节，11173922 个扇区
    单元：扇区 / 1 * 512 = 512 字节
    扇区大小(逻辑/物理)：512 字节 / 512 字节
    I/O 大小(最小/最佳)：512 字节 / 512 字节
    磁盘标签类型：gpt
    磁盘标识符：00000000-0000-0000-0000-00004D9B9EF0

    设备                                          起点     末尾    扇区  大小 类型
    avod-image-minimal-rk3568-avod.rootfs.wic1    8192    16383    8192    4M Microsoft 基本数据
    avod-image-minimal-rk3568-avod.rootfs.wic2   16384    20479    4096    2M Linux 文件系统
    avod-image-minimal-rk3568-avod.rootfs.wic3   32768   557055  524288  256M Linux 文件系统
    avod-image-minimal-rk3568-avod.rootfs.wic4  557056  4751359 4194304    2G Linux 文件系统
    avod-image-minimal-rk3568-avod.rootfs.wic5 4751360  8945663 4194304    2G Linux 文件系统
    avod-image-minimal-rk3568-avod.rootfs.wic6 8945664  9076735  131072   64M Linux 文件系统
    avod-image-minimal-rk3568-avod.rootfs.wic7 9076736 11173887 2097152    1G Linux 文件系统

# 使用dd命令拆解出boot分区
# 这里第3个分区是boot分区，bs为扇区大小，skip为分区起始，count为分区大小
# if为原文件 of为要得到的目标文件，后缀可以命名为wic2以作区分
dd if=*.wic of=target.wic2 bs=512 skip=32768 count=524288
```

然后使用这个带文件系统的镜像来烧录。



另外还需要注意一点，如果是整体烧写`wic`镜像，在`wks`中配置好了`--active`作为启动分区。但是生成的`parameter`分区表中这一标识缺失了，因此如果选择分开烧写，使用烧写工具+分区表的话，需要在`parameter`中修改`boot`分区，添加`bootable`标签。这样uboot才可以识别该分区是启动分区。
```bash
0x00080000@0x00008000(boot:bootable)
```

因为`wic`镜像在制作时内部为`boot`分区标记了可启动，可以使用`sudo parted *.wic print`来查看：
```bash
型号： (file)
磁盘 /home/public/yocto-avod/build-minimal/tmp/deploy/images/rk3568-avod/cetca-avod-minimal-rk3568-avod.rootfs-20251128022227.wic: 5721MB
扇区大小 (逻辑/物理)：512B/512B
分区表：gpt
磁盘标志：

编号  起始点  结束点  大小    文件系统  名称       标志
 1    4194kB  8389kB  4194kB            uboot      msftdata
 2    8389kB  10.5MB  2097kB            uboot-env
 3    16.8MB  285MB   268MB   ext4      boot       旧版启动
 4    285MB   2433MB  2147MB  ext4      rootfs
 5    2433MB  4580MB  2147MB  ext4      usr
 6    4580MB  4647MB  67.1MB            resv
 7    4647MB  5721MB  1074MB  ext4      log
```

而我们拆除来的`boot`分区镜像再用相同的方法查看：

```bash
型号： (file)
磁盘 /home/public/yocto-avod/build-minimal/tmp/deploy/images/rk3568-avod/target-boot.wic4: 268MB
扇区大小 (逻辑/物理)：512B/512B
分区表：loop
磁盘标志：

编号  起始点  结束点  大小   文件系统  标志
 1    0.00B   268MB   268MB  ext4
```

就会发现该标志位没有了，具体原因很有可能是`wic`镜像在开头储存了分区表来对各个分区进行描述和标记，显然拆出来的镜像没有这个头。并且wks的分区定义转换到`parameter`文件中似乎会丢失一些参数，因此我们需要手动去更改一下`boot`分区，将其设置为`bootable`。



### 裸镜像启动(带`android`头)

如果想要使用裸镜像，也就是`deploy/`目录下的`boot.img`镜像，这个镜像被添加了一个`Android`头，在U-boot命令行中可以使用`boot_android`来解析并启动，不过需要修改启动参数，也就是`bootargs`。

正常来说`Android`头应该在制作时就会携带启动参数，但是可能是这里我们本意并不是生成该镜像，它只是一个中间产物，因此缺少了这些参数，我们如果想要用这个镜像作为内核来启动的话，就需要手动去设置`bootargs`：

```bash
setenv bootargs "root=/dev/mmcblk2p4 rootwait rw console=tty2,115200"
saveenv
boot_android mmc 0:3 # 注意boot分区
```

注意和我们的`extlinux`中的设置对应。

**为了避免额外的问题，最好也像烧录带文件系统的`boot`分区镜像一样，在分区表中手动标记为`bootable`。**



# 调通内核网口

## 尝试，开发板gmac0的rgmii：tx有问题

目前手里的这块板子是基于`rockchip`官方原理图，在某些芯片选型上作了修改的一块板子，因此官方的内核源码可能会在某些情况下不适配。(也是回答了一点疑惑，我一开始以为是原装的官方板)。另外，之后在实际板子上使用的可能是`1000base-t1`网口，硬件上又存在很大区别，因此这里只是作为一个练手，帮助我稍微了解一点网络驱动相关的知识。

先去了解一下`OSI`7层模型，大致明白了开发板上的`PHY`芯片主要对应物理层，内置`GMAC`主要是数据链路层。我在调试`rk3568-evb`开发板时，进入内核后将开发板与pc用`RJ45`网线连接。

> PHY：指Physical Layer Transeiver，物理层收发器
>
> GMAC：指Gigabit Ethernet MAC，千兆网媒体访问控制器

在使用默认设备树时，网线接好后网口灯能够亮。我再为我的pc和开发板分别使用`ifconfig`或者`ip`工具配置静态`ip`地址为

```bash
192.168.1.100(主机)
192.168.1.101(开发板)
```

正常来说，这时我们直接就能够在终端相互ping通，但是失败了。



### 排查问题

首先我们需要确认网络接口的名字和我们配置的静态ip是否对应。比如在主机端是`eno1`，开发板是`end0`，那么配置静态ip时需要配置到对应网卡设备上。

`ip link`，`ip addr`以及`ifconfig`都能够查看网口名。

此外还需要确认对应网卡是否被启用，网卡状态栏中是否有`UP`，如果是`DOWN`则：

```bash
ip link set end0 up
# 或者
ip link set eno1 up
```

然后需要检查IP/子网配置，这里我们期望的掩码应该为24位。

还需要检查物理链路是否正常，也就是网口灯是否是绿灯常亮，橙灯闪烁。

用`ethtool`工具查看链路状态：

```bash
ethtool end0
```

另外，在pc这边最好是关闭防火墙，我使用的是Ubuntu，因此可以直接：

```bash
sudo ufw disable
```

用ARP判断在PC上先ping，然后查看ARP表：

```bash
ping 192.168.1.101
arp -n
```

如果有ARP但是ping不通，那应该是IP层/防火墙的问题。

如果连ARP都没有，说明二层就没有通(驱动/PHY/网线)。

可以通过在开发板上：

```bash
dmesg | grep -i eth
dmesg | grep -i rk_gmac
```

查看是否存在以太网错误。

我按照上述方式进行了测试，发现先ping后稍等一下arp查看，在开发板端能够看到pc的MAC地址，但是主机这边ping后arp什么也看不到。说明是一个"单项通信的问题"。通过`ping`+`sudo tcpdump -i <网卡> arp`来抓取信息发现也是同样的结果。开发板发出了请求，但是主机无法接收。

**说明开发板的TX出现了问题！**

可以查看内核日志，是否存在GMAC相关的警告：

```bash
dmesg | grep -i gmac
dmesg | grep -i eth
dmesg | grep -i phy
```

我们还可以利用`ethtool`工具让网口强制降速来进行测试。我们可以通过`ethtool end0`来查看当前网卡的配置，它现在是自动协商的速率：

```bash
root@rk3568-avod:~# ethtool end0
Settings for end0:
        Supported ports: [ TP    MII ]
        Supported link modes:   10baseT/Half 10baseT/Full
                                100baseT/Half 100baseT/Full
                                1000baseT/Full
        Supported pause frame use: Symmetric Receive-only
        Supports auto-negotiation: Yes
        Supported FEC modes: Not reported
        Advertised link modes:  10baseT/Half 10baseT/Full
                                100baseT/Half 100baseT/Full
                                1000baseT/Full
        Advertised pause frame use: Symmetric Receive-only
        Advertised auto-negotiation: Yes
        Advertised FEC modes: Not reported
        Link partner advertised link modes:  10baseT/Half 10baseT/Full
                                             100baseT/Half 100baseT/Full
                                             1000baseT/Full
        Link partner advertised pause frame use: Symmetric Receive-only
        Link partner advertised auto-negotiation: Yes
        Link partner advertised FEC modes: Not reported
        Speed: 1000Mb/s
        Duplex: Full
        Auto-negotiation: on
        master-slave cfg: preferred slave
        master-slave status: slave
        Port: MII
        PHYAD: 0
        Transceiver: external
        Supports Wake-on: ug
        Wake-on: d
        Current message level: 0x0000003f (63)
                               drv probe link timer ifdown ifup
        Link detected: yes
```

我们通过：
```bash
ethtool -s end0 speed 100 duplex full autoneg off
```

来降速到百兆，再进行ping测试，如果ping通了那就是RGMII时序的问题。也可以关闭`autoneg`，强制设置速率为`1000`，看看是否是自动协商的问题。测试发现，自动协商没问题，强制降速后成功ping通，很有可能是RGMII时序的问题。**不过我尝试调大和调小`tx_delay`的值，都没有成功调通...**

然后我尝试修改了`phy-mode`的模式，原本的模式为`rgmii`，无内部延时。首先尝试修改为`rgmii-id`，在收发两端都加入内部延时，一样ping不通。

尝试使用`rgmii-txid`，只在发送端添加延时，甚至连百兆速率下都无法通过ARP获取对方MAC地址了。

尝试匹配主机-开发板两端的流控：

```bash
sudo ethtool -A eno1 rx off tx off
(root) ethtool -A end0 rx off tx off
```

依然无法解决问题。

降速到百兆后虽然能够ping通，但是丢包率非常高，超过50%。用tcpdump抓包查看数据发现还是开发板发送reply数据到pc端，pc端不一定能够收到。我对比了接收到的reply数据包，能够被tcpdump抓到的都是没问题的，开发板发送的和pc端收到的是一个包，没有错误。

### 采用发包工具验证

尝试用发包工具，在win下使用发包工具`anysend`发送至开发板(使用的是ARP协议)，开发板端可以使用

```bash
tcpdump -i end0 -XXX arp
```

来查看ARP协议的具体报文。能够成功收到报文。那么和之前的结论一致。开发板的RX端没有问题。

现在需要在开发板端向PC发包，我们可以将PC端发送的ARP消息通过tcpdump保存到`pcap`文件中，然后利用`tcpreplay`工具发包给主机。

```bash
tcpdump -i end0 -w /temp/rx_arp.pcap arp # 注意输入该指令后再开始ping，ping完ctrl+c退出才会写入pcap
tcpreplay -i end0 --pps=1 /temp/rx_arp.pcap # 向end0网口发送arp数据包
```

`tcpreplay`不用管ip地址，只需要我们的网口相连。结果发现果然果然开发板的TX通路有问题，发送成功但是PC端接受不到。

定位还是PHY的TX通路有问题。最后发现可能是PHY芯片的硬件电路在TX相关电路上存在问题。遂搁置。

因为在百兆下，对延时的要求降低了，所以能够ping通，但是也仍然丢包严重。



# U-boot

就像x86的BIOS一样，对于嵌入式Linux，U-boot是最常见的bootloader之一(开源)，因此有必要学习和掌握U-boot的一些基本知识，并且具备使用U-boot修改系统配置的能力。

对于家用的x86架构的个人PC，在主板LOGO界面按`F2`之类的键来进入BIOS的GUI。x86的BIOS一般由各个主板的厂商直接提供，并且针对消费级用户，因此具备GUI这类友好的交互界面。U-boot则是提供命令行界面，并且具有自己的命令行语法。

## U-boot的FIT镜像

之前一直纳闷为什么在前辈的`wks.in`文件中有一个单独的`trust.img`镜像，并且裸写烧进板子固定偏移处的。但是wic镜像中没有这个子镜像。我直接用组件烧录时没有烧录他，系统也可以起来。当时以为是该镜像可有可无，启动中会自动判断。

但是在仔细查看了镜像和日志后发现不是那么一回事。我去检查了yocto的构建日志，发现他使用的`wks`文件在从源码拷贝到出来时**将`trust`的那一行给删除了**。这是非常神奇的，说明`wic`镜像在制作时根本没有`trust.img`这一个镜像。去查看`u-boot-*.bb`的源码也可以看到根本没有生成该镜像。但是系统启动时可以看到是有`BL31`存在的。那么这是怎么回事？

去查看了`U-boot`的编译过程，其中使用的一些脚本如`make.sh`中提到了一个特殊的启动方式：`FIT`镜像。如果`U-boot`源码中启用了`CONFIG_FIT*`相关的选项，那在构建`U-boot`镜像时将会根据脚本规则(这里将`Trust`包含的`BL31`和`BL32`都打包放进`FIT`)将一些组件打包放进了`FIT`镜像。

而`FIT`镜像是一个使用类似设备树文件来组织的镜像包。它是为了解决启动流程越来越复杂的的情况下，用一个打包好的镜像来描述其中各个组件的启动链。比如对于`armv8`。启动链从`BL1`一直到`BL33`而启动的`BL31，BL32，BL33`都有着自己的：加载地址、入口地址、运行异常等级、是否需要校验、签名以及启动顺序。

在`FIT`镜像前，这些启动流程中的组件间的关系和启动顺序更多是依靠**硬编码和约定来规定好**，是写死的。没有统一的描述结构。一点都不优雅。

了解了一下`FIT`镜像后也是终于找到了为啥`trust.img`会不见了。想要确定镜像是否是一个`FIT`镜像，可以用

```bash
file /path/to/your/image/uboot.img
*/uboot.img: Device Tree Blob version 17, size=3584, boot CPU=0, string block size=197, DT structure block size=2828
```

`FIT`镜像其实是一个`.itb`格式的文件，是一种特殊的设备树文件。所以这里我们看到是`Device Tree Blob version`。

如何查看`FIT`的具体信息呢，可以使用

```bash
dumpimage -l uboot.img
```

这样可以看到该镜像的一些更加详细的输出。



## U-boot命令行

嵌入式设备可以利用USB接口连接PC，使用串口来调试。设备会将调试信息输出到串口，并且接受串口输入。

当板子上电后，控制台通常会打印U-boot的启动信息。在出现`Hit any key to stop autoboot:`的倒计时时，**立即按下回车键**，就能够中断自动启动流程，进入U-boot的命令行模式。如果错过了按键，那么就会执行预设的`bootcmd`环境变量来自动启动内核。

命令行界面就是我们和U-boot这个bootloader进行交互的窗口。

下面给出一些常用的U-boot命令类别和核心的指令：

| **命令类别**     | **命令示例**                                         | **功能说明**                                                 |
| :--------------- | :--------------------------------------------------- | :----------------------------------------------------------- |
| **信息查询**     | `bdinfo`, `version`, `printenv`                      | 查看板卡信息、U-Boot版本、所有环境变量。                     |
| **环境变量操作** | `setenv ipaddr 192.168.1.100`, `saveenv`             | 设置环境变量（如IP地址），并将变更保存到Flash中使其永久生效。 |
| **内存操作**     | `md.b 0x80000000 10`, `mw.w 0x82000000 0xdeadbeef 1` | 显示/修改内存内容，用于低级调试。                            |
| **存储设备操作** | `mmc info`, `fatload mmc 0:1 0x82000000 zImage`      | 查看存储设备信息；从SD卡（MMC设备）的FAT分区加载文件到内存。 |
| **网络操作**     | `ping 192.168.1.1`, `tftpboot 0x82000000 uImage`     | 测试网络连通性；通过TFTP协议从服务器下载文件（如内核镜像）到内存。 |
| **系统启动**     | `bootm 0x82000000`, `bootz 0x82000000 - 0x83000000`  | 启动位于内存指定地址的内核镜像。`bootz`常用于启动zImage格式内核，`-`表示无initrd，后跟设备树地址。 |



## 在U-boot源码中修改启动配置宏

U-boot的环境变量是通过编译时**拼接字符串宏来生成的**。如果我们自己更改了某一个环境变量的宏，并且出现了语法错误，那么就可能出现把后面所有的环境变量给截断从而导致环境变量的缺失。

注意每一条命令后一定要跟`;`！

`\0`是用于在一个宏中定义多个环境变量时，用来分隔每一个环境变量的，而`;`是用来分隔 环境变量中的命令的，如果环境变量中储存的是一个具体的值，那么也不需要加`;`。



在U-boot源码中，有大量和硬件完全无关的代码(这是上层主体)，也有很多和芯片硬件架构，和硬件电路(开发板)相关的代码。如果需要做改动，我们一般也是去改动和对应硬件相关的文件，很少去动那些共性文件。作为底层开发者，我们需要去关注与架构和硬件相关的代码目录：

* `arch`：即`architecture`，也就是架构目录。该目录下放着所有U-boot支持的架构源码，由各个企业支持。而所谓的架构一般就是与板子所使用的`SoC`绑定，比如我们使用的是`rk3568`，那么就需要关注`arm64`、`rockchip`、`armv8`等目录，然后关注相关的源码文件。另外，U-boot使用的设备树文件也在该目录下，一般位于对应架构下的`dts`目录，比如`arch/arm/dts/`
* `board`：这是和具体的板子对应了，一个完整的开发板，除了核心以外还有大量的外围电路。不同的开发板外围电路存在着差异，在这个目录下可能就定义了这些差异。同时我们也可以在这里去定义自己定制的开发板。本着尽量不改动原码的原则，创建一个新的目录来放置我们自己的文件，然后去更改对应的`config`配置，才是符合开发标准的。
* `include/configs`：`include`放置了U-boot的各类头文件，而`configs`下又放置了众多开发板的配置头文件。我们的启动参数，或者说U-boot内置的环境变量就放在这里，比如`evb_rk3568.h`。

我们如果想要定制环境变量和启动参数，比较好的办法就是在`board`和`include/configs`目录下创建自己板子的相关文件，然后改动`Kconfig`，并在`menuconfig`或者手动修改`defconfig`/`.config`文件使得编译时选中我们自己的文件。

有关`Kconfig`，`defconfig`和`menuconfig`之间的关系，需要单独了解，这里不作赘述。



## 移植U-boot的源码，添加命令行工具

前辈要求实现在U-boot下通过`TFTP`来更新内核或者`rootfs`镜像，数据流为

```bash
remote->ddr->emmc
```

当然也可以直接通过在U-boot中通过命令行来实现，但是对于产线来说，最好的方法是集成为一条命令，通过带参数来实现自动化更新。前辈已经实现了这一功能，我在这里主要是学习如何在U-boot源码中添加自己的指令，并且扩展了解一下U-boot源码的结构。



在U-boot源码中，命令行所使用的各种工具都在`cmd`目录下有定义，按照其类型分为各个源码文件甚至目录。由于我们的任务最终是将系统镜像下载到存储介质中(多是EMMC)，因此我们可以直接在`mmc.c`中添加命令的源码。

**注意！**添加完我们的源码函数后，需要使用`U_BOOT_CMD`宏来注册我们的命令，这是标准做法，这样U-boot在编译时会识别到这是一个命令并把它放到一个特殊的段中，并注册到命令表中。但是这还没完！

我们还需要修改两处代码，才能够真正让U-boot发现并使用我们的代码。首先是修改一下`cmd`目录下的`Makefile`，因为有一些`Makefile`中可能是手动选择编译对象，或者条件选择，而不一定是类似脚本话的自动编译(个人认为应该是为了实现比较高自由度的定制吧)。我们可能需要在末尾添加自定义文件(如果单独新建了源码文件的话)的编译规则：

```makefile
obj-y += avod.o
# 或者
obj-$(CONFIG_CMD_AVOD) += avod.o
```

第一个是直接编译，第二个则是条件编译，如果存在`CONFIG_CMD_AVOD`的配置项(在`.config`中)，那么就会编译。而这个配置项需要和接下来讲的`Kconfig`相配合。

然后我们需要修改`cmd`目录下的`Kconfig`，添加一小段内容，比如：

```bash
config CMD_UP
	bool "update - update emmc image from remote tftp server"
	default n
	select CMD_PART
	select CMD_MMC
	select CMD_DHCP
	select CMD_TFTPBOOT
	help
	  This allows user to update imx-boot, kernel, dtb and rootfs
          images that store in emmc media.
```

然后可以将我们的源码用：

```c
#if defined(CONFIG_CMD_UP)
#endif
```

包裹起来，这样是一个比较标准的符合U-boot开发的方式，通过配置选项来管理我们的源码是否被编译。但是其实也可以不用修改`Kconfig`，只需要添加一个简单的`Makefile`命令，这样我们的源码就会正常参与编译，只是这样非常不优雅。



## U-boot 启动流程

这里以rk3568(armv8架构)为例。

U-boot的启动入口一般是`arch/arm/cpu/armv8/start.S`(注意不同架构的路径不同)。

```assembly
master_cpu
	bl _main
```

在arm汇编指令中`bl`一般是跳转并保存返回地址(类似于函数调用)。那么入口就是名为`_main`的字段。而在源码中我们找不到这个字段。`_main`很可能是一个全局符号，在链接阶段被解析为一个**具体的地址**。比较好的办法是通过`nm`工具去查看符号表

我们可以在本地用sdk编译一遍u-boot，然后找到编译生成的`elf`文件(一般就叫做`u-boot`):

```bash
file u-boot
u-boot: ELF 64-bit LSB executable, ARM aarch64, version 1 (SYSV), statically linked, with debug_info, not stripped
```

然后用

```bash
aarch64-linux-gnu-nm -n u-boot | grep _main
aarch64-linux-gnu-nm -n u-boot | grep main
```

来查询这两个符号所指的地址是否一致。这里我们主要是想要找到`_main`到底指向了哪里，来追踪u-boot的启动流程。

而`common/main.c`似乎是进入`C`之后的一个通用程序入口，和这里的`_main`所指地址不一定相同。

想要查询`_main`这个符号到底在哪里，我们还可以使用`u-boot.map`这个编译后生成的**链接映射表**，这里面会告诉你`_main`的符号来源。

```bash
grep -R -A10 -B10 "_main" u-boot.map
# -A10 查看后10行
# -B10 查看前10行
```

最后发现`_main`在`arch/arm/lib/built-in.o`中，而不是在`common/main.o`中，说明两个`main`不同！这里`_main`还未进入标准`C`。

这里我们只找到了`_main`所在的`.o`文件，继续寻找其源文件位置：
```bash
aarch64-linux-gnu-nm arch/arm/lib/built-in.o | grep _main
aarch64-linux-gnu-objdump -t arch/arm/lib/built-in.o | grep _main
```

这里可以得到一些信息，但是只能说明`_main`的确是在该`.o`文件中，而具体对应到哪一个源文件，这样其实不太好找。`built-in.o`很可能是该目录下的其他`.o`文件聚合得到的。我们也可以继续`nm`来扫描其他的`.o`文件。也可以粗暴一点，去对应的源码目录下

```bash
grep -R "_main" arch/arm/lib
```

来查找。最终定位到了`crt0.S`和`crt0_64.S`两个文件，根据架构我们应该使用`crt0_64.S`。这样就找到了`_main`符号对应的源码了，继续阅读~

在`crt0_64.S`中可以看到

```assembly
ENTRY(_main) # 汇编程序入口点
...
ENDPROC(_main) # 标记函数结束
```

这就是我们要找的`_main`汇编函数，里面我们可以注意到跳转和调用了几个重要的函数：

```assembly
bl	board_init_f # f指 before relocation
b	relocate_code
b	board_init_r # r指 after relocation
```

这里有一个很重要的概念：**重定位**。U-boot在启动早期不在最终运行地址运行，因为此时DRAM还未初始化，在`board_init_f()`中初始化DRAM后，U-boot会会将自己拷贝到DRAM的最终地址处，这就是`relocate`。其他的一些`_f`和`_r`也是类似的。这里先了解流程就好。

然后在`board_init_r()`中：

```c
if(initcall_run_list(init_sequence_r))
	...
```

这里的`init_sequence_r`就是一系列的初始化流程函数，他是一个函数指针数组(结构类似)。最后一个叫做`run_main_loop`：

```c
static int run_main_loop(void)
{
	...
	for(;;)
		main_loop();
	return 0;
}
```

这一次就是真的进入标准C程序了。



## U-boot设备模型

```
设备树节点 (fdt node)
        ↓ 解析
udevice（设备实例）
        ↓ 属于
uclass（设备类别）
        ↓ 由
driver（驱动）来 probe / bind
```

U-boot与Kernel有着类似的设备模型，设备树节点的解析这部分暂时先略过。

`udevice`结构体就是设备实例，而`uclass`则有一点类似于`kset`，它是一类设备的容器。

我们会看到`UCLASS_DRIVER`，这个并不是一个驱动，而是设备类别的总体定义。而`U_BOOT_DRIVER`才是驱动的定义，它会初始化一个`struct driver`结构体。在其中会描述该驱动属于哪一类设备(`uclass id`)，能匹配哪些节点(`of_match`)，如何匹配，以及驱动具体的操作等等。

U-boot设备模型初始化主要是在`board_init_r()->initcall_run_list(init_sequence_r)->initr_dm()`。在这里会扫描设备树节点，建立其设备模型的基本拓扑结构，并且**绑定**设备和对应驱动。这里的绑定只是建立关系，并不会访问硬件。简单来说就是把`udevice->driver`指针给填上。但是实际上其对应的`probe`函数还未调用。



需要到对应子系统初始化时(一般也在`board_init_r()`中)，才回去`probe`，这个时候就会去初始化对应的设备。

这里以gmac为例：

```
initr_net()
	└─ eth_initialize()
		└─ uclass_first_device(UCLASS_ETH, &dev)
			└─ uclass_get_device_tail(dev, ret, devp)
				└─ device_probe(dev)
```

这是代码层面可以找到的，但是具体的mac控制器和phy芯片的驱动和设备是如何绑定和probe的，我其实还是会有一点点懵。

