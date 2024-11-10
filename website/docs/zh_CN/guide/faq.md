# 常见问题

## KernelSU 是否支持我的设备 ？

首先，您的设备应该能够解锁 bootloader。 如果不能，则不支持。

然后在你的设备上安装 KernelSU 管理器并打开它，如果它显示 `不支持` ，那么你的设备没有官方支持的开箱即用的 boot image；但你可以自己编译内核集成 KernelSU 进而使用它。

## KernelSU 是否需要解锁 Bootloader ？

当然需要。

## KernelSU 是否支持模块 ？

支持。请查阅 [模块](module.md)。

## KernelSU 是否支持 Xposed ？

支持。LSPosed 可以在 [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext) 的支持下正常运行。

## KernelSU 支持 Zygisk 吗?

KernelSU 本体不支持 Zygisk，但是你可以用 [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext) 来使用 Zygisk 模块。

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

## 我是 GKI1.0, 能用 KernelSU 吗?

GKI1 跟 GKI2 完全是两个东西，所以你需要自行编译内核。

## 如何把 `/system` 变成挂载为可读写？

我们不建议你直接修改系统分区，你应该使用[模块功能](module.md) 来做修改；如果你执意要这么做，可以看看 [magisk_overlayfs](https://github.com/HuskyDG/magic_overlayfs)

## KernelSU 能修改 hosts 吗，我如何使用 AdAway？

当然可以。但这个功能 KernelSU 没有内置，你可以安装这个 [systemless-hosts](https://github.com/symbuzzer/systemless-hosts-KernelSU-module)

## 为什么有个 1 TB 的超大文件？

1 TB 大小的 `modules.img` 是一个磁盘镜像文件，**不要担心它的大小**，它是一种被称之为[稀疏文件](https://en.wikipedia.org/wiki/Sparse_file)的文件格式，它的实际大小只有你使用的模块的大小，并且你在删除模块后它会动态缩小；它并不实际占用 1 TB 大小的磁盘空间（实际上你手机可能并没有这么多空间）。

如果你真的对这个文件的大小感到不爽，你可以使用 `resize2fs -M` 命令让它变成实际大小；但此时模块可能无法正常工作，我们也不会为此提供任何支持。
