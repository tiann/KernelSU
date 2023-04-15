# 常见问题

## KernelSU 是否支持我的设备 ？

首先，您的设备应该能够解锁 bootloader。 如果不能，则不支持。

然后在你的设备上安装 KernelSU 管理器并打开它，如果它显示 `不支持` ，那么你的设备没有官方支持的开箱即用的 boot image；但你可以自己编译内核集成 KernelSU 进而使用它。

## KernelSU 是否需要解锁 Bootloader ？

当然需要。

## KernelSU 是否支持模块 ？

支持，但它是早期版本，可能有问题。请等待它稳定 :)

## KernelSU 是否支持 Xposed ？

支持。[Dreamland](https://github.com/canyie/Dreamland) 和 [TaiChi](https://taichi.cool) 可以正常运行。LSPosed 可以在 [Zygisk on KernelSU](https://github.com/Dr-TSNG/ZygiskOnKernelSU) 的支持下正常运行。

## KernelSU 支持 Zygisk 吗?

KernelSU 本体不支持 Zygisk，但是你可以用 [Zygisk on KernelSU](https://github.com/Dr-TSNG/ZygiskOnKernelSU) 来使用 Zygisk 模块。

## KernelSU 与 Magisk 兼容吗 ？

KernelSU 的模块系统与 Magisk 的 magic mount 有冲突，如果 KernelSU 中启用了任何模块，那么整个 Magisk 将无法工作。

但是如果你只使用 KernelSU 的 `su`，那么它会和 Magisk 一起工作：KernelSU 修改 `kernel` 、 Magisk 修改 `ramdisk`，它们可以一起工作。

## KernelSU 会替代 Magisk 吗？

我们不这么认为，这也不是我们的目标。Magisk 对于用户空间 root 解决方案来说已经足够好了，它会存活很久。KernelSU 的目标是为用户提供内核接口，而不是替代 Magisk。

## KernelSU 可以支持非 GKI 设备吗？

可以。但是你应该下载内核源代码并将 KernelSU 集成到源代码树中并自己编译内核。

## KernelSU 支持 Android 12 以下的设备吗？

影响 KernelSU 兼容性的是设备内核的版本，它与设备的 Android 版本没有直接的关系。唯一有关联的是：**出厂** Android 12 的设备，一定是 5.10 或更高的内核（GKI设备）；因此结论如下：

1. 出厂 Android 12 的设备必定是支持的（GKI 设备）
2. 旧版本内核的设备（即使是 Android 12，也可能是旧内核）是兼容的（你需要自己编译内核）

## KernelSU 可以支持旧内核吗？

可以，目前最低支持到 4.14；更低的版本你需要手动移植它，欢迎 PR ！

## 如何为旧内核集成 KernelSU？

参考[教程](how-to-integrate-for-non-gki)

## 为什么我手机系统是 Android 13，但内核版本却是 "android12-5.10"?

内核版本与 Android 版本无关，如果你需要刷入 KernelSU，请永远使用**内核版本**而非 Android 版本，如果你为 "android12-5.10" 的设备刷入 Android 13 的内核，等待你的将是 bootloop.

## KernelSU 支持 --mount-master/全局挂载命名空间吗？

目前没有（未来可能会支持），但实际上有很多种办法手动进入全局命名空间，无需 su 内置支持，比如：

1. `nsenter -t 1 -m sh` 可以获得一个全局 mount namespace 的 shell.
2. 在你要执行的命令之前添加 `nsenter --mount=/proc/1/ns/mnt` 就可以让此命令在全局 mount namespace 下执行。 KernelSU 本身也使用了 [这种方法](https://github.com/tiann/KernelSU/blob/77056a710073d7a5f7ee38f9e77c9fd0b3256576/manager/app/src/main/java/me/weishu/kernelsu/ui/util/KsuCli.kt#L115)

## 我是 GKI1.0, 能用 KernelSU 吗?

GKI1 跟 GKI2 完全是两个东西，所以你需要自行编译内核。
