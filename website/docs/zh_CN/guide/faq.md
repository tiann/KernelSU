# 常见问题

## KernelSU 是否支持我的设备 ？

首先，您的设备应该能够解锁 bootloader。 如果不能，则不支持。

然后在你的设备上安装 KernelSU 管理器并打开它，如果它显示 `Unsupported` ，那么你的设备是不受支持的，以后也不会得到支持。

## KernelSU 是否需要解锁 Bootloader ？

当然。

## KernelSU 是否支持模块 ？

是的，但它是早期版本，可能有问题。请等待它稳定 :)

## KernelSU 是否支持 Xposed ？

是的，[Dreamland](https://github.com/canyie/Dreamland) 和 [TaiChi](https::/taichi.cool) 现在部分可用，我们正在努力使其他 Xposed Framework 可用。

## KernelSU 与 Magisk 兼容吗 ？

KernelSU 的模块系统与 Magisk 的 magic mount 有冲突，如果 KernelSU 中启用了任何模块，那么整个 Magisk 将无法工作。

但是如果你只使用 KernelSU 的 `su`，那么它会和 Magisk 一起工作：KernelSU 修改 `kernel` 、 Magisk 修改 `ramdisk`，它们可以一起工作。

## KernelSU 会替代 Magisk 吗？

我们不这么认为，这不是我们的目标。Magisk 对于用户空间 root 解决方案来说已经足够好了，而且它的寿命会很长。KernelSU 的目标是为用户提供内核接口，而不是替代 Magisk。

## KernelSU 可以支持非 GKI 设备吗？

可以。但是你应该下载内核源代码并将 KernelSU 集成到源代码树中并自己编译内核。

## KernelSU 可以支持旧内核吗？

可以，但你需要手动移植它，欢迎 PR ！

## 如何为旧内核基础 KernelSU？

参考[教程](how-to-integrate-for-non-gki)
