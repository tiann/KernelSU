# What is KernelSU?

KernelSU is a root solution for Android GKI devices. It works in kernel mode and grants root permission to userspace apps directly in kernel space.

## Features

The main feature of KernelSU is that it's **kernel-based**. KernelSU works in kernel mode, enabling it to provide a kernel interface that we never had before. For example, it's possible to add hardware breakpoints to any process in kernel mode, access the physical memory of any process invisibly, intercept any system call (syscall) within the kernel space, among other functionalities.

Additionally, KernelSU provides a [metamodule system](metamodule.md), which is a pluggable architecture for module management. Unlike traditional root solutions that bake mounting logic into their core, KernelSU delegates this to metamodules. This allows you to install metamodules like [meta-overlayfs](https://github.com/tiann/KernelSU/tree/main/userspace/meta-overlayfs) to provide systemless modifications to the `/system` partition and other partitions.

## How to use KernelSU?

See [Installation](installation.md).

## How to build KernelSU?

See [How to build](how-to-build.md).

## Discussion

- Telegram: [@KernelSU](https://t.me/KernelSU)
