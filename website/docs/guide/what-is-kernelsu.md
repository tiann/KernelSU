# What is KernelSU?

KernelSU is a root solution for Android GKI devices, it works in kernel mode and grants root permission to userspace apps directly in kernel space.

## Features

The main feature of KernelSU is that it's **kernel-based**. KernelSU works in kernel mode, enabling it to provide a kernel interface that we never had before. For example, it's possible to add hardware breakpoints to any process in kernel mode, access the physical memory of any process invisibly, intercept any system call (syscall) within the kernel space, among other functionalities.

Additionally, KernelSU provides a module system via OverlayFS, which allows you to load your custom plugin into system. It also provides a mechanism to modify files in `/system` partition.

## How to use KernelSU?

Please refer: [Installation](installation)

## How to build KernelSU?

Please refer: [How to build](how-to-build)

## Discussion

- Telegram: [@KernelSU](https://t.me/KernelSU)
