# study-note-for-linux-and-arm

嵌入式 Linux 与 ARM 开发的学习笔记，基于 RK3568 平台，涵盖从构建系统到内核驱动的完整链路。

## 内容概览

- **Yocto 构建体系**：BitBake 任务链、配方/层/配置、变量与优先级、离线构建、SVN 集成
- **镜像制作与烧写**：分区定义与布局、镜像生成流程、Uboot 烧写、根文件系统配置
- **源码阅读与开发**：U-Boot 源码结构、内核 Makefile/Kconfig、以太网/MMC 调试、VSCode 阅读环境
- **设备驱动模型**：kobject/kset、bus/device/driver 框架、I2C 实例解析、sysfs 拓扑结构
- **驱动实例分析**：MAX6635 温度传感器驱动、hwmon 子系统、devres 生命周期
- **I2C 与 SMBus**：协议对比、Linux I2C 架构、rk3x 控制器驱动源码
- **SPI 总线**：SPI 协议基础、Linux SPI 子系统架构、设备注册与驱动绑定
- **设备树**：RK3568 CRU 时钟模型、clock-cells、assigned-clocks 生产者-消费者模式
- **AI Coding 评估**：代码生成、审查、UT 生成的实战测试与局限性分析
- **虚拟机管理**：QEMU/KVM 镜像创建、磁盘扩容、Samba 配置

笔记整体遵循由浅入深的递进关系：先介绍构建体系（01-03），再进入内核模型（05），结合真实驱动实例（06），深入各总线子系统（07、09），最后补充设备树时钟框架（08）。

## 目录

| 文件 | 说明 |
|------|------|
| `01-Yocto工程与构建体系.md` | BitBake 任务链、配方与层管理、变量机制、构建目录结构、SVN 集成 |
| `02-系统镜像制作与烧写.md` | 分区定义与布局、镜像生成流程、Uboot 烧写、根文件系统裁剪 |
| `03-Linux与U-boot源码阅读与开发.md` | U-Boot 源码结构、内核构建系统、以太网/MMC 调试、代码阅读环境 |
| `04-AI Coding测试报告.md` | 本地 AI 模型在嵌入式开发中的编码、审查与测试评估 |
| `05-linux设备驱动模型.md` | 设备-总线-驱动框架、I2C 实例解析、数据访问模型、sysfs 拓扑 |
| `06-驱动实例(MAX6635).md` | MAX6635 驱动 probe 分析、hwmon 子系统、devres 生命周期 |
| `07-I2C与SMBus.md` | I2C/SMBus 协议对比、Linux I2C 子系统架构、rk3x 控制器驱动 |
| `08-linux驱动-设备树.md` | RK3568 CRU 时钟控制器、clock-cells、assigned-clocks 机制 |
| `09-SPI总线.md` | SPI 协议基础、Linux SPI 子系统、设备注册与驱动绑定 |
| `00-虚拟机管理文档.md` | QEMU/KVM 虚拟机创建、磁盘扩容、Samba 共享 |
| `旧版文档/` | 重构前的三份旧版笔记（Yocto 指南、Linux 学习指南、早期笔记）及重构说明 |
