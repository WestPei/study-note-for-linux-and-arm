- [1. Bash 基础](#1-bash-基础)
  - [1.1. 变量与括号](#11-变量与括号)
  - [1.2. 赋值操作符](#12-赋值操作符)
- [2. Linux 内核源码阅读](#2-linux-内核源码阅读)
  - [2.1. file.list + Source Insight (SI)](#21-filelist--source-insight-si)
    - [2.1.1. 背景：为什么需要 file.list](#211-背景为什么需要-filelist)
    - [2.1.2. 操作步骤](#212-操作步骤)
  - [2.2. compile\_commands.json + VScode clangd](#22-compile_commandsjson--vscode-clangd)
    - [2.2.1. Yocto 环境的挑战](#221-yocto-环境的挑战)
    - [2.2.2. 解决方案：本地 SDK 编译](#222-解决方案本地-sdk-编译)
    - [2.2.3. clangd 配置](#223-clangd-配置)
- [3. U-boot 源码阅读](#3-u-boot-源码阅读)
- [4. U-boot 深入](#4-u-boot-深入)
  - [4.1. FIT 镜像](#41-fit-镜像)
    - [4.1.1. 概念](#411-概念)
    - [4.1.2. 识别 FIT 镜像](#412-识别-fit-镜像)
    - [4.1.3. 查看 FIT 信息](#413-查看-fit-信息)
    - [4.1.4. 与 trust.img 的关系](#414-与-trustimg-的关系)
  - [4.2. U-boot 命令行](#42-u-boot-命令行)
    - [4.2.1. 常用命令分类](#421-常用命令分类)
  - [4.3. 修改启动配置宏](#43-修改启动配置宏)
    - [4.3.1. 与硬件相关的代码目录](#431-与硬件相关的代码目录)
    - [4.3.2. 定制环境变量的标准做法](#432-定制环境变量的标准做法)
  - [4.4. 移植源码并添加自定义命令](#44-移植源码并添加自定义命令)
    - [4.4.1. 基本流程](#441-基本流程)
  - [4.5. U-boot 启动流程 (ARMv8 / rk3568)](#45-u-boot-启动流程-armv8--rk3568)
    - [4.5.1. 入口点](#451-入口点)
    - [4.5.2. 重定位 (Relocation)](#452-重定位-relocation)
    - [4.5.3. 进入主循环](#453-进入主循环)
    - [4.5.4. 关键调用链总结](#454-关键调用链总结)
  - [4.6. U-boot 环境变量加载](#46-u-boot-环境变量加载)
    - [4.6.1. `CONFIG_USING_KERNEL_DTB` 带来的变化](#461-config_using_kernel_dtb-带来的变化)
    - [4.6.2. 经典加载流程](#462-经典加载流程)
  - [4.7. U-boot 设备模型](#47-u-boot-设备模型)
    - [4.7.1. 关键结构体](#471-关键结构体)
    - [4.7.2. 初始化流程](#472-初始化流程)
    - [4.7.3. 以 GMAC 为例](#473-以-gmac-为例)
- [5. 案例：网口调试 (rk3568 GMAC)](#5-案例网口调试-rk3568-gmac)
  - [5.1. 背景](#51-背景)
  - [5.2. 排查过程](#52-排查过程)
    - [5.2.1. 基础检查](#521-基础检查)
    - [5.2.2. 物理链路检查](#522-物理链路检查)
    - [5.2.3. ARP 测试](#523-arp-测试)
    - [5.2.4. 降速测试](#524-降速测试)
    - [5.2.5. PHY Mode 测试](#525-phy-mode-测试)
    - [5.2.6. 发包工具验证](#526-发包工具验证)
  - [5.3. 结论](#53-结论)
- [6. 附录：常用调试命令速查](#6-附录常用调试命令速查)


---

# 1. Bash 基础

Yocto 工程的初始化不可避免地使用 Bash 脚本，因此有必要掌握 Bash 脚本的基本语法。

## 1.1. 变量与括号

美元符号 `$` 与各种括号在脚本中有着强大的功能：

| 符号组合 | 主要用途 | 简要说明与示例 |
|:---|:---|:---|
| `$var` / `${var}` | **变量引用** | 获取变量值。花括号主要用于明确变量名的边界，例如 `echo ${file}s` 可以清晰地区分变量 `file` 和字母 `s` |
| `$(command)` | **命令替换** | 执行 `command` 并将其标准输出结果替换到当前位置。例如 `current_date=$(date)` |
| `$((expression))` | **算术运算** | 计算整数算术表达式。例如 `result=$(( 5 + 3 ))` |
| `()` | **子 Shell 中执行** | 在**新的子 Shell 进程**中执行括号内的命令。括号内的变量更改不会影响父 Shell。例如 `(cd /tmp && pwd)` |
| `{}` | **当前 Shell 中执行** | 在**当前 Shell 进程**中执行括号内的命令（命令需用分号 `;` 隔开，左括号后需有空格）。括号内的变量更改会影响当前 Shell。例如 `{ var="hello"; echo $var; }` |
| `{1..10}` | **序列扩展** | **不**需要 `$`，直接生成一个序列。例如 `echo {1..3}` 输出 `1 2 3`，`touch file{01..03}.txt` 创建三个文件 |
| `[ condition ]` | **条件测试** | `test` 命令的另一种形式，用于条件判断。括号内的条件表达式需要遵循特定规则，例如字符串比较用 `=`，整数比较用 `-eq` |
| `[[ condition ]]` | **增强的条件测试** | 比 `[]` 更强大、更安全，支持模式匹配（`==`）和正则匹配（`=~`）。例如 `if [[ "$name" == u* ]]` |


---

## 1.2. 赋值操作符

| 写法 | 展开时机 | 说明 |
|:---|:---|:---|
| `=` | 使用时（延迟展开） | 递归展开 |
| `:=` | 定义时（立即展开） | 简单展开 |
| `?=` | 如果未定义 | 条件赋值 |
| `+=` | 立即/延迟依上下文 | 追加 |

---

# 2. Linux 内核源码阅读

## 2.1. file.list + Source Insight (SI)

### 2.1.1. 背景：为什么需要 file.list

前辈提供的脚本 + Yocto 时间戳判断哪些文件参与编译的方法似乎因为内核版本/Yocto 版本不同而失效了，所以要使用 Source Insight 去查看代码可能就需要手动去剔除不需要的源码文件，工作量有些大。

我换了一种思路，通过解析 Yocto `${WORKDIR}/build` 目录中的 `.o.cmd` 文件来得到编译所需要的全部文件的列表（生成基于源码顶层目录的相对路径），然后再拷贝一个干净的源码目录作为阅读用，在 SI 中通过该 `file.list` 只加入所需要的源码实现阅读。

### 2.1.2. 操作步骤

1. **生成文件列表**：首先需要 Yocto 完整构建一次 `virtual/kernel`（其实只需要 `bitbake -c compile`），得到 `${WORKDIR}/build` 目录，然后使用制作好的脚本 `make-file-list(kernel).sh` 在 `build` 目录下运行，就能够得到内核源码编译时所需要的全部源文件/头文件的列表，生成 `kernel-file.list`

2. **准备源码副本**：将 `build` 目录下的 `.config` 和脚本生成的 `kernel-file.list` 一同拷贝到干净的源码副本根目录下。因为我们还需要生成一些编译中间文件，为了不污染源码，最好是单独拷贝一个副本来用作阅读

3. **（可选）生成配置中间文件**：为了使得 `.config` 中的宏生效并展开，更方便阅读（展开 `#ifdef` 宏），可以在源码副本中执行：
   ```bash
   make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- prepare
   ```
   这会生成 `generated` 目录和 `autoconf.h`（最终编译使用的配置宏定义）等文件。`ARCH` 和 `CROSS_COMPILE` 的选择不必与 Yocto 完全一致（此处不使用 `aarch64-poky-linux-`），因为不是真正的编译

4. **在 SI 中创建项目**：源码目录选择源码副本（包含 `kernel-file.list`），添加文件时选择 `add from list`，导入 `kernel-file.list`，然后就是同步和条件解析（用于解析 `#ifdef`）

这样基本实现了 SI 阅读源码，不过 `#include` 这样的头文件跳转似乎还是有点问题（SI 对某些路径解析不完整），但核心的代码浏览和 `#ifdef` 条件解析可以正常工作。

**注意**：该方案可能因内核版本不同导致 `.o.cmd` 文件格式存在差异，使脚本提取失败（前辈提供的脚本就因为内核/Yocto 版本变化而失效）。可行的做法是将要阅读的源码对应的 `.o.cmd` 文件内容发给 AI，让其根据实际格式针对性地写提取脚本。

## 2.2. compile_commands.json + VScode clangd

### 2.2.1. Yocto 环境的挑战

Yocto 的构建系统，由于不是使用本地编译器，并且隔离了源码和构建目录，因此即使能够使用 `bitbake -c devshell` 生成 `compile_commands.json`，也没有办法使用 VScode+clangd 来实现很好的代码阅读。

正常情况下本地 `make` 会将编译结果一同放在源码目录下，然后 `compile_commands.json` 就完全是基于源码+构建二合一目录的，我们在使用 `clangd` 插件时，VScode 也只需要打开源码目录就可以被 `clangd` 服务器解析并实现跳转。而 Yocto 则采用了源码/编译产物隔离的策略，将所有的构建产物全部放在 `${WORKDIR}/build` 中，就连 `.config` 和 `compile_commands.json` 也一样，这样导致 `compile_commands.json` 中的路径：

- 源码路径指向 `${WORKDIR}/kernel`（绝对路径）
- 构建目录指向 `${WORKDIR}/build`（绝对路径）

两者分离，`clangd` 解析困难。而 `devshell` 生成的 `compile_commands.json` 完全基于 `${WORKDIR}` 绝对路径，想用它解析本地源码需要大幅改动，不可行。

### 2.2.2. 解决方案：本地 SDK 编译

然后我换了一个思路，Yocto 可以制作镜像的 SDK，也就是说可以在本地安装我自己定义的 `image` 的 SDK，当然也就包含有编译工具链，那么我就可以直接在本地使用 `make` 来生成本地目录对应的 `compile_commands.json` 了！

```bash
# 基于 defconfig 生成 .config
make defconfig

# 生成 generated 目录和 autoconf.h
make prepare

# 生成 compile_commands.json
make compile_commands -j$(nproc)
```

这样就会在源码根目录下生成 `compile_commands.json` 文件。

### 2.2.3. clangd 配置

在源码根目录下创建 `.clangd` 文件，移除 clangd 不能识别的 GCC 特有选项：

```yaml
CompileFlags:
  Remove:
    - -mabi=*
    - -fconserve-stack
    - -fno-var-tracking-assignments
    - -fno-ipa-sra
    - -fno-ipa-cp
# 也可以根据具体的信息来忽略一些声明
```

在 `.vscode/settings.json` 中配置当前文件夹的 clangd 设置。

完成配置或者修改配置后，记得 `Shift+Ctrl+P` → `clangd: Restart language server` 来重启 `clangd` 服务器使得配置生效。然后我们就可以正常跳转了。

**注意**：如果跳转不了，大概率是当前文件没有参与编译（未包含在 `compile_commands.json` 中）。

不过为了不污染源码目录(`.vscode`这种目录会被 svn 忽略)，我们可以把 `compile_commands.json` 放到源码外。(一方面是因为 yocto 构建时要求我们的源码目录的干净，而 `make distclean` 会把源码目录下的 `compile_commands.json` 给清理，而每次生成很花时间，不如直接保存在外面)。

---

# 3. U-boot 源码阅读

如果是比较新版本的 U-boot，那么它本身也支持 `make compile_commands.json`，那么操作方法就和 kernel 一致。

但是对于 rk3568 使用的是比较老版本的 U-boot，虽然也可以生成 `compile_commands.json`，但 `clangd` 对它兼容性非常差（相比起 kernel）。不过我们暂时也没有大幅度改动和阅读 U-boot 源码的需求，所以也就暂时搁置了。

不过有了 `compile_commands.json`，即使是因为 `clangd` 兼容不了，也可以直接使用 VScode 的 `C/C++` 插件来实现跳转，只是每一次打开都需要重新加载，速度会比较慢，不过能用了。以后更换高版本的 U-boot 了再说吧。

这里记录一下，`bear` 用来生成 `compile_commands.json` 不太靠谱，似乎环境变量有问题，会报错。所以可以尝试使用一个叫做 `intercept-build` 的工具命令。

---

# 4. U-boot 深入

就像 x86 的 BIOS 一样，对于嵌入式 Linux，U-boot 是最常见的 bootloader 之一（开源），因此有必要学习和掌握 U-boot 的一些基本知识，并且具备使用 U-boot 修改系统配置的能力。

对于家用的 x86 架构的个人 PC，在主板 LOGO 界面按 `F2` 之类的键来进入 BIOS 的 GUI。x86 的 BIOS 一般由各个主板的厂商直接提供，并且针对消费级用户，因此具备 GUI 这类友好的交互界面。U-boot 则是提供命令行界面，并且具有自己的命令行语法。

## 4.1. FIT 镜像

### 4.1.1. 概念

之前一直纳闷为什么在前辈的 `wks.in` 文件中有一个单独的 `trust.img` 镜像，并且裸写烧进板子固定偏移处的。但是 wic 镜像中没有这个子镜像。我直接用组件烧录时没有烧录他，系统也可以起来。当时以为是该镜像可有可无，启动中会自动判断。

但是在仔细查看了镜像和日志后发现不是那么一回事。我去检查了 Yocto 的构建日志，发现他使用的 `wks` 文件在从源码拷贝到出来时**将 `trust` 的那一行给删除了**。这是非常神奇的，说明 `wic` 镜像在制作时根本没有 `trust.img` 这一个镜像。去查看 `u-boot-*.bb` 的源码也可以看到根本没有生成该镜像。但是系统启动时可以看到是有 `BL31` 存在的。那么这是怎么回事？

去查看了 U-boot 的编译过程，其中使用的一些脚本如 `make.sh` 中提到了一个特殊的启动方式：**FIT 镜像**。如果 U-boot 源码中启用了 `CONFIG_FIT*` 相关的选项，那在构建 U-boot 镜像时将会根据脚本规则（将 Trust 包含的 BL31 和 BL32 都打包放进 FIT）将一些组件打包放进了 FIT 镜像。

FIT (Flattened Image Tree) 镜像是用设备树语法组织的镜像包，解决启动流程越来越复杂的问题。对于 ARMv8，启动链从 BL1 到 BL33（BL1→BL31→BL32→BL33），每个组件都有各自的加载地址、入口地址、运行异常等级、是否需要校验/签名以及启动顺序。

在 FIT 之前，这些组件间的关系和启动顺序更多依靠**硬编码和约定**来规定，没有统一的描述结构。FIT 镜像用一套设备树格式统一描述所有组件的元信息，实现了启动链的声明式管理。

### 4.1.2. 识别 FIT 镜像

```bash
file /path/to/your/image/uboot.img
# 输出: Device Tree Blob version 17, size=3584, boot CPU=0, string block size=197, DT structure block size=2828
```

FIT 镜像实际是 `.itb` 格式，是一种特殊的设备树文件。

### 4.1.3. 查看 FIT 信息

```bash
dumpimage -l uboot.img
```

### 4.1.4. 与 trust.img 的关系

如果 U-boot 启用了 `CONFIG_FIT_*` 相关选项，构建时会将 Trust 固件（BL31、BL32）打包进 FIT 镜像。这解释了为什么有些项目中 `trust.img` 被"消失"了——它已经被合并进 `uboot.img` 中。

## 4.2. U-boot 命令行

嵌入式设备可以利用 USB 接口连接 PC，使用串口来调试。设备会将调试信息输出到串口，并且接受串口输入。

当板子上电后，控制台通常会打印 U-boot 的启动信息。在出现 `Hit any key to stop autoboot:` 的倒计时时，**立即按下回车键**，就能够中断自动启动流程，进入 U-boot 的命令行模式。如果错过了按键，那么就会执行预设的 `bootcmd` 环境变量来自动启动内核。

命令行界面就是我们和 U-boot 这个 bootloader 进行交互的窗口。

### 4.2.1. 常用命令分类

| 类别 | 命令示例 | 功能说明 |
|:---|:---|:---|
| **信息查询** | `bdinfo`, `version`, `printenv` | 查看板卡信息、U-Boot 版本、所有环境变量 |
| **环境变量操作** | `setenv ipaddr 192.168.1.100`, `saveenv` | 设置环境变量（如 IP 地址），并将变更保存到 Flash 中使其永久生效 |
| **内存操作** | `md.b 0x80000000 10`, `mw.w 0x82000000 0xdeadbeef 1` | 显示/修改内存内容，用于低级调试 |
| **存储设备操作** | `mmc info`, `fatload mmc 0:1 0x82000000 zImage` | 查看存储设备信息；从 SD 卡（MMC 设备）的 FAT 分区加载文件到内存 |
| **网络操作** | `ping 192.168.1.1`, `tftpboot 0x82000000 uImage` | 测试网络连通性；通过 TFTP 协议从服务器下载文件（如内核镜像）到内存 |
| **系统启动** | `bootm 0x82000000`, `booti ...`, `bootz 0x82000000 - 0x83000000` | 启动位于内存指定地址的内核镜像。`bootz` 常用于启动 zImage 格式内核，`-` 表示无 initrd，后跟设备树地址 |

## 4.3. 修改启动配置宏

U-boot 的环境变量是通过编译时**拼接字符串宏来生成的**。如果我们自己更改了某一个环境变量的宏，并且出现了语法错误，那么就可能出现把后面所有的环境变量给截断从而导致环境变量的缺失。

注意每一条命令后一定要跟 `;`！

`\0` 是用于在一个宏中定义多个环境变量时，用来分隔每一个环境变量的，而 `;` 是用来分隔 环境变量中的命令的，如果环境变量中储存的是一个具体的值，那么也不需要加 `;`。

### 4.3.1. 与硬件相关的代码目录

在 U-boot 源码中，有大量和硬件完全无关的代码（这是上层主体），也有很多和芯片硬件架构、和硬件电路（开发板）相关的代码。如果需要做改动，我们一般也是去改动和对应硬件相关的文件，很少去动那些共性文件。作为底层开发者，我们需要去关注与架构和硬件相关的代码目录：

| 目录 | 说明 |
|:---|:---|
| `arch/` | 架构目录，包含 SoC 相关源码和设备树。板子所使用的 SoC 决定了关注的架构目录（如 `arm64`、`rockchip`、`armv8`）。U-boot 使用的设备树文件也在该目录下，一般位于 `arch/arm/dts/` |
| `board/` | 具体开发板相关的外围电路定义，可在此创建定制板。本着尽量不改动原码的原则，创建一个新的目录来放置自己的文件，然后更改对应的 `config` 配置 |
| `include/configs/` | 各开发板的配置头文件（启动参数/内置环境变量），如 `evb_rk3568.h` |

### 4.3.2. 定制环境变量的标准做法

在 `board/` 和 `include/configs/` 下创建自己板子的文件，改动 `Kconfig`，然后在 `menuconfig` 或 `defconfig` 中选中自定义文件，尽量不动原码。

## 4.4. 移植源码并添加自定义命令

前辈要求实现在 U-boot 下通过 `TFTP` 来更新内核或者 `rootfs` 镜像，数据流为 `remote → DDR → eMMC`。当然也可以直接通过在 U-boot 中通过命令行来实现，但是对于产线来说，最好的方法是集成为一条命令，通过带参数来实现自动化更新。

### 4.4.1. 基本流程

1. 在 `cmd/` 目录下添加命令源码（或直接修改现有文件，如 `cmd/mmc.c`）

2. 使用 `U_BOOT_CMD` 宏注册命令：
   ```c
   U_BOOT_CMD(
       mycmd,          // 命令名（字符串）
       4,              // 最大参数个数
       1,              // 是否可重复（0=不可，1=可）
       do_mycmd,       // 命令处理函数
       "short description",       // 简短描述（help 命令显示）
       "long help text"           // 详细帮助（help mycmd 显示）
   );
   ```
   U-boot 在编译时将 `U_BOOT_CMD` 宏声明的命令放入特殊段中，启动时自动注册到命令表。

3. 修改 `cmd/Makefile`，添加编译规则：
   ```makefile
   # 方式一：无条件编译（直接、但不优雅）
   obj-y += mycmd.o

   # 方式二：条件编译（标准做法，通过 Kconfig 控制）
   obj-$(CONFIG_CMD_MYCMD) += mycmd.o
   ```
   方式二需要配合 `cmd/Kconfig` 中定义 `CONFIG_CMD_MYCMD` 配置项，并在 `defconfig` 或 `menuconfig` 中启用。这是 U-boot 社区的标准做法——通过配置选项管理功能模块。

4. （可选）修改 `cmd/Kconfig`，添加配置项：
   ```
   config CMD_MYCMD
       bool "mycmd - description"
       default n
       help
         Help text for this command.
   ```

   实际项目中的 `Kconfig` 示例（TFTP 更新命令）：

   ```
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

   `select` 表示启用当前配置项时自动启用依赖项，无需手动逐个开启。

5. 在源码中用条件编译包裹：
   ```c
   #if defined(CONFIG_CMD_UP)
   // ... 命令实现
   #endif
   ```

   这种方式是 U-boot 社区的标准做法——通过配置选项管理功能模块。但是其实也可以不用修改 `Kconfig`，只需要添加一个简单的 `Makefile` 命令，这样我们的源码就会正常参与编译，只是这样非常不优雅。

## 4.5. U-boot 启动流程 (ARMv8 / rk3568)

### 4.5.1. 入口点

U-boot 的启动入口一般是 `arch/arm/cpu/armv8/start.S`（注意不同架构的路径不同）：

```assembly
master_cpu
    bl  _main
```

在 arm 汇编指令中 `bl` 一般是跳转并保存返回地址（类似于函数调用）。那么入口就是名为 `_main` 的字段。而在源码中我们找不到这个字段——`_main` 很可能是一个全局符号，在链接阶段被解析为一个具体的地址。

`_main` 是一个全局符号，在链接阶段解析为具体地址，**在源码中找不到它的定义**。需要用以下方法定位其来源 `.o` 文件：

```bash
# 方法 1: nm 查看 ELF 文件的符号表（按地址排序，-n）
aarch64-linux-gnu-nm -n u-boot | grep _main
# 同时对比 main_loop 的地址以确认不同
aarch64-linux-gnu-nm -n u-boot | grep main

# 方法 2: 在链接映射表中搜索（最直接）
grep -R -A10 -B10 "_main" u-boot.map
# -A10 查看匹配行后10行
# -B10 查看匹配行前10行
# .map 文件是编译后生成的链接映射表，记录了每个符号的来源

# 方法 3: 扫描中间 .o 文件的符号表
aarch64-linux-gnu-nm arch/arm/lib/built-in.o | grep _main
aarch64-linux-gnu-objdump -t arch/arm/lib/built-in.o | grep _main
```

这里为啥要去找这个 `main_loop()` 呢？因为这个函数应该是和我们打交道的，它也在一个非常权威的位置：`common/main.c`。一看就像是那种一个工程的入口点。

通过 `u-boot.map` 发现 `_main` 来自 `arch/arm/lib/built-in.o`，而非 `common/main.o`——说明 `_main` 和 `main_loop()` 是两个不同的入口点，`_main` 是汇编阶段的入口，`main_loop()` 是进入 C 语言后的主循环入口。

`built-in.o` 是目录级聚合的目标文件（由该目录下所有 `.o` 文件合并而成，因此直接在该目录下查找就好），不是最终的源文件。继续用 `grep` 在 `arch/arm/lib/` 目录下搜索：

```bash
grep -R "_main" arch/arm/lib/

grep: arch/arm/lib/built-in.o: 匹配到二进制文件
grep: arch/arm/lib/crt0_64.o: 匹配到二进制文件

```

最终定位到 `crt0_64.S`（ARMv8 64位架构的 C 运行时启动文件）和 `crt0.S`（ARMv7 32位版本，根据架构选择对应文件）。

```assembly
ENTRY(_main)    # 汇编程序入口点
    ...
    bl  board_init_f    # before relocation
    b   relocate_code
    b   board_init_r    # after relocation
ENDPROC(_main)  # 标记函数结束
```

### 4.5.2. 重定位 (Relocation)

U-boot 启动早期不在最终运行地址运行（DRAM 尚未初始化）。在 `board_init_f()` 中初始化 DRAM 后，U-boot 会将自己拷贝到 DRAM 的最终地址处——这就是 `relocate_code`。函数名中的 `_f` 和 `_r` 分别表示 "before/after relocation"。

U-boot 早期多是在片上 SRAM 中运行，此时 DRAM 还未初始化。不过由于 SRAM 太小了，进入 U-boot proper 后需要一些近似于内核的功能，所以在完成 DRAM 的初始化后就会重定向，将自己搬到 DRAM 中去。

**注意**：并不能简单将“重定位”和“DDR初始化”绑定！因为现代 U-boot 一般会在 SPL 阶段就将 DDR 进行初始化了，实际上我们看到的 U-boot 代码从 `Start.S` 开始已经进入了 U-boot proper 了，SPL 都过了！

所以这里的 `board_init_f()` 和 `board_init_r()` 本质上只是 U-boot 在对自己初始化前后的区别。重定向前对自己进行初始化，然后重定向。

### 4.5.3. 进入主循环

在 `board_init_r()` 中：

```c
if (initcall_run_list(init_sequence_r))
    ...
```

这里的 `init_sequence_r` 就是一系列的初始化流程函数，他是一个函数指针数组。最后一个叫做 `run_main_loop`：

```c
static int run_main_loop(void)
{
    ...
    for (;;)
        main_loop();
    return 0;
}
```

这一次就是真的进入标准 C 程序了。

### 4.5.4. 关键调用链总结

```
start.S: _main
  → crt0_64.S: board_init_f()     // 初始化 DRAM 等
  → crt0_64.S: relocate_code()    // 将自己拷贝到 DRAM 最终地址
  → crt0_64.S: board_init_r()     // 后续初始化
    → init_sequence_r             // 一系列 init 函数
      → initr_dm()                // 设备模型初始化
      → initr_net()               // 网络子系统初始化（其中触发 probe）
      → run_main_loop()           // 进入命令行主循环
        → main_loop()             // common/main.c
```

---

## 4.6. U-boot 环境变量加载

为了搞清楚 U-boot 环境变量是如何进行加载的，特别是有了存储介质之后，需求去中希望能够在 内核和 U-boot 命令行都能够配置 U-boot 环境变量，并且希望能够在线配置分区表。另外在 mmc 中的环境变量如果出现破坏，也能够恢复默认环境变量。有必要弄清 U-boot 环境变量的加载流程。

在完成重定向后，U-boot 会进行一系列初始化，这些初始化都是通过 `init_sequence_r` 这么一个初始化函数指针数组来进行的，按照其顺序 `board_init()` 之后就是 `initr_env_switch()` 。入口处源码：

```c
#ifdef CONFIG_USING_KERNEL_DTB
	initr_env_nowhere,
#endif
#if defined(CONFIG_BOARD_EARLY_INIT_R)
	board_early_init_r,
#endif

#if defined(CONFIG_ARM) || defined(CONFIG_NDS32) || defined(CONFIG_RISCV)
	board_init,	/* Setup chipselects */
#endif
#if defined(CONFIG_USING_KERNEL_DTB) && !defined(CONFIG_ENV_IS_NOWHERE)
	initr_env_switch,
#endif

...

#ifndef CONFIG_USING_KERNEL_DTB
	/* init before storage(for: devtype, devnum, ...) */
	initr_env,
#endif
```

可以注意到，初始化环境变量有两条路，根据是否配置了 `CONFIG_USING_KERNEL_DTB` 进行了分叉。

### 4.6.1. `CONFIG_USING_KERNEL_DTB` 带来的变化

`CONFIG_USING_KERNEL_DTB` 允许 U-bot 阶段直接加载内核设备树来获取例如 regulator，屏显，usb等复杂设备(uart，emmc等设备仍使用 U-boot 自己的设备树)。由于该配置的启用会导致 U-boot 初始化顺序有区别。由于 DTB 的切换需要从环境变量中获取内核 DTB 的加载地址，所以需要预加载默认环境变量。我们可以来看一看具体的代码，DTB 的切换发生在 `board_init()` 中：

```c
arch/arm/mach-rockchip/board.c

int board_init(void)
{
  #ifdef CONFIG_USING_KERNEL_DTB
	init_kernel_dtb();
  #endif
}
```

在 `init_kernel_dtb()` 中完成了内核设备树的初始化(这里只截取了最终要的代码部分)：

```c
arch/arm/mach-rockchip/kernel_dtb.c

int init_kernel_dtb(void)
{
  ...
  fdt_addr = env_get_ulong("fdt_addr_r", 16, 0);
  ...
  ret = rockchip_read_dtb_file((void *)fdt_addr);
}
```

可以看到这里需要使用到 `fdt_addr_r` 环境变量，因此需要预加载默认环境变量。否则只需要在后面直接 `initr_env()` 加载环境变量即可。而在 `initr_env()` 中会根据配置情况选择是否加载环境变量和从哪里加载。总之，**`CONFIG_USING_KERNEL_DTB` 导致了环境变量的加载变得复杂，原本的一步变为了两步**。

那我们接着来看一看使能了该配置后，这里的两步到底在干什么吧。第一步会进入 `initr_env_nowhere`。这里一般就是加载最小的默认环境变量，能够完成对内核设备树的加载。这里具体代码就不看了(先 mark 一下)。其实里面也并不复杂，就是从源码宏中取出了一些必要的环境变量进行组装，然后 `set_board_env()` -> `himport_r()` 来导入环境变量。这个 `himport_r()` 应该是导入环境变量的核心函数，所有的流程最后都会调用它来完成实现，具体作用应该就是将源码中的环境变量宏导入到一个哈希表中供 U-boot 命令行查找。

**这里很重要！**因为之后我想要修改内核，dtb 还有 rootfs 加载到 ddr 中的地址时，我只是通过 `setenv` + `saveenv` 并不能够让存储在 emmc 中的对应环境变量如 `fdt_addr_r` 还有 `kernel_addr_r` 等生效！reboot 之后这些变量还是会被覆盖成为默认值。最后发现其实就是这里在搞鬼。因为我们启用了 `CONFIG_USING_KERNEL_DTB` 所以进入了 `initr_env_nowhere()` 分支，这里面会从源码中获取 `ENV_MEM_LAYOUT_SETTINGS` 宏设置的相关环境变量，因为此时还未完成环境变量的完整加载，**只是提前取出了一些必要的环境变量使用**，在 `initr_env_switch()` 真正完成这条初始化链条的环境变量加载时，为了保持一致，会在加载完 emmc 中的环境变量后，**使用 `nowhere` 中预先使用过的环境变量覆盖目前的值**，这就导致我们设置的值被覆盖掉了。

在 `board_init()` 中完成了内核 DTB 加载后，如果用了存储设备储存环境变量，那么就会触发 `initr_env_switch()`，否则就会继续其他初始化，环境变量初始化到此为止。这里要求配置项 `CONFIG_ENV_IS_NOWHERE` 未配置。因为配置了该项，U-boot 就只会从源码中读取默认配置，虽然也可以通过 `setenv` 来配置环境变量，但是无法存储到 mmc 这类非易失性存储中，下一次上电后恢复为默认配置。

我们进入 `initr_env_switch()` 内部看看：

```c
static int initr_env_switch(void)
{
	ALLOC_CACHE_ALIGN_BUFFER(env_t, env_nowhere, 1);
	char *data;
	int ret;

	data = env_get("bootargs");
	if (data) {
		env_set("bootargs_tmp", data);
		env_set("bootargs", NULL);
	}

	/* Export nowhere env for late use */
	ret = env_export(env_nowhere);
	if (ret) {
		printf("%s: export nowhere env fail, ret=%d\n", __func__, ret);
		return -EINVAL;
	}

	/* Destroy nowhere env and import storage env */
	initr_env();

	/* Append/override nowhere env to storage env */
	set_board_env((char *)env_nowhere->data, ENV_SIZE, H_NOCLEAR, false);

	/*
	 * Restore nowhere bootargs to append/override the one in env storage.
	 *
	 * Without this, the entire "bootargs" in storage env is replaces by
	 * the one in env_nowhere->data.
	 */
	data = env_get("bootargs_tmp");
	if (data) {
		env_update("bootargs", data);
		env_set("bootargs_tmp", NULL);
	}

	return 0;
}
```

这一步干了三件事：备份现有的环境变量，导入存储介质中的环境变量以及合并。

`ret = env_export(env_nowhere)` 就是备份 `initr_env_nowhere()` 中配置的环境变量。`initr_env()` 就是回归经典加载流程，见下一小节。这里就是根据情况选择从 mmc 等存储介质中加载环境变量，或者使用默认环境变量。最后就是 `set_board_env()` 将一开始使用的 `env_nowhere()` 合并到现有的环境变量中去。

**注意**：这里加载的顺序很重要，如果从 mmc 中加载了环境变量，那么最后 `set_board_env()` 会**覆盖 mmc 中定义的同名环境变量**。不过可以注意到的是，在 `initr_env_nowhere()` 中，加载的环境变量较少，只是用了初始化内核 DTB 必要的一些环境变量，并非全量加载。所以在最后覆盖时，**基本不会涉及到自定义的环境变量**。

如果真的存在被覆盖的问题，也可以像 `initr_env_switch()` 中最后的：

```c
	/*
	 * Restore nowhere bootargs to append/override the one in env storage.
	 *
	 * Without this, the entire "bootargs" in storage env is replaces by
	 * the one in env_nowhere->data.
	 */
	data = env_get("bootargs_tmp");
	if (data) {
		env_update("bootargs", data);
		env_set("bootargs_tmp", NULL);
	}
```

这里就是在 mmc 中用 `bootargs_tmp` 进行了备份，避免 `set_board_env()` 将 mmc 的环境变量给覆盖了。




### 4.6.2. 经典加载流程

最简单的流程自然就是未配置 `CONFIG_USING_KERNEL_DTB` 的初始化了，就是直接调用 `initr_env`：

```c
static int initr_env(void)
{
	/* initialize environment */
	if (should_load_env())
		env_relocate();
	else
		set_default_env(NULL);
#ifdef CONFIG_OF_CONTROL
	env_set_addr("fdtcontroladdr", gd->fdt_blob);
#endif

	/* Initialize from environment */
	load_addr = env_get_ulong("loadaddr", 16, load_addr);

	return 0;
}
```

其中，通过 `should_load_env()` 来判断加载环境变量的具体情况：

```c
static int should_load_env(void)
{
#ifdef CONFIG_OF_CONTROL
	return fdtdec_get_config_int(gd->fdt_blob, "load-environment", 1);
#elif defined CONFIG_DELAY_ENVIRONMENT
	return 0;
#else
	return 1;
#endif
}
```

这里我们是进入了 `CONFIG_OF_CONTROL` 的分支，`fdtdec_get_config_int()` 会从 U-boot 设备树中寻找 `/config/load-environment` 的配置项读取对应值，也就是通过设备树来控制**是否从存储介质中的加载环境变量**，如果设备树未配置值，默认返回 `1`，也就是进入 `env_reloacate()`。



```c
void env_relocate(void)
{
#if defined(CONFIG_NEEDS_MANUAL_RELOC)
	env_reloc();
	env_htab.change_ok += gd->reloc_off;
#endif
	if (gd->env_valid == ENV_INVALID) {
#if defined(CONFIG_ENV_IS_NOWHERE) || defined(CONFIG_SPL_BUILD)
		/* Environment not changable */
		set_default_env(NULL);
#else
		bootstage_error(BOOTSTAGE_ID_NET_CHECKSUM);
		set_default_env("!bad CRC");
#endif
	} else {
		env_load();
	}
}
```

这里就是关键了，`CONFIG_NEEDS_MANUAL_RELOC` 忽略，检查 `gd->env_valid` 的值，该值应该是由之前的初始化配置，我们这里不深究。但是可以看到当存储介质中的环境变量存在错误时，就会尝试使用默认配置，根据具体情况还会上报错误。如果存储环境变量无误，就调用 `env_load()` 进行加载。

```c
int env_load(void)
{
	struct env_driver *drv = env_driver_lookup_default();
	int ret = 0;

	if (!drv)
		return -ENODEV;
	if (!drv->load)
		return 0;
	ret = drv->load();
	if (ret) {
		debug("%s: Environment failed to load (err=%d)\n", __func__,
		      ret);
		return ret;
	}

	return 0;
}
```

通过 `env_driver_lookup_default()` 查找对应的 env 设备驱动，然后调用 `drv->load()`。我们这里是 mmc，所以源码位于 `env/mmc.c`，函数指针 `load()` 指向 `env_mmc_load()`。该函数大体流程为从设备中读取环境变量，然后用 `env_import()` 导入环境变量到哈希表。其核心函数为 `himport_r()`。

## 4.7. U-boot 设备模型

U-boot 与 Kernel 有着类似的设备模型：

```
设备树节点 (fdt node)
        ↓ 解析
udevice (设备实例)
        ↓ 属于
uclass (设备类别)
        ↓ 由
driver (驱动) 来 probe / bind
```

### 4.7.1. 关键结构体

- **`udevice`**：设备实例，对应设备树中的一个节点
- **`uclass`**：设备类别容器，类似于 `kset`，一类设备的集合
- **`UCLASS_DRIVER`**：设备类别的总体定义（不是真正的驱动）
- **`U_BOOT_DRIVER`**：驱动定义，初始化 `struct driver` 结构体，描述驱动属于哪类设备（`uclass id`）、能匹配哪些节点（`of_match`）、驱动操作等

### 4.7.2. 初始化流程

U-boot 设备模型初始化主要是在 `board_init_r() → initcall_run_list(init_sequence_r) → initr_dm()`。在这里会扫描设备树节点，建立其设备模型的基本拓扑结构，并且**绑定**设备和对应驱动。这里的绑定只是建立关系，并不会访问硬件。简单来说就是把 `udevice->driver` 指针给填上。但是实际上其对应的 `probe` 函数还未调用。

需要到对应子系统初始化时（一般也在 `board_init_r()` 中），才调用 `probe` 真正初始化设备。

### 4.7.3. 以 GMAC 为例

```
initr_net()
  └─ eth_initialize()
      └─ uclass_first_device(UCLASS_ETH, &dev)
          └─ uclass_get_device_tail(dev, ret, devp)
              └─ device_probe(dev)
```

这是代码层面可以找到的，但是具体的 MAC 控制器和 PHY 芯片的驱动和设备是如何绑定和 probe 的，我其实还是会有一点点懵。

---

# 5. 案例：网口调试 (rk3568 GMAC)

## 5.1. 背景

目前手里的这块板子是基于 Rockchip 官方原理图，在某些芯片选型上作了修改的一块板子，因此官方的内核源码可能会在某些情况下不适配。另外，之后在实际板子上使用的可能是 `1000base-t1` 网口，硬件上又存在很大区别，因此这里只是作为一个练手，帮助我稍微了解一点网络驱动相关的知识。

先去了解一下 OSI 7 层模型，大致明白了开发板上的 PHY 芯片主要对应物理层，内置 GMAC 主要是数据链路层。我在调试 `rk3568-evb` 开发板时，进入内核后将开发板与 PC 用 RJ45 网线连接。板子使用 GMAC0 的 RGMII 接口。

> **PHY**：Physical Layer Transceiver，物理层收发器
>
> **GMAC**：Gigabit Ethernet MAC，千兆网媒体访问控制器

在使用默认设备树时，网线接好后网口灯能够亮。我再为我的 PC 和开发板分别使用 `ifconfig` 或者 `ip` 工具配置静态 ip 地址为：

```bash
192.168.1.100(主机)
192.168.1.101(开发板)
```

正常来说，这时我们直接就能够在终端相互 ping 通，但是失败了。

## 5.2. 排查过程

### 5.2.1. 基础检查

首先我们需要确认网络接口的名字和我们配置的静态 ip 是否对应。比如在主机端是 `eno1`，开发板是 `end0`，那么配置静态 ip 时需要配置到对应网卡设备上。

```bash
# 查看网口名和状态（注意是否有 UP 标记，若为 DOWN 则需启用）
ip link
ip addr
ifconfig
```

此外还需要确认对应网卡是否被启用，网卡状态栏中是否有 `UP`，如果是 `DOWN` 则：

```bash
ip link set end0 up
# 或者
ip link set eno1 up
```

然后需要检查 IP/子网配置，这里我们期望的掩码应该为 24 位。两侧掩码应一致。

### 5.2.2. 物理链路检查

- **网口灯**：绿灯常亮、橙灯闪烁表示物理链路正常
- **ethtool**：
  ```bash
  ethtool end0       # 基本链路状态、协商速率
  ethtool -S end0    # 网卡统计计数器（收发字节数、错误计数等，定位丢包层次）
  ```
- **内核日志**：
  ```bash
  dmesg | grep -i eth
  dmesg | grep -i rk_gmac
  dmesg | grep -i gmac
  dmesg | grep -i phy
  ```

另外，在 PC 这边最好是关闭防火墙，我使用的是 Ubuntu，因此可以直接：

```bash
sudo ufw disable
```

### 5.2.3. ARP 测试

```bash
# PC 端
ping 192.168.1.101
arp -n
```

**ARP 结果的诊断逻辑**：
- PC 端 `arp -n` 能查到开发板 MAC 地址，但 ping 不通 → **IP 层/防火墙问题**
- PC 端 `arp -n` 什么都看不到，但开发板能收到 PC 的 ARP 请求 → **二层就没通（驱动/PHY/网线）**

排查时先在开发板端抓包确认：

```bash
# 开发板端抓包
tcpdump -i end0 arp
```

**结论**：我按照上述方式进行了测试，发现先 ping 后稍等一下 arp 查看，在开发板端能够看到 PC 的 MAC 地址，但是主机这边 ping 后 arp 什么也看不到。通过 `ping` + `sudo tcpdump -i <网卡> arp` 来抓取信息发现也是同样的结果。开发板发出了请求，但是主机无法接收。

**说明开发板的 TX 出现了问题！单向通信，TX 通路有问题。**

### 5.2.4. 降速测试

我们通过 `ethtool` 工具让网口强制降速来进行测试。首先查看当前网卡的配置，它是自动协商的速率：

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

强制降速到百兆：

```bash
ethtool -s end0 speed 100 duplex full autoneg off
```

降速到百兆后可以 ping 通，但**丢包率超过 50%**。强制千兆则不通。

也可以关闭 `autoneg`，强制设置速率为 `1000`，看看是否是自动协商的问题。测试发现，**自动协商没问题，是 RGMII 时序问题**——百兆对时序要求降低，所以勉强能通。

### 5.2.5. PHY Mode 测试

通过修改设备树中 GMAC 节点的 `phy-mode` 属性来测试内部延时配置。原本的模式为 `rgmii`，无内部延时：

| phy-mode | 含义 | 结果 |
|:---|:---|:---|
| `rgmii` | 无内部延时（MAC 和 PHY 都不加） | 百兆可通但丢包严重，千兆不通 |
| `rgmii-id` | 收发两端均加内部延时 | 不通 |
| `rgmii-txid` | 仅发送端加内部延时 | 百兆下也无法 ARP |

不过我尝试调大和调小 `tx_delay` 的值，都没有成功调通...

另外可通过 `ethtool` 输出判断自动协商是否正常——关注 `Link partner advertised link modes` 和 `master-slave status` 字段。如果自动协商已成功协商到千兆全双工，但 ping 不通，问题大概率在 RGMII 时序层面。

同时尝试匹配两端流控：

```bash
# PC 端
sudo ethtool -A eno1 rx off tx off
# 开发板端
ethtool -A end0 rx off tx off
```

流控关闭后问题依旧，进一步确认是 PHY TX 通路的硬件时序问题。

### 5.2.6. 发包工具验证

除了 ARP + ping 外，还可以用发包工具进一步隔离 RX/TX 问题：

**验证 RX（PC → 开发板）**：在 Windows 端使用 `anysend` 等发包工具发送 ARP 报文到开发板，开发板端用 tcpdump 确认收到：

```bash
# 查看 ARP 报文的详细内容（含十六进制 dump）
tcpdump -i end0 -XXX arp
```

能够成功收到报文，说明开发板 RX 通路正常。

**验证 TX（开发板 → PC）**：将 PC 发来的 ARP 消息通过 tcpdump 保存为 pcap 文件，然后用 `tcpreplay` 重放给 PC——以此排除上层协议栈的干扰，单纯验证链路层 TX：

```bash
# 开发板端抓取 PC 发来的 ARP 包（注意先启动抓包，再开始 ping，Ctrl+C 退出后才写入 pcap）
tcpdump -i end0 -w /tmp/rx_arp.pcap arp

# 用 tcpreplay 重放给 PC（--pps=1 每秒发1个包，便于观察）
tcpreplay -i end0 --pps=1 /tmp/rx_arp.pcap
```

`tcpreplay` 不需要关心 IP 地址，只要网口物理连通即可。PC 端完全收不到 → **TX 通路确认有问题**。

## 5.3. 结论

定位到 PHY 芯片的硬件电路在 TX 相关电路上存在问题（RGMII 发送时序不满足千兆要求）。对于百兆，时序要求降低所以能通但丢包严重。最终因硬件问题搁置。

---

# 6. 附录：常用调试命令速查

```bash
# 查看内核日志中的网络相关信息
dmesg | grep -i eth
dmesg | grep -i gmac
dmesg | grep -i phy
dmesg | grep -i rk_gmac

# 查看网卡详细状态
ethtool end0
ethtool -S end0

# 强制设置速率/双工
ethtool -s end0 speed 100 duplex full autoneg off
ethtool -s end0 speed 1000 duplex full autoneg off

# 关闭流控
ethtool -A end0 rx off tx off

# 关闭 PC 端防火墙 (Ubuntu)
sudo ufw disable

# 抓包分析
tcpdump -i end0 -XXX arp
tcpdump -i end0 -w /tmp/capture.pcap arp

# 重放 pcap 文件发包
tcpreplay -i end0 --pps=1 /tmp/capture.pcap
```
