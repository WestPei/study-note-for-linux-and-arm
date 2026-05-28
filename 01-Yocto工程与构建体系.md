# Yocto 工程与构建体系

本文档聚焦 Yocto 构建系统本身——从核心概念到工程管理，涵盖任务链、变量机制、工程结构规范、SVN 集成以及开发环境配置。

---

# 1. Yocto 核心概念

Yocto 是个开源项目，用来构建自定义的嵌入式 Linux 系统的，它是一系列工具的集合。其核心组件包括：

- **BitBake**：Yocto 的**任务执行引擎**，负责解析元数据并执行构建任务。
- **OpenEmbedded Core**：提供基础的元数据（如软件包配方和类），它是 Yocto 的**核心**。
- **元数据 (Metadata)**：包括配置文件、软件包配方 (recipes) 和类 (classes)，定义了如何构建系统。
- **镜像生成**：生成最终的 Linux 镜像文件，包括内核、根文件系统和用户空间工具。
- **层 (Layer)**：元数据的文件夹，将不同功能或不同硬件的元数据分离开，便于管理和复用。

## 1.1 配方 (.bb / .bbappend)

**配方**是 Yocto 中最关键的概念，一般是以 `.bb` 或 `.bbappend` 为扩展名的文件。它描述如何构建一个软件包——应用程序、内核、甚至镜像本身都可以是一个配方。

构建镜像时使用 `bitbake core-image-minimal` 这样的命令，`core-image-minimal.bb` 就是一个镜像配方，`bitbake` 根据该配方来决定哪些软件包需要添加到镜像中。

配方的通用存放路径为：

```
<层目录>/recipes-<分类>/<软件名>/<软件名>-<版本号>.bb
```

## 1.2 配置文件 (.conf)

配置文件与配方息息相关，大到对整个 Yocto 工程的配置，小到对一个应用程序的配置都是通过 `.conf` 文件实现的。几个比较重要的 `.conf` 文件：

- **`bblayers.conf`**（位于 `build/conf/`）：整个工程"层的开关"，只有加入其中的层才会被 `bitbake` 扫描和使用。在该文件中声明的层包含的各种配方在构建时会被 `bitbake` 自动发现。注意：启动层 ≠ 构建镜像时会包含，具体取决于镜像配方是否包含该配方或其依赖。

- **`local.conf`**（位于 `build/conf/`）：当前构建目录的**核心配置**，优先级最高。用于定义目标硬件 (`MACHINE`)、限制构建资源、控制编译优化等本地个性化参数。

- **`layer.conf`**（位于每个层的 `conf/` 目录下）：定义层自身的属性和默认配置，包括层的优先级、元数据路径、层的依赖等。官方层无需修改，自定义层需自行编写。

此外还有机器配置 `machine/<机器名>.conf` 和发行版配置 `distro/<发行版名>.conf`，详见第 5 节。

### 配方与配置文件的关系

`.conf` 类似于"环境变量"，定义了全局可见的参数；而配方 `.bb` 就像是脚本，运行在这些环境变量下，可以使用也可以修改这些环境变量。

**注意**：`.conf` 中定义的变量，默认对整个 Yocto 工程可见。只是说不同的 `.conf` 文件的优先级不同，因此定义同一个变量会存在优先级覆盖的问题。

## 1.3 层 (Layer)

用来管理元数据的文件夹，命名通常是 `meta-*`，例如 Poky 的核心层 `meta`、ST 官方的 BSP 层 `meta-st-stm32mp`、Rockchip 的 `meta-rockchip`，或自定义的 `meta-mylayer`。

## 1.4 构建目录 (build/)

初始化环境时运行 `source oe-init-build-env build` 会创建构建目录。**所有 `bitbake` 命令都应该在该目录下执行**。配方和层的定位由 `conf/bblayers.conf` 中定义的路径决定。

---

# 2. 构建流程与任务链

## 2.1 完整构建流程

整个构建流程可以概括为 **解析配置 → 执行任务 → 生成输出**：

```
bitbake core-image-minimal
  → 解析配置与配方 (bitbake.conf, 层配置, 依赖关系)
  → 执行任务链 (Fetch → Extract → Configure → Compile → Install → Package)
  → 打包镜像 (分析依赖, 组装根文件系统, 生成最终镜像)
```

## 2.2 默认任务链详解

Yocto 配方的默认任务链：

```
1. do_fetch
2. do_unpack
3. do_patch
4. do_configure
5. do_compile
6. do_install
7. do_package
8. do_rootfs
```

> **注意**：完整的标准任务链末尾还有 `do_deploy`，负责将构建产物（如内核镜像、设备树）部署到 `${DEPLOY_DIR_IMAGE}` 目录。`do_rootfs` 和 `do_deploy` 之间还有 `do_image` 系列任务（如 `do_image_ext4`），负责将根文件系统内容打包为特定格式的镜像。可以用 `bitbake <recipe> -c listtasks` 查看某个配方的完整任务列表，用 `bitbake <recipe> -g` 生成任务依赖图（`task1 -> task2` 表示 task2 完成后才能执行 task1）。

现在来分析每一个任务的细节。

### do_fetch — 获取源码/补丁

- **执行目录**：无固定目录
- **核心操作**：从 `SRC_URI` 定义的地址 (HTTP/GIT/SVN/本地文件) 下载源码包、补丁、配置文件等，并进行校验
- **输出目录**：`${DL_DIR}` — 存放源码包、Git 仓库、补丁文件
- **文件形态**：原始压缩包/裸 Git 仓库，未解压、未修改
- **关键变量**：`DL_DIR`、`SRC_URI`、`SRCREV`

### do_unpack — 解压/检出源码

- **执行目录**：`${WORKDIR}`
- **核心操作**：
  - 压缩包：解压到 `${S}`
  - Git：从 `${DL_DIR}` 的裸仓库检出源码到 `${S}`
  - 补丁/本地文件：复制到 `${WORKDIR}`
- **输出目录**：`${S}` — 源码目录 (.c/.h/Makefile/configure 等)
- **文件形态**：解压后的可读源码
- **关键变量**：`S`、`WORKDIR`、`SRC_URI`

### do_patch — 打补丁

- **执行目录**：`${S}` — 补丁直接打到源码中
- **核心操作**：
  - 从 `FILESPATH` (层的 `files` 目录) 或 `${DL_DIR}` 读取补丁
  - 按 `SRC_URI` 顺序应用补丁 (后者覆盖前者)
  - 支持自定义 `do_patch_append` 扩展
- **输出目录**：`${S}`
- **文件形态**：打补丁后的源码
- **关键变量**：`S`、`FILESPATH`、`PATCHTOOL` (默认 `patch`)

### do_configure — 配置编译参数

- **执行目录**：`${B}` — 编译目录
- **核心操作**（针对 Kernel 和 U-boot）：
  - 加载基础配置，生成 `${B}/.config`
  - 合并 `.cfg` 文件，叠加到 `${B}/.config` 上
  - 补全默认配置，自动补全 `.config` 中未定义的配置项
- **输出目录**：`${B}`
- **文件形态**：基础 `defconfig` + `cfg` 合成 `.config`
- **关键变量**：`KBUILD_DEFCONFIG` (默认使用的配置文件)

### do_compile — 编译

- **执行目录**：`${B}`
- **核心操作**：执行 `make`，编译源码生成目标文件 (.o)、可执行文件、库 (.so/.a)
- **输出目录**：`${B}` 下的编译产物（含中间产物）
- **文件形态**：二进制文件
- **关键变量**：`B`、`CC` (编译器)

### do_install — 安装到伪根文件系统

- **执行目录**：`${B}`
- **核心操作**：
  - 执行 `make install`，将编译产物安装到 `${D}` 下
  - 自定义安装：也可以手动复制文件
- **输出目录**：`${D}` — 按照文件系统结构组织的文件
- **文件形态**：按目标系统目录结构部署的文件（与最终 rootfs 目录一致）
- **关键变量**：`D`、`prefix`

### do_package / do_rootfs — 打包与生成镜像

- `do_package`：在 `${WORKDIR}` 下解析 `${D}` 中的文件并打包为 `.ipk`
- `do_rootfs`：将所有选中的包安装到 `${IMAGE_ROOTFS}`，生成完整的根文件系统，最终打包为镜像输出到 `${DEPLOY_DIR_IMAGE}`

这里的变量都可以通过 `bitbake -e` 进行查询，不过比较麻烦的是可能我们并不知道这些变量具体在哪里定义的。

### do_install 之后为什么还需要 do_package？

`do_install` 其实就是在组织 `do_compile` 得到的二进制文件、配置文件和库文件，按照**目标系统的最终路径**，复制到一个临时安装目录——这一步似乎就实现了安装，为什么还需要 `do_package`？

`do_package` 将软件再打包成 `.ipk` 包，来供目标系统的根文件系统识别和使用。如果直接把软件安装的目录直接给根文件系统，其实存在大量问题，比如软件的**依赖关系、文件的权限、软件可能有特殊的安装/卸载脚本、版本信息**等等一系列元信息，根文件系统无法知晓。而 `do_package` 这一步在打包时，就会将这些元信息包含进去，实际上进行了一个**规范化/标准化**。如果每一个软件都直接将自己的可执行文件目录拷贝给根文件系统，根文件系统就完全无法管理这些软件。

### do_rootfs 干了什么？

在 `local.conf` 或镜像配方（比如 `core-image-minimal`）中，会定义生成内核镜像所需的各类软件包（`IMAGE_INSTALL`）。在构建时 `do_rootfs` 就会去找这些软件包的 `.ipk`（统一且规范），将这些 `.ipk` 软件包解压到根文件系统目录中。Yocto 会创建一个最终根文件系统的临时目录 `${IMAGE_ROOTFS}`，把所有 `.ipk` 按照规范解压到该目录下。这样做就会让目标系统启动时就能够识别到这些软件（类似于预装软件）。

在这之后 `do_rootfs` 还会进行一些收尾工作：

- 生成 `/etc/fstab`、`/etc/inittab`
- 处理用户组（创建软件需要的用户/组）
- 清理临时文件、压缩根文件系统（比如生成 `rootfs.ext3`、`rootfs.cpio` 等镜像格式）

最终 `${IMAGE_ROOTFS}` 目录就是"目标系统的完整根目录"：里面包含了所有软件的二进制、库、配置文件、系统工具等。

另外，**根文件系统的内容和格式是分离的**。`do_rootfs` 处理了根文件系统的内容，`do_image` 利用 `IMAGE_FSTYPES` 来确定文件系统的最终镜像格式。这样我们的根文件系统内容只需处理一次，后续如果要部署到不同平台，只需更改根文件系统的格式就好。

---

# 3. 关键目录与变量

## 3.1 四个核心目录变量

Yocto 中每个配方都围绕四个核心变量展开，它们决定了构建时的工作空间和文件流向：

| 变量 | 含义 | 默认值 |
|:---|:---|:---|
| `WORKDIR` | 配方的工作目录 | `build/tmp/work/${MULTIMACH_TARGET_SYS}/${PN}/${EXTENDPE}${PV}-${PR}` |
| `S` | 源码目录（解压/检出后的源码） | `${WORKDIR}/${PN}-${PV}` (压缩包) 或 `${WORKDIR}/git` (Git 源码) |
| `B` | 编译目录（Out-of-Tree 编译） | `${WORKDIR}/build` |
| `D` | 安装目录（伪根文件系统） | `${WORKDIR}/image` |

> `WORKDIR`：配方的工作目录
>
> `S`：源码目录，存放解压、检出后的原始（打补丁后）源码
>
> `B`：编译目录 (Out-of-Tree 编译)，存放配置/编译的中间产物；也有直接在源码下编译的 `${S}` (In-Tree 编译)
>
> `D`：安装目录 (伪根文件系统)，编译产物先安装到这里，是打包的基础

这些变量的基础定义在 `poky/meta/conf/bitbake.conf` 中。查询方法：

```bash
bitbake <my-recipe> -e | grep "^S=\|^D=\|^B=\|^WORKDIR="
```

`^` 是正则表达式的行首锚点，确保只匹配**已生效的**变量定义（而非被注释或覆盖的中间值）。

**典型路径模式**（以 U-boot 为例）：

```
build/tmp/work/<machine>-poky-linux-gnueabi/u-boot/<version>/git/    # S — 源码
build/tmp/work/<machine>-poky-linux-gnueabi/u-boot/<version>/build/  # B — 编译目录
```

要快速定位到某个配方展开后的源码目录，还可单独查询：

```bash
bitbake -e u-boot | grep ^WORKDIR=
bitbake -e <my-recipe> | grep ^S=
```

要定位某个配方文件在哪个层中：

```bash
find sources/meta-<bsp>/ -name "*.bb" | grep <machine>
```

## 3.2 变量与赋值操作符

在 Yocto 和 Makefile 中都有变量与赋值，而赋值的手段不仅仅只有 `=`，还有一些变种：

| 操作符 | 含义 | 特点 |
|:---|:---|:---|
| `=` | 基本赋值（延迟展开） | 延迟展开，可被覆盖 |
| `:=` | 立即赋值 | 立即展开，内容固定 |
| `?=` | 条件赋值 | 仅当变量未定义时赋值，**解析时立即生效** |
| `??=` | 弱默认值 | 仅当变量**最终未定义**时才赋值，**解析结束时才生效** |
| `+=` | 立即追加 | 立即执行，可能被覆盖 |
| `=+` | 前置追加 | 在现有值前添加 |
| `:append` | 延迟追加 | 解析完成后追加，**不会被覆盖** |
| `:prepend` | 延迟前置 | 解析完成后前置，**不会被覆盖** |

**关键区别**：`+=` 可以被后续的 `=` 覆盖，而 `:append` / `:prepend` 在所有解析完成后才生效，因此不会被覆盖。这就是为什么在 `.bbappend` 中用 `=` 覆盖 `SRC_URI` 时，原配方中的 `SRC_URI +=` 会丢失，需要用 `:append` 来保证追加的内容不被覆盖。

条件语法：`VARIABLE:operation:condition = "value"`，如 `SRC_URI:append:arm = " ..."` 表示仅在 arm 架构下追加。bitbake 严格遵循这一语法：

```bash
VARIABLE:operation:condition1:condition2... = "value"
```

一般在 `local.conf` 中不推荐使用 `??=`，这个通常位于发行版或基础层 `.conf` 中用于**保底**（例如 `qemux86-64`），来确保在任何情况下变量都有一个定义。在 `local.conf` 中，一般比较常见的用法是 `?=`，既能够提供我们想要的默认值，又能够允许其他机制来覆盖它。

注意 `.conf` 文件在工程全局和每一个层中都有，会存在覆盖的问题，因此有优先级。**后解析的层赋值会覆盖先解析的值**（当然也看具体的赋值语句）。

## 3.3 任务标记 (Varflags)

用于对任务行为进行标记或重定义：

```bash
do_<task_name>[Varflags] = "1"
```

| 标记名 | 作用 |
|:---|:---|
| `noexec` | **不执行该任务**。任务保留依赖关系，但内容不运行（相当于空任务）。常用于 placeholder 或禁用默认行为 |
| `nostamp` | **不创建 stamp 文件** → BitBake 总是认为任务"未执行"，因此每次都执行 |
| `network` | 允许任务访问网络（默认只有 fetch 允许） |
| `lockfiles` | 为任务设置锁文件，使多个任务互斥执行 |
| `number_threads` | 限制任务运行时使用的线程数（控制并行） |
| `dirs` | 任务开始前需要创建的目录列表 |
| `depends` | 为任务添加额外依赖 |

常用场景：跳过补丁任务 `do_patch[noexec] = "1"`（当补丁已固化进源码时）。

## 3.4 任务修改

bitbake 有着标准的任务链，如果在配方中没有作改动，那么在构建该配方时就会按照下列顺序执行标准任务：

```
do_fetch    →  do_unpack  →  do_patch  →  do_configure  →  do_compile  →  do_install  →  do_package  →  do_deploy
下载源码        解压源码       应用补丁      配置项目         编译代码        安装文件       打包文件        部署到最终目录
```

在配方中我们可以对这些标准任务进行修改，使用操作符在标准任务前后添加操作，甚至直接编写完整的任务来覆盖标准任务：

```bash
# 在标准任务前后添加自定义操作
do_configure:prepend() {
    # 在 configure 前执行的命令
}
do_compile:append() {
    # 在 compile 后执行的命令
}

# 自定义任务并插入到任务链中
python do_custom_task() {}
addtask custom_task before do_configure after do_patch
```

---

# 4. virtual/kernel 等虚拟目标机制

在实际使用 `bitbake` 构建镜像时，有一个特殊的用法：我们可以使用 `virtual/kernel` 来构建内核镜像，`virtual/bootloader` 来构建引导程序的镜像。这些是什么？

## 4.1 什么是虚拟目标

`virtual/kernel`、`virtual/bootloader` 是 **虚拟目标**——抽象的标识符，代表了"为当前这台机器构建的内核/引导程序"这一需求。`MACHINE` 变量决定虚拟目标指向哪个具体配方。

使用虚拟目标可以提高抽象程度和可移植性，无需硬编码具体配方名。不同的硬件平台（BSP 层）都可以提供它们自己的、实际的内核配方，通过依赖 `virtual/kernel`，构建系统能够根据当前项目的机器配置和启用的层，来自动选择合适的内核配方。

## 4.2 Provider 模式

虚拟目标的工作机制是"提供者 (Provider) 模式"。构建系统解析到虚拟目标时，由 `PREFERRED_PROVIDER` 变量控制谁来实现：

```bash
PREFERRED_PROVIDER_virtual/kernel = "linux-stm32mp"
```

这通常在对应 `MACHINE` 的配置文件或顶层 `local.conf` 中定义。`MACHINE` 一般由厂商的 `BSP` 层提供，比如 `meta-st-stm32mp/conf/machine/` 目录下就会有 `<machine>.conf`。

另外还需要注意的是，这行配置可能不一定会出现在 `<machine>.conf` 配置文件中，该文件可能会包含其他的 `*.inc` 文件，这可能是为了通用性而设置的，比如 `stm32mp1` 系列和 `stm32mp2` 系列可能共用一些配置等。

## 4.3 解析流程

解析核心是一套"提供者 (Provider)"和"偏好 (Preference)"系统。具体的解析流程如下：

```
构建系统遇到对 virtual/kernel 的需求
  → 收集所有在其 PROVIDES 变量中声明了 virtual/kernel 的配方
  → 有多个提供者？
      是 → 读取 PREFERRED_PROVIDER_virtual/kernel 变量的值（如 linux-stm32mp）
      否 → 直接使用唯一的提供者
  → 在 PROVIDES 包含 virtual/kernel 的配方中，锁定具体配方名（如 linux-stm32mp）
  → 在锁定的配方名下，选择 PV（版本号）最高的配方文件
  → 应用 PREFERRED_VERSION 规则（如有设置则覆盖默认选择）
  → 确定最终要构建的具体配方文件（如 linux-stm32mp_6.6.bb）
```

这里有一个关键问题：虚拟目标指向的配方为 `linux-stm32mp`，但通过 `find sources/meta-st-stm32mp -name "linux-stm32mp*.bb"` 找到的具体配方名为 `linux-stm32mp_6.6.bb`，后面会有一个版本号。它是如何将这两个对应上的？如果有多个不同的版本存在，那么如何确定使用哪一个版本呢？

### 连接机制：PROVIDES 变量

配方的 `PROVIDES` 变量是实现连接的关键。一个配方会通过这个变量声明"我能满足哪些依赖需求"：

- **隐式声明**：每个配方会自动提供与其文件名（`PN`，即包名）同名的依赖。例如，`linux-stm32mp_6.6.bb` 隐式地提供了 `linux-stm32mp`。
- **显式声明**：内核配方会显式声明 `PROVIDES += "virtual/kernel"`，表明它有能力满足系统对"一个内核"的抽象需求。U-Boot 配方也是类似的道理，通常会声明 `PROVIDES += "virtual/bootloader"`。

这样，当 bitbake 遇到 `virtual/kernel` 这个依赖时，它就知道所有声明了此功能的配方（如 `linux-stm32mp`、`linux-yocto`）都是候选者。

### 选择机制：PREFERRED_PROVIDER 与版本规则

当有多个候选者时，就需要一个选择标准：

- **指定提供者**：`PREFERRED_PROVIDER_virtual/kernel = "linux-stm32mp"` 明确告诉 bitbake："在众多能满足 `virtual/kernel` 的配方里，我**首选** `linux-stm32mp`"。这个配置通常在机器配置（`.conf`）文件中定义。
- **处理同名配方（多版本）**：对于同名但版本不同的配方（如 `linux-stm32mp_5.4.bb`、`linux-stm32mp_6.6.bb`），bitbake 有清晰的规则：
  1. **默认选择最高版本**：在没有特殊配置的情况下，bitbake 默认选择 PV（配方版本）最高的那个。
  2. **使用 `PREFERRED_VERSION` 干预**：可以通过 `PREFERRED_VERSION_linux-stm32mp = "5.4%"` 明确指定使用某个主要版本（支持通配符 `%`），覆盖默认的最高版本选择。
  3. **使用 `DEFAULT_PREFERENCE` 标记**：配方开发者可以通过设置 `DEFAULT_PREFERENCE = "-1"` 来标记某个配方（如开发版、不稳定版）不被优先选择，除非被 `PREFERRED_VERSION` 明确指定。

整个过程是一个双向选择：配置文件提出**需求**（设定谁是 `virtual/kernel`），而配方文件提供**实现**（声明我能做什么）。

## 4.4 确定 virtual/* 指向的配方

在构建目录下，可以通过：

```bash
bitbake -e virtual/kernel | grep ^PREFERRED_PROVIDER
```

来查看 `virtual/kernel` 的详细环境变量，这里就会包括提供者的信息。

也可以更为精准：

```bash
bitbake -e virtual/kernel | grep ^PROVIDES=
```

`bitbake -e` 用于查看 Bitbake 的环境变量。该命令会解析该目标或配方相关的所有元数据（.bb 文件，.conf 文件，.bbclass 类文件等），然后将**解析完成后生效的所有变量及其值**打印到标准输出。因此通过 `bitbake -e * | grep *` 命令可以去筛选对应目标的一些生效的变量。`grep ^PREFERRED_PROVIDER` 中的 `^` 是一个正则表达式，表示**一行的开始**，这样只会匹配以字符串 `PREFERRED_PROVIDER` 开头的行，这也表示**生效的行**。

## 4.5 配方如何声明自己是 virtual/kernel

配方需要在 `.bb` 文件中显式声明（或通过继承的类间接声明）：

```bash
PROVIDES += "virtual/kernel"
```

该声明通常位于 `poky/meta/classes-recipe/kernel.bbclass` 中，内核配方通过 `inherit kernel` 自动获得。

现在我们来从 `virtual/kernel` 反向找到一个镜像配方。可以通过 `grep` 命令来查找 `PREFERRED_PROVIDER_virtual/kernel`：

```bash
grep -r "PREFERRED_PROVIDER_virtual/kernel" conf/ ../sources/meta-*/
```

主要是在 `build/conf` 和 `sources/meta-*` 中去查找。最终可能会定位到某个 `.inc` 文件，该文件很可能被其他文件给 `include`。我们一般会在 `local.conf` 中配置硬件设备 `MACHINE`，找到对应的 `.conf` 配置文件后发现的确在里面 `include` 了该文件。

一个配方要成为 `virtual/kernel` 需要满足：

- 配置文件 `.conf` 中使用 `PREFERRED_PROVIDER` 设置了它
- 配方中自己也使用 `PROVIDES += "virtual/kernel"` 声明可以作为 `virtual/kernel` 的实现

## 4.6 查找具体配方文件位置

```bash
bitbake -e linux-stm32mp | grep ^FILE=
```

结果就会显示该配方的路径信息。

由于 `bitbake` 并不支持很多文件查找的命令，这里给出一些比较好用的查找命令：

- `find 搜索目录 -name "文件名"`：在目录下查找对应的文件
- `grep -r "搜索内容" 搜索目录 --include="文件类型过滤"`：递归搜索包含特定内容的文件

---

# 5. 工程结构规范

## 5.1 标准 Yocto 工程结构

如何去组织一个 Yocto 工程？在 quick-build 中如果按照默认选项进行操作，那么所有的文件和资源都会被放在 `poky/` 下，而 `poky/` 本身应该是作为 Yocto 工程的核心层，而不是根目录。因此需要调整我们的工程结构。

```
工程根目录/
├── build/                  # 构建目录
│   ├── conf/
│   │   ├── local.conf
│   │   └── bblayers.conf
│   └── init-build-env      # 自定义初始化脚本
└── sources/
    ├── meta-mylayer/       # 自定义层
    ├── meta-rockchip/      # 厂商 BSP 层
    ├── meta-qt6/           # 第三方软件层
    └── poky/               # Yocto 核心层
```

一般厂商提供的和官方给的一些软件层都会放在 `sources/` 目录下，比如 `meta-rockchip`，`meta-qt6` 等等。根据需求我们会对这些官方的配方进行修改或添加，但是为了可维护性，我们通常在 `sources/` 目录下单独创建一个自己的层来存放修改的文件。

## 5.2 自定义层的组织结构

按照软件室规范，自定义层的结构为：

```
meta-cetca/
├── meta-etu/               # 项目层 (非 Yocto 意义的层)
│   ├── meta-bsp/           # BSP 层 (Yocto 层)
│   │   ├── conf/
│   │   │   ├── machine/    # 机器配置
│   │   │   └── layer.conf
│   │   ├── recipes-bsp/    # U-boot、固件等
│   │   ├── recipes-core/   # 核心系统组件
│   │   └── recipes-kernel/ # 内核
│   └── meta-sdk/           # SDK/发行版层 (Yocto 层)
│       ├── conf/
│       │   ├── distro/     # 发行版配置
│       │   └── layer.conf
│       └── recipes-cetca/  # 自定义软件包
└── meta-avod/              # 另一个项目
    └── ...
```

SVN 将自定义层单独放置在一个目录下进行管理，并有着相对统一的组织结构。根目录就是自定义层 `meta-cetca`，然后在其中我们按照不同的项目分别创建各自的目录，按照项目名称来，比如 `meta-etu` 或者 `meta-avod`。

一般情况下我们会去改动内核和 U-boot 的源码，并且会自定义生成可以烧录的系统镜像以及其中的开发工具。因此每一个项目的层中主要有：

> - `meta-bsp`：包含 kernel，u-boot 以及与硬件紧密相关的修改后的配方，还会存放 machine 配置文件
> - `meta-sdk`：包含发行版配置，与 SDK 相关的配方，比如后续的 rootfs 以及大镜像的配方都放在这里

**注意**：`meta-cetca` 和 `meta-<项目名>` 本身不是 Yocto 意义上的"层"（没有 `conf/layer.conf`），只有里面的 `meta-bsp`、`meta-sdk` 才是真正的层。在 `bblayers.conf` 中应添加最内层的路径，如 `meta-cetca/meta-avod/meta-bsp/`。

另外注意结构也需要和硬件厂商的结构一致，保证能够找到。比如我们基于 `meta-rockchip/recipes-bsp/u-boot/u-boot-rockchip.bb` 修改，那么我们的 `.bbappend` 文件结构就是 `meta-cetca/meta-bsp/recipes-bsp/u-boot/u-boot-rockchip.bbappend`。

## 5.3 meta-bsp vs meta-sdk vs local.conf

在一个 Yocto 工程中，存在很多个不同的 `.conf` 文件。虽然都是配置文件，但是其**作用范围、设计目的和管理优先级**上都有显著区别。

优先级从低到高依次为：

1. **基础配置 (OE-Core)**：`poky/meta/conf/` 下的全局默认配置，提供最底层的默认值
2. **发行版层 (Distro Layer)**：`meta-sdk/conf/distro/xx.conf`，定义镜像的"风味"——构建的完整镜像会包含哪些软件包，实现什么样的功能
3. **BSP 层 (BSP Layer)**：`meta-bsp/conf/machine/xx.conf`，告诉 Yocto 工程"系统要在什么样的硬件上运行"，负责内核、启动加载、设备树等与硬件强相关的配置
4. **用户配置 (User Conf)**：`build/conf/local.conf`，**优先级最高**，允许开发者在不修改项目核心元数据的情况下，临时调整构建参数

这种配置分层实现了**解耦和复用**：镜像用途 (sdk) 和硬件适配 (bsp) 可以完全解耦，同一个发行版配置可以适配多种不同的硬件，反之亦然。

| 配置维度 | `meta-bsp/conf/machine/xx.conf` (机器配置) | `meta-sdk/conf/distro/xx.conf` (发行版配置) | `build/conf/local.conf` (本地构建配置) |
|:---|:---|:---|:---|
| **核心职责** | 定义硬件特定参数：CPU 架构、内核、启动加载器、设备树 | 定义发行版策略和通用软件栈选择：包管理、C 库、文件系统布局、全局特性 | 单次构建的个性化设置和本地环境选项 |
| **作用范围** | 特定于一种或几种机器 | 全局性，影响所有基于该发行版的机器 | 仅对当前构建目录有效 |
| **设计初衷** | 为特定硬件平台提供支持，可与不同的发行版搭配 | 为一个产品系列或项目定义统一的软件标准和策略 | 本地定制、临时调整，覆盖全局设置进行本地调试 |
| **优先级** | 中等，可被 local.conf 覆盖 | 中等，可被 local.conf 覆盖 | **最高** |
| **版本控制** | 强烈建议纳入版本控制，作为 BSP 层的一部分 | 强烈建议纳入版本控制，作为项目资产 | 通常不纳入版本控制 |

### 我应该修改哪个配置文件？

- 放入 **distro**：当配置代表**整个项目统一的、战略性的软件策略**时（如：所有产品都使用 `systemd` 作为初始化系统，或统一使用 `opkg` 作为包管理器）
- 放入 **machine**：当配置**直接依赖于硬件特性**时（如：内核版本、U-Boot 配置文件、GPU 驱动类型、启动分区布局）
- 放入 **local.conf**：当配置**纯粹为本地开发调试**时（如：禁用安全检查、设置代理）

比如我们接下来可能会涉及到的，修改 Yocto 工程的源码获取方式，从原来的联网下载或者本地缓存，变为从 SVN 仓库中拉取。这种配置显然不涉及硬件，那么就应该放入 `distro` 中，而事实上前辈也是如此做的。

## 5.4 层配置文件 (layer.conf) 示例

```bash
# meta-bsp/conf/layer.conf
BBPATH .= ":${LAYERDIR}"
BBFILES += "${LAYERDIR}/recipes-*/*/*.bb \
            ${LAYERDIR}/recipes-*/*/*.bbappend"
BBFILE_COLLECTIONS += "cetca-bsp"
BBFILE_PATTERN_cetca-bsp := "^${LAYERDIR}/"
BBFILE_PRIORITY_cetca-bsp = "10"
LAYERDEPENDS_cetca-bsp = "core openembedded-layer rockchip"
LAYERSERIES_COMPAT_cetca-bsp = "scarthgap"
```

## 5.5 整改已有工程

若已有工程未按规范组织（如前辈的结构比较简单粗暴，创建了一个自定义层 `meta-cetca`，然后就直接在其中加入了很多修改后的配方，所有配方直接放在单个 `meta-cetca` 层中），整改步骤：

1. 在项目目录下创建 `meta-bsp` 和 `meta-sdk` 子层（其实可以直接在 `meta-avod` 下 `bitbake-layer create layer meta-bsp` 来创建层）
2. 将 `recipes-kernel`、`recipes-bsp` 移入 `meta-bsp`
3. 将 `machine/` 和 `distro/` 配置分别移入对应子层的 `conf/` 下
4. 为每个新子层编写 `layer.conf`（可以参考前辈的 `meta-cetca` 的 `conf` 中的配置文件来修改，前辈的 `meta-cetca` 是一个完整意义的层，不同于规范里的 `meta-cetca`）
5. 更新 `build/conf/bblayers.conf` 中的层路径
6. 运行 `bitbake -p` 检查语法，这条命令能够检查出有效的层路径、配方语法等基础问题。然后就可以尝试构建 `virtual/kernel` 和 `virtual/bootloader` 等配方了。如果遭遇报错，可以根据问题进行对应的修改。

---

# 6. 初始化脚本与工程迁移

在整改的过程中我发现，直接去修改这些 `.conf` 文件其实没有那么方便，特别是 Yocto 工程会涉及到很多环境变量，这些变量在移植的过程中路径可能会出问题（从一台电脑到另一台）。另外，Yocto 工程是非常庞大的，每一次拷贝如果都是拷贝全部效率非常低。这个时候，脚本的强大之处就体现了出来。

## 6.1 官方初始化脚本

`poky/` 目录提供了 bitbake 的初始化脚本 `oe-init-build-env`，该脚本会在当前终端初始化 bitbake，并且会创建 Yocto 工程的构建目录 `build/` 并跳转至该目录下。这个目录如果不加定义会创建在 `poky/` 目录下，这显然不符合我们的工程结构，因此每一次需要：

```bash
source sources/poky/oe-init-build-env build/
```

而每次打开一个新的终端都需要重新运行该脚本，稍不注意就可能在 `poky/` 目录下再创建一个 `build/`。

## 6.2 自定义构建目录初始化脚本

有没有更好的办法呢？使得我们直接在构建目录内就可以初始化，并且不用输入繁复的地址指令？我们可以在构建目录下创建一个新的脚本用于初始化环境。该脚本可以继承 `oe-init-build-env` 脚本，然后还可以自定义一些配置。

这里给出了一个通用的初始化脚本，它被放在 `./build/` 构建目录下，我们可以直接运行该脚本来初始化构建环境并进入构建目录，不要求当前目录：

```bash
#!/bin/bash

# 动态获取工程根目录（脚本位于 build 目录下，上一级即为工程根目录）
PROJECT_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

# 定义必要路径（基于你的工程结构）
OE_INIT_SCRIPT="${PROJECT_ROOT}/sources/poky/oe-init-build-env"
BUILD_DIR="${PROJECT_ROOT}/build"

# 错误检查：确保初始化脚本存在
if [ ! -f "${OE_INIT_SCRIPT}" ]; then
    echo "错误：未找到 oe-init-build-env 脚本，路径：${OE_INIT_SCRIPT}"
    echo "请确认工程结构是否符合预期：${PROJECT_ROOT}/sources/poky/"
    exit 1
fi

# 错误检查：确保 build 目录存在
if [ ! -d "${BUILD_DIR}" ]; then
    echo "错误：build 目录不存在，路径：${BUILD_DIR}"
    exit 1
fi

# 自动切换到 build 目录
echo "切换到构建目录：${BUILD_DIR}"
cd "${BUILD_DIR}" || {
    echo "错误：无法切换到 build 目录"
    exit 1
}

# 执行官方环境初始化脚本
echo "正在初始化 yocto 构建环境..."
source "${OE_INIT_SCRIPT}" "${BUILD_DIR}"

# 传递 DL_DIR 和 SSTATE_DIR 环境变量（如果已设置）
if [ -n "$DL_DIR" ]; then
    BB_ENV_PASSTHROUGH_ADDITIONS="$BB_ENV_PASSTHROUGH_ADDITIONS DL_DIR"
fi
if [ -n "$SSTATE_DIR" ]; then
    BB_ENV_PASSTHROUGH_ADDITIONS="$BB_ENV_PASSTHROUGH_ADDITIONS SSTATE_DIR"
fi
export BB_ENV_PASSTHROUGH_ADDITIONS

# 清除模板配置变量，确保使用当前 build/conf 目录的配置
unset TEMPLATECONF

echo "环境初始化完成！"
```

该脚本只要保证工程的总体结构是一致的：

```
工程根目录/
├── build/                  # 构建目录（脚本存放位置）
└── sources/
    └── poky/
        └── oe-init-build-env  # 官方初始化脚本
```

那就可以实现复用。另外在脚本的末尾还有一些自定义的变量，用于特殊的情况。比如对 `BB_ENV_PASSTHROUGH_ADDITIONS` 变量的设置，一般是用于临时覆盖某些变量比如 `DL_DIR`，这时候该变量将不采用 `local.conf` 中设置的值，而是直接使用环境变量中的值，较为灵活。

## 6.3 setup-env 项目初始化脚本

在 SVN 上有一个统一的工程初始化入口 `setup-env`，这个入口也是一个脚本，只是通过不同的选项来为对应项目完成初始化，本质上就是脚本调用脚本。入口脚本会根据项目再调用对应的初始化脚本，比如我的项目脚本就可以叫做 `avod-setup-env.sh`。

**因此最关键的脚本其实就是这个 `avod-setup-env.sh`！** 它会帮助我们在一个陌生的环境中搭建起我们熟悉的工程环境。其源码并不复杂（至少前辈修改的那个版本），主要就是利用脚本去寻找路径并且完成一些自定义的初始化。核心功能：

- **自动定位工程目录**：脚本基于自身位置通过相对路径计算工程根目录，不依赖绝对路径
- **`.sample` 模板机制**：用预置的 `local.conf.sample` 和 `bblayers.conf.sample` 生成对应的 `.conf` 文件（覆盖 `oe-init-build-env` 的默认模板）
- **模板碎片 `.conf.inc`**：为特定 `MACHINE` 追加额外的配置片段
- **`machines` 和 `features` 校验**：脚本内置合法机器列表，传入错误机器名会提前报错
- **调用 `oe-init-build-env`** 完成最终的 BitBake 环境变量初始化

脚本内部的关键变量是 `AGL_REPOSITORIES`（标识当前项目目录名），后续所有路径定位都依赖此变量 + 固定的相对路径规则。这样移植工程时无需手动修改 `.conf` 中的绝对路径，直接运行脚本即可。

```bash
source sources/meta-cetca/meta-avod/meta-bsp/tools/avod-setup-env.sh -b build
```

具体的脚本和修改方式可以直接参考对应的脚本文件。对于 `avod-set-up-env.sh`，我们更多的是关注开头 `AGL_REPOSITORIES` 的查询和确定，对 `machines` 和 `features` 的查找，以及最后可能需要添加的一些配置。之间很多部分其实完全不需要修改，因为 Yocto 工程自己的目录结构一般是固定的，后续的脚本主要依靠前面确定的 `AGL_REPOSITORIES` 和相对路径完成任务。

## 6.4 工程迁移：打包-传输-解压

Yocto 工程包含大量文件，跨机器拷贝时不能简单使用 `cp`（会丢失权限、遗漏隐藏文件、传输损坏等）。正确做法：

```bash
# 打包（保留权限）
tar -zcvpf yocto.tar.gz yocto/

# 传输到目标机器后解压
tar -zxvpf yocto.tar.gz

# 修复所有者（如果源机与目标机 uid 不同）
sudo chown -R $(id -u):$(id -g) ~/yocto
```

参数说明：
- `-z`：gzip 压缩（减少体积，方便传输）
- `-j`：bz2 压缩（压缩率更高但速度较慢，大工程可选用）
- `-c`：创建压缩包
- `-v`：显示打包过程（可选，方便确认）
- `-p`：**保留所有文件权限和所有者**（核心参数，避免权限丢失）
- `-f`：指定压缩包名
- `-x`：解压

**迁移前的清理**：在构建镜像时可以先用 `bitbake --runall=fetch st-image-*` 来只是进行 `do_fetch` 操作，仅仅下载源码。而后将 `build` 目录下多余的文件全部清理，比如 `tmp/` 等，仅保留 `conf/` 目录（含 `local.conf` 和 `bblayers.conf`）。如果没有修改 `DL_DIR`（在 `local.conf` 中修改），那么还需要保留 `downloads/` 目录。

迁移后检查 `build/conf/local.conf` 和 `bblayers.conf` 中的绝对路径是否需要修改。

## 6.5 离线构建配置

仅下载源码而不构建全部：

```bash
bitbake --runall=fetch core-image-minimal
```

在 `local.conf` 中配置使用本地缓存：

```bash
# 跳过网络检查（仅首次连接检查）
CONNECTIVITY_CHECK_URIS = ""
# 设置 downloads 目录
DL_DIR ?= "${TOP_DIR}/../downloads"
```

**为什么用本地 downloads 而不是把所有配方都改为 SVN 源？**

一个系统镜像涉及到的配方文件多达几百个。逐一修改每个 `.bb` 文件中的 URL 或在自定义层中通过 `.bbappend` 覆盖 `SRC_URI`，都是费时费力且易出错的工作。配置 `MIRROR` 路径也会遇到各种问题。

Yocto 工程本身支持本地缓存——如果 `downloads/` 目录下已有源码，bitbake 会自动跳过 `do_fetch` 而直接使用本地文件。因此**与其费尽心思配置 URL 来在构建时从 SVN 仓库下载源码，不如提前将源码拖下来，设置好路径，让 bitbake 直接使用**。只需要对需要后续修改的配方（如 kernel、u-boot）单独配置 SVN 源即可。

当然这也存在一些问题，如果后期工程繁杂了，需要的源码文件增多，而某一个工程可能仅需要其中的一小部分，这个时候也许配置 URL 就变得有意义了。

**注意**：不要设置 `BB_NO_NETWORK = "1"`，这会导致无法连接 SVN 服务器。只需要配置 `CONNECTIVITY_CHECK_URIS = ""` 来绕过初始联网检查。

---

# 7. SVN 集成专题

## 7.1 在 distro 中配置 SVN 镜像重定向

对于不需要修改源码的软件包（直接从互联网下载的压缩包等），不需要逐一修改配方中的 `SRC_URI`，而是通过在发行版配置中设置镜像重定向，让 bitbake 在 fetch 时自动将互联网 URI 重定向到 SVN 仓库。

在发行版配置（如 `distro/cetca-avod-dev.conf`）中定义 SVN 连接变量：

```bash
# svn configuration
PROJECT_SVN_SERVER_URIS = "192.168.1.77"
PACKAGES_SVN_SERVER_URIS = "192.168.1.77"
SWITCH_SVN_SERVER_URIS = "192.168.1.77"
PROJECT_SVN_PATH = "/svn/svn/Cabin_SW/trunk/PLATFORM/ARM_PLATFORM/AVOD/00-code/"
PACKAGES_SVN_PATH = "/svn/svn/Cabin_SW/trunk/PLATFORM/COMPILE_PLATFORM"
SOURCE_MIRROR_SVN_PATH = "/svn/svn/Cabin_SW/trunk/PLATFORM/COMPILE_PLATFORM/downloads/"
SVN_USER = "user=dailybuild;pswd=cetcA123"

SOURCE_MIRROR_URL = "https://${PROJECT_SVN_SERVER_URIS}${SOURCE_MIRROR_SVN_PATH}"

FETCHCMD_wget = "/usr/bin/env wget -t 2 -T 30 --passive-ftp --no-check-certificate --user=dailybuild --password=cetcA123"
INHERIT += "own-mirrors"
PREMIRRORS:append = " \
    git://.*/.* ${SOURCE_MIRROR_URL} \n \
    gitsm://.*/.* ${SOURCE_MIRROR_URL} \n \
"
MIRRORS:append = " \
    https://.*/.* ${SOURCE_MIRROR_URL} \n \
"
```

这里主要是设置了一些变量，注意根据项目可能里面会做一些替换。前面部分是将 SVN 仓库的 URI 变成变量，用变量形式在配方中引用方便后续更改 URI。

最后的 `PREMIRRORS` 和 `MIRRORS` 就是镜像的重定向，这样配置以后所有使用这个 `distro` 的工程在下载源码时会将原来互联网中的 URI 重定向到 SVN 仓库中，并且是优先检查 SVN 仓库。也就是说不需要去修改这些 `.bb` 配方中的 `URI`，bitbake 在 fetch 时会自动重定向。

变量说明：
- `PROJECT_SVN_SERVER_URIS` / `PACKAGES_SVN_SERVER_URIS`：SVN 服务器地址，分别用于项目源码和预编译包（通常指向同一台服务器）
- `PROJECT_SVN_PATH`：项目源码在 SVN 上的路径（存放 kernel、u-boot 等需要修改的源码）
- `PACKAGES_SVN_PATH`：预编译包路径
- `SOURCE_MIRROR_SVN_PATH`：源码镜像路径，指向 SVN 上的 `downloads/` 目录（存放从互联网下载的原版源码包）
- `SVN_USER`：认证信息，以 `user=xxx;pswd=xxx` 格式
- `FETCHCMD_wget`：覆盖 wget 下载命令，携带认证信息（有些配方使用 wget 而非 bitbake 的 fetcher）
- `INHERIT += "own-mirrors"`：启用自定义镜像机制
- `PREMIRRORS`：**预镜像**，在去原始 URI 之前先检查的镜像地址
- `MIRRORS`：**后备镜像**，在原始 URI 下载失败后尝试的镜像地址

> **设计原则**：这种 SVN 镜像配置应该放入 `distro`（发行版配置）而非 `machine`（机器配置），因为它代表的是整个项目的软件获取策略，与具体硬件无关。

## 7.2 配方中配置 SVN 下载

对于需要从 SVN 拉取源码的配方（如 kernel、u-boot），在 `.bbappend` 中覆盖 `SRC_URI`：

```bash
SRC_URI = "svn://192.168.1.77/svn/svn/Cabin_SW/trunk/PLATFORM/ARM_PLATFORM/AVOD/00-code;protocol=https;module=kernel;username=dailybuild;password=cetcA123"
SRCREV = "${AUTOREV}"
```

URI 格式说明：
- 开头必须为 `svn:`（即使使用的是 https 协议）
- 地址写到存放源码的**上级目录**
- `protocol=https`：指定传输协议
- `module=kernel`：存放源码的具体目录名
- `username=` / `password=`：SVN 认证信息

### 源码目录匹配问题

SVN 拉取的源码目录名由 `module` 决定，默认的 `${S}` 可能不匹配。比如 `module` 的值是 `kernel`，那么 bitbake 执行任务后源码被放在了 `${WORKDIR}/kernel` 这样的目录下。需要在配方中覆盖：

```bash
S = "${WORKDIR}/kernel"  # 与 module 的值对应
```

对于 kernel 和 u-boot 的头文件配方（`*-headers-*.bb`，通常与主配方放在同一目录如 `recipes-kernel/linux/`），其 URI 通常与主配方相同，也需要同步修改。这些头文件配方用于生成用户空间开发所需的头文件接口，如果遗漏会导致构建失败。

### 使用 .inc 文件统一管理公共变量

由于 kernel/u-boot 的主配方和 headers 配方共享相同的 URI 和 `S` 目录定义，前辈的做法是在 `recipes-*` 目录下创建一个 `.inc` 文件来统一定义：

```bash
# linux-avod.inc — 被 linux-avod_*.bb 和 linux-avod-headers_*.bb 共同 include
SRC_URI = "svn://${PROJECT_SVN_SERVER_URIS}${PROJECT_SVN_PATH};protocol=https;module=kernel;${SVN_USER}"
S = "${WORKDIR}/kernel"
```

这样修改一处即可同步到所有相关配方，避免遗漏。

### cfg 配置碎片文件路径问题

如果原配方的 `SRC_URI` 中通过 `SRC_URI +=` 引用了 `.cfg` 配置碎片文件，而我们用 `=` 覆盖了 `SRC_URI`，这些引用会丢失。解决方案有两种：

1. **将碎片文件拷贝到自定义层**，并在 `.bbappend` 中重新引用：
   ```bash
   FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"
   ```
   这会让 bitbake 在当前配方目录下也查找文件。然后将原配方目录下的所有文件（`.cfg`、补丁等）整体拷贝到 `.bbappend` 所在目录。

2. **固化到源码**：在 `do_configure` 完成后，进入 `${B}` 目录执行 `make savedefconfig`，将生成的精简 `defconfig` 放入源码 `arch/arm64/configs/` 目录，然后在配方中修改 `KBUILD_DEFCONFIG` 指向它（详见 02-文档第 2.2 节）。

### 跳过 do_patch

如果补丁已固化进源码，可以跳过 patch 阶段：

```bash
do_patch[noexec] = "1"
```

初次编译补丁已经被打进源码了，所以上传 SVN 后可以直接跳过。

## 7.3 local-git 类的干扰

`local-git` 本身是使用本地 `git` 的一种加速手段，它识别到 `.bb` 文件中的 `git` 源后就会强制使用该源，即使我们在 `.bbappend` 中使用自己的 `svn` 源去覆盖了。他的优先级似乎很高。

表现为 `do_unpack` 报错"未找到对应解压文件"，但 `do_fetch` 却标记为成功。实际上就是 fetch 时源码根本没有从 `svn` 仓库中下载，但是 `fetch` 任务却不知什么原因认为本地存在源码选择跳过，并且认为任务成功了（可能是 `local-git` 强制使用 `git` 源时的一些错误配置）。

**解决方案**：在原 `.bb` 文件中注释掉 `inherit local-git`：

```bash
# inherit local-git
```

我们想要不改动 `.bb` 文件的情况下禁用 `local-git` 似乎非常麻烦。可以在 `.bbappend` 中写一个注释提醒，然后去 `.bb` 文件中禁用。

定位这个问题就花了很久，一开始可能的 SVN 相关配置问题，到缓存问题等等... 之后要更改下载路径时要尤其注意 `git` 带来的影响，如果原本使用的 `git` 源，`local-git` 会强制绑定一些下载配置，会严重干扰我们的自定义下载。

## 7.4 SVN 证书问题

由于 SVN 服务器的版本比较老，并且证书也长时间没有更新了，所以在使用较新的 Ubuntu 和 SVN 版本时，首次连接 SVN 仓库会提示证书问题并给出选项：**拒绝**，暂时接受或**永久接受**。

本应是这样，但是我的 Ubuntu22.04 虚拟机不知为何缺少了**永久接受**这一选项，导致了 SVN 几乎无法正常使用，要进行任何的操作都必须手动接受证书。而之前前辈的 Ubuntu22.04 就可以正常选择永久接受。经过详细的检查，SVN 及其依赖包的版本甚至都完全一致，只有 Ubuntu 的版本差异和内核版本差异...

我的是 Ubuntu22.04.5LTS Linux 内核版本为：6.8.0-85-generic；前辈为 Ubuntu22.04.4LTS Linux 内核版本为：6.5.0-35-generic。进一步依赖发现：我们的 OpenSSL 版本以及 CA 证书版本存在差异！安全检查有根本差异，新版本变得更严格了。

如果键入 `svn checkout`，我的 Ubuntu 版本会弹出：

```
 - The certificate hostname does not match.
 - The certificate has expired.
 - The certificate has an unknown error.
```

而前辈的提示为：

```
 - The certificate is not issued by a trusted authority.
 - The certificate hostname does not match.
 - The certificate has expired.
```

由于对 `unknown error` 的容忍度似乎在新的版本变低了，因此 SVN 隐藏了**永久接受**的选项。

我一开始考虑过很多解决方案，获取并信任证书，回退 SVN 版本甚至回退内核版本，最后发现这些想要从根上解决问题的办法非常麻烦，而且很可能导致以后的兼容性问题！

最后的解决办法其实就在一开始提出的方法中，通过修改配置文件来忽略证书问题。但是这里其实又有坑...

这里先给出完整的解决方案：

在 `~/.subversion/servers` 中的 `global` 字段加入：

```bash
[global]
...
# 保存 SVN 账户的用户名和凭据，减少重复输入
username = dailybuild
store-auth-creds = yes
# 保留证书信任开关
ssl-trust-server-cert = yes
```

允许 SVN 保存用户名和凭据，保留证书的信任开关。然后执行一次相关的命令与 SVN 仓库交互一下，比如：

```bash
svn list https://192.168.1.77/svn/svn/Cabin_SW/trunk/PLATFORM/ARM_PLATFORM/STM32
```

这时就会提示我们证书的各种问题，有永久接受就选，没有就选暂时。中间可能还会让我们输入本地的账户密码和 SVN 的账户与密码。注意如果用 MobaXterm 的话可能需要打开 GUI，去输入密码，不然终端会卡住。

完成之后应该会实现第一次与 SVN 仓库的交互，在本地会保存一些基础的信息。现在为了避免每一次 SVN 命令都去添加一堆后缀来避免证书的问题，我们可以将其添加到配置文件或全局变量中。

然而这就是一个坑，按理说添加到配置文件中是非常合理的，然而事实上真的把下列代码添加进 `server` 中会完全不生效：

```bash
ssl-trust-server-cert-failures = cn-mismatch,expired,unknown-ca,other
```

最开始我就因为这个直接放弃了该方法...

最后我们选择使用全局变量的方法，在 `~/.bashrc` 的末尾添加轻量级别名：

```bash
alias svn='svn --trust-server-cert --trust-server-cert-failures=cn-mismatch,expired,unknown-ca,other'
```

记得在当前终端启用：

```bash
source ~/.bashrc
```

这样我们就可以正常使用 `svn` 命令了，它会自动将这些选项加入。

**另外值得注意的是，这个配置必须在每个虚拟机下单独进行，不能在模板中配置。**

### SVN 密码问题

另外就是因为 SVN 的机制问题，似乎无法实现保存密码，每一次敲命令都会要求输入密码。这一点倒是好解决。我们可以在 `~/.bashrc` 中为 `svn` 命令添加别名，直接显式指定用户和密码。而对应 Yocto 来说也是一样，直接在 URI 中显式指定就好。

## 7.5 bitbake 使用本地 SVN

在正常的命令行中使用 SVN 遇到的安全证书问题已经得到了解决，但是 bitbake 在执行 `do_fetch` 任务时，该环境不会继承原本的别名，因此原来解决的问题又出现了...

在 `.bashrc` 中添加别名的方式只能解决命令行界面使用 SVN 的问题，但是 bitbake 构建时使用的 SVN 命令会绕过它，别名不生效。

首先我们的 Yocto 工程可能还不支持使用本地的 SVN，因此可以在 `local.conf` 中添加：

```bash
HOSTTOOLS += "svn"
```

这样我们在更改 URI 后 bitbake 也能够正确地使用本地 SVN 去拉取服务器上的代码。

但是这个时候并不代表 bitbake 使用的 SVN 命令会使用别名！我们可以查看 bitbake 调用的是哪个 SVN：

```bash
ls -la build/tmp/hosttools/ | grep svn
# lrwxrwxrwx  1 public public   12 10月 21 14:12 svn -> /usr/bin/svn
```

可以看到 bitbake 调用的是本地的 svn 二进制可执行文件。我们可以直接对其进行修改来达到目的。

具体做法就是我们创建一个包装脚本，然后该脚本来调用原本的 svn 可执行文件，这样所有的 svn 命令都会使用额外的选项：

```bash
# 创建新的包装位置（避免覆盖系统命令）
sudo tee /usr/local/bin/svn-custom << 'EOF'
#!/bin/bash
exec /usr/bin/svn.orig --trust-server-cert \
  --trust-server-cert-failures=cn-mismatch,expired,unknown-ca,other \
  --non-interactive "$@"
EOF

# 设置可执行权限
sudo chmod +x /usr/local/bin/svn-custom

# 创建原始命令备份
sudo cp /usr/bin/svn /usr/bin/svn.orig

# 创建符号链接（指向自定义包装）
sudo ln -sf /usr/local/bin/svn-custom /usr/bin/svn
```

注意我们写的包装脚本在 `/usr/local/bin/` 下，然后把原本的 `/usr/bin/` 下的 `svn` 改为 `svn.orig`，并在这个目录下再生成一个 `svn` 链接到 `svn-custom`。这样我们在执行 `svn` 命令时，其实执行的就是包装脚本。

不过也要注意，我们依然需要先使用一次普通的 svn 命令来缓存用户名和密码，甚至可能还需要更改一些必要的设置。然后再来改动这里。

此时我们可以来验证一下：

```bash
ls -la /usr/bin/svn
# lrwxrwxrwx 1 root root 25 10月 21 15:31 /usr/bin/svn -> /usr/local/bin/svn-custom

# 测试命令
svn info https://192.168.1.77/svn/svn/Cabin_SW/trunk/PLATFORM/ARM_PLATFORM/STM32/
```

应该能够正常输出 SVN 仓库对应目录的基本信息。这样我们就可以在虚拟机中绕过安全证书的检查，正常使用 SVN 了，bitbake 构建时应该也不会因为证书问题报错了。

还有要注意的是，bitbake 在解析时使用的是系统本地的 SVN，但是 bitbake 在构建时如果 `do_fetch` 阶段遇到了 SVN 的 `URI`，那么它会尝试安装一个 `subversion-native` 来在自己的环境内拉取，**这样会导致我们之前配置好的本地 SVN 完全失效**！因此我们需要在 `local.conf` 中额外对这个进行配置：

```bash
# ./build/conf/local.conf
...

# 禁止构建 subversion-native
ASSUME_PROVIDED += "subversion-native"

# 指定使用主机系统的 SVN
PREFERRED_PROVIDER_subversion-native = "host-subversion"
```

这样就能够保证在构建阶段 bitbake 也会使用本地的配置好的 SVN 运行指令。

**验证配置**：

```bash
# 确认 bitbake 使用的是本地包装后的 svn
ls -la build/tmp/hosttools/ | grep svn
# 应输出: svn -> /usr/bin/svn (而 /usr/bin/svn -> /usr/local/bin/svn-custom)

# 确认包装脚本正常工作
svn info https://192.168.1.77/svn/svn/Cabin_SW/trunk/PLATFORM/ARM_PLATFORM/STM32/
```

## 7.6 SVN 配置层次总结

由于服务器 SVN 版本过低、证书过期、新版本 OpenSSL/CA 安全性增强等原因，SVN 的配置需要分三个层次进行：

| 层次 | 作用域 | 配置方式 |
|:---|:---|:---|
| **命令行终端** | `svn` CLI 命令 | `~/.bashrc` 别名 + `~/.subversion/servers` 配置 |
| **bitbake 解析阶段** | bitbake 调用 `svn` 解析 `SRC_URI` | 在 `/usr/bin/svn` 处创建包装脚本（因为 bitbake 不继承 shell 别名） |
| **bitbake 构建阶段** | `do_fetch` 实际拉取源码 | `local.conf` 中 `ASSUME_PROVIDED` + `PREFERRED_PROVIDER_subversion-native`（防止 bitbake 构建自己的 `subversion-native` 绕过本地配置） |

我一共进行了三次配置，分别针对不同的环境：

1. 为虚拟机终端配置：SVN 本地配置 + 环境变量
2. 为 bitbake 解析 SVN：根目录 SVN 二进制文件的包装脚本
3. 为 bitbake 构建时使用 SVN：在 `local.conf` 中配置使用本地 SVN

本质上都是为了绕过 SVN 的安全检查，但是为了找到问题以及如何解决这个问题耗费了大量的时间... 有帮助吗？有一点，这个过程也帮我了解了 bitbake 的构建的细节。但是代价有点大了...

### SVN 最终解决

上面的这些做法都是治标不治本。因为问题的根源出在 SVN 服务器上，所以最好的办法就是去更新 SVN 服务器的证书。根据资料（AI 就行）重新自签名一个新的证书，这样在客户端就不会再遇到各种错误了。问题直接解决！不需要去自定义 `svn` 然后软链接这种麻烦的操作，直接从根上解决了问题。另外 Yocto 也不必再使用本地的 svn 了，相关的配置可以直接注释掉。

至此，SVN 的相关问题完美解决~

## 7.7 清除缓存注意事项

构建失败需要清除缓存时，**不要使用**：

```bash
bitbake -c cleanall <recipe>   # 会删除下载的源码！
```

优先使用：

```bash
bitbake -c cleansstate <recipe>  # 保留源码，仅清除编译结果
```

**注意**：不要配置 `BB_NO_NETWORK = "1"`，这会导致无法连接 SVN 服务器。离线构建只需配置 `CONNECTIVITY_CHECK_URIS = ""` 绕过初始网络检查，让 bitbake 从 SVN（或本地 `DL_DIR`）获取源码。

---

# 8. *-native 概念

Yocto 中同一个软件可以存在于三个不同"世界"：

| 后缀 | 世界 | 运行在哪 | 用途 |
|:---|:---|:---|:---|
| 无后缀 | target | ARM64 板子 | 运行时 |
| `-native` | native | x86_64 构建机 | **构建时工具** |
| `-nativesdk` | SDK | x86_64 (SDK) | 给应用开发 |

**判断原则**：这个东西是给 bitbake 用的还是给板子用的？
- 给 bitbake → `-native` → `DEPENDS`
- 给板子 → 无后缀 → `IMAGE_INSTALL / RDEPENDS`

无后缀但用 `DEPENDS` 的情况：当配方依赖另一个配方的构建结果（如链接库、数据文件）时：

```bash
DEPENDS += "applications"   # 依赖另一个配方的构建产物
```

---

# 9. Applications 用户态工具编译

嵌入式 Linux 上电后可能需要硬件自检、周期检查等用户态应用。`applications` 配方从 SVN 拉取源码、编译，并在 `do_install` 中定制安装路径。其产物配合 `rootfs` 配方一同打包进根文件系统。

典型的工程结构：

```
board/modules/bite.c             # 底层进程源码
lib/bite/bite_lib.c              # 用户态库函数
tools/bite_debug/bite_debug.c    # 调试接口
```

`applications` 的构建采用 Linux 内核风格的递归 Make——顶层 Makefile 只描述目标和依赖关系，具体如何构建由每个子目录下的 `.build` 文件（语法与 Makefile 一致）完成。大致流程：顶层 `make` → 遍历子目录优先构建 `lib` → 各子目录中 `make` 遍历目标 → 运行 `.build` 文件编译。

## 编译报错：递归 Make + 并行竞态

从 SVN 拉取的源码编译时，`$(LD)` 合并 `.o` 文件时失败：

```makefile
BUILD = $(OUTPUTDIR)/module-$(DIRNAME).o
$(BUILD): $(curdir_objs) $(subdir_objs)
	$(LD) -r -o $@ $^
```

该语句能被正确解析，但执行时失败。如果在报错后手动进入对应目录执行相同的 `$(LD)` 命令（需先 source 环境变量来获取交叉编译工具链路径），能成功执行。再次 `bitbake applications`（不 clean）也能编译成功。

**原因**：SVN 拉取的文件时间戳非常接近（都是拉取时刻），而本地文件的时间戳较为松散。GNU Make 根据时间戳判断文件状态，对于递归 Make 结构，在并行编译时可能出现竞态——子目录的 `.o` 文件刚被创建、尚未写入完毕甚至还在被其他任务修改，另一个并行任务就检测到它存在并开始 `$(LD)` 链接。

**尝试过的修复方案**（均无效）：
1. `$(LD) -r -o $@ $(sort $^)` — 显式排序依赖，但根本原因不是顺序而是竞态
2. 引入 `.PHONY: all_objs` 中间目标 — 试图等所有 `.o` 构建完再链接，但 `.PHONY` 无法表示"文件已完成写入"的状态

**根本原因**：这是 GNU Make 在处理递归 Make 时的已知问题。父 Makefile 依赖子 Make 的产物时，如果依赖的不是真实文件而是 `.PHONY` 目标，就无法判断子 Make 是否真正完成。标准写法 `$(BUILD): $(curdir_objs)` 在单文件 Makefile 中安全，但在递归 Make 下不保证安全。

**解决方案**：限制并行任务数：

```bash
PARALLEL_MAKE = "-j1"
PARALLEL_MAKEINST = "-j1"
```

> GNU Make 官方建议：如果使用递归 Make，父 Makefile 应该依赖子 Make **产生的真实文件**而非 `.PHONY` 目标。但对于已有的复杂遗留工程，限制 `-j1` 是最稳妥的做法。

---

# 10. VScode 远程连接开发

使用 MobaXterm+VIM 也可以直接在终端中实现 Yocto 工程的各种文件编辑，但是终究比较麻烦并且在修改代码时也不方便，于是我考虑使用本地的 VScode+SSH 远程连接虚拟机来进行工程的修改。VScode 是一款轻量化的代码编辑器，拥有着丰富的插件库。我不仅可以 SSH 连接虚拟机，还可以安装 Yocto 工程相关的插件来实现图形化的构建与调试。

## 10.1 SSH 配置

在本地 VScode 上安装 `remote SSH` 相关的插件，界面的左边就会有`远程资源管理器`。

可以通过 `Ctrl+Shift+P` 并键入 `Remote-SSH` 来更新我们的 SSH 配置文件，我们优先去更改自己用户的 `.ssh` 文件，一般位于 `C:\Users\username\.ssh\config`。

根据我们的虚拟机 IPv4 地址和用户名，在 `config` 中添加：

```
Host ubuntuvm
    Hostname 192.168.x.x
    User vm-username
```

配置好以后我们就可以 `Ctrl+Shift+P` → `Remote-SSH: Connect to Host...` 来用命令连接虚拟机，也可以直接用 GUI 选项连接。

虚拟机侧还需要注意一些配置：

```bash
sudo apt update && sudo apt install openssh-server  # 下载 ssh 服务
sudo systemctl start ssh      # 开启 ssh 服务
sudo systemctl enable ssh     # 设置 ssh 服务开机自启
sudo systemctl stop ufw       # 关闭防火墙
sudo systemctl disable ufw
```

## 10.2 SSH 密钥免密登录

由于虚拟机有用户名和密码，每一次连接时就需要输入密码，非常的繁琐。我们可以通过使用 SSH 密钥来进行认证。

首先在 Win 主机上生成一个 SSH 密钥对。`Win+R` → `cmd` 或 `powershell` 打开终端，输入以下命令生成密钥（邮箱可以替换为任意的标识信息）：

```bash
ssh-keygen -t rsa -b 4096 -C "your_email@example.com"
```

接着会提示为密钥设置"通行短语"，为了免密登录，我们只需要按回车留空就好，这样就会生成一个无通行短语的密钥对。

完成后我们就可以在用户的 `.ssh` 目录下得到两个文件：

```
id_rsa        # 私钥 需要严格保密
id_rsa.pub    # 公钥
```

然后我们需要将公钥上传至虚拟机：在本地查看 `id_rsa.pub` 公钥的内容，并完整复制。通过各种方式登录虚拟机（比如 MobaXterm），进入 `~/.ssh/` 目录，没有可以自行建立，然后将复制的公钥内容追加到 `~/.ssh/authorized_keys` 文件末尾。

然后为该目录和文件设置正确的权限：

```bash
chmod 700 ~/.ssh
chmod 600 ~/.ssh/authorized_keys
```

还要注意 `~` 目录，也就是 `/home/user_name` 目录的权限：

```bash
chmod 755 /home/user_name
```

---

# 附录：SVN 基本操作

| 操作 | 命令 | 说明 |
|:---|:---|:---|
| 检出仓库 | `svn checkout [URL] [本地目录]` | 首次获取工作副本 |
| 查看状态 | `svn status` / `svn st` | M=修改, A=新增, D=删除 |
| 更新 | `svn update` / `svn up` | 拉取服务器最新内容 |
| 提交 | `svn commit -m "说明"` / `svn ci -m "说明"` | 必须填写提交说明 |
| 添加文件 | `svn add 文件名` | 新增文件后需执行 |
| 删除文件 | `svn delete 文件名` / `svn rm 文件名` | 需后续提交生效 |
| 查看历史 | `svn log` | 显示提交记录 |
| 查看差异 | `svn diff 文件名` / `svn di 文件名` | 对比本地与服务器差异 |
| 撤销修改 | `svn revert 文件名` | 恢复到上次提交状态（谨慎使用） |

SVN 仓库的 Yocto 工程版本管理结构：

```
platform/
├── 1.8.1/           # Yocto 版本 → poky 版本
│   └── ...
└── 5.0.11/
    ├── meta-rockchip
    ├── ...
    └── poky
```

这里的 `1.8.1`、`5.0.11` 都是 Yocto 的版本号，对应着不同的 `poky/`。

自定义层（如 `meta-cetca`）单独管理，与 poky 版本解耦。

在 Ubuntu 中建立与 SVN 仓库的联系：

```bash
svn checkout https://192.168.1.77/svn/svn/Cabin_SW/trunk/PLATFORM/ARM_PLATFORM/STM32 ~/svn-workspace
```
