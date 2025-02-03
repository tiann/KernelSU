# What is KernelSU?

KernelSU is a root solution for Android GKI devices, it works in kernel mode and grants root permission to userspace apps directly in kernel space.

## Features

The main feature of KernelSU is it is **Kernel-based**. KernelSU works in kernel mode, so it can provide kernel interface we never had before. For example, we can add hardware breakpoint to any process in kernel mode; We can access physical memory of any process without anybody being aware of; We can intercept any syscall in kernel space; etc.

And also, KernelSU provides a module system via OverlayFS, which allows you to load your custom plugin into system. It also provides a mechanism to modify files in `/system` partition.

## How to use?

Please refer: [Installation](installation)

## How to build?

Please refer: [How to build](how-to-build)

## Discussion

- Telegram: [@KernelSU](https://t.me/KernelSU)
