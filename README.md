# study-note-for-linux-and-arm

嵌入式 Linux 与 ARM 开发的学习笔记，基于 RK3568 平台，涵盖从构建系统到内核驱动的完整链路。

## 内容概览

- **虚拟机管理**：QEMU/KVM 镜像创建、磁盘扩容、Samba 配置
- **Yocto 构建系统**：BitBake 原理、配方与层定制、离线构建、镜像裁剪
- **嵌入式 Linux 基础**：Bash 脚本、U-Boot 引导与移植、内核源码阅读环境搭建
- **Linux 设备驱动模型**：kobject/kset、bus/device/driver 框架、module_init 机制
- **设备树**：RK3568 CRU 时钟模型、clock-cells、生产者-消费者模式
- **驱动实例分析**：MAX6635 温度传感器驱动的 probe 流程、hwmon 子系统、devres 机制
- **I2C 子系统**：I2C/SMBus 协议对比、Linux I2C 架构、rk3x 控制器驱动源码分析
- **AI Coding 评估**：代码生成、审查、UT 生成的实战测试与局限性分析

笔记整体遵循由浅入深的递进关系：先介绍设备模型概念（05），再分析真实驱动（06），接着深入 I2C 子系统（07），最后补充设备树时钟框架（08）。

## 目录

| 文件 | 说明 |
|------|------|
| `00-虚拟机管理文档.md` | QEMU/KVM 虚拟机创建、磁盘扩容、Samba 共享 |
| `01-yocto工程改造指南(基于rk3568).md` | Yocto 工程改造实践、机器配置、BSP 集成 |
| `02-嵌入式Linux学习指南(基于rk3568).md` | Bash 脚本、U-Boot 启动流程、内核源码阅读、以太网调试 |
| `03-yocto学习笔记(早期).md` | BitBake 引擎、配方/层/镜像等 Yocto 基础知识 |
| `04-AI Coding测试报告.md` | 本地 AI 模型在嵌入式开发中的编码、审查与测试评估 |
| `05-linux设备驱动模型.md` | 设备-总线-驱动模型、I2C 实例解析、数据访问模型、sysfs 拓扑结构 |
| `06-驱动实例(MAX6635).md` | MAX6635 驱动 probe 分析、hwmon 子系统、devres 生命周期 |
| `07-I2C与SMBus.md` | I2C/SMBus 协议对比、Linux I2C 子系统架构、rk3x 控制器驱动 |
| `08-linux驱动-设备树.md` | RK3568 CRU 时钟控制器、clock-cells、assigned-clocks 机制 |
