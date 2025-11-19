# 什么是 KernelSU？ {#introduction}

KernelSU 是 Android GKI 设备的 root 解决方案，它工作在内核模式，并直接在内核空间中为用户空间应用程序授予 root 权限。

## 功能 {#features}

KernelSU 的主要特点是它是**基于内核的**。KernelSU 运行在内核空间，所以它可以提供我们以前从未有过的内核接口。例如，我们可以在内核模式下为任何进程添加硬件断点；我们可以在任何进程的物理内存中访问，而无人知晓；我们可以在内核空间拦截任何系统调用; 等等。

此外，KernelSU 提供了 [metamodule 系统](metamodule.md)，这是一个可插拔的模块管理架构。与将挂载逻辑内置到核心的传统 root 方案不同，KernelSU 将此功能委托给 metamodule。这允许您安装 [meta-overlayfs](https://github.com/tiann/KernelSU/tree/main/userspace/meta-overlayfs) 等 metamodule，以提供对 `/system` 分区和其他分区的无系统修改。

## 如何使用 {#how-to-use}

请参考: [安装](installation)

## 如何构建 {#how-to-build}

请参考: [如何构建](how-to-build)

## 讨论 {#discussion}

- Telegram: [@KernelSU](https://t.me/KernelSU)
