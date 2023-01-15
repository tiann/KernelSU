[English](README.md) | **中文**

# KernelSU

一个基于内核，为安卓 GKI 准备的 root 方案。

## 阅读之前

现在 KernelSU 已经支持 5.10 以下的旧内核，但**永远不会**有旧内核的 CI，因为它们不是通用的。
**任何关于如何编译旧内核的问题都不会得到任何答复并将被关闭。**

KernelSU 还处于早期开发阶段，你不应该生产环境中使用它。KernelSU 的开发者将不对你的任何损失负责。

如果你遇到任何问题，请打开 [issue](https://github.com/tiann/KernelSU/issues) 告诉我们！ (最好使用英语)

## 兼容状态

现在，KernelSU 可以在这些版本的内核上正常工作，不需要任何修改。

- `5.15`
- `5.10`
- `5.4`
- `4.19`
- `4.14`

目前支持架构 : `arm64-v8a` & `x86_64`

如果你确认 KernelSU 能在其他版本上工作，请打开一个 [issue](https://github.com/tiann/KernelSU/issues) 告诉我们！

## 使用方法

1. 用 KernelSU 刷入一个自定义的内核，你可以自己构建它或者[从 CI 下载](https://github.com/tiann/KernelSU/actions/workflows/build-kernel.yml)。
2. 安装管理器应用, 然后享受吧 :)

对于 5.10 以下的旧内核, 你必须自己构建。

## 构建

### 构建GKI内核

1. 首先下载 GKI 源代码，你可以参考[ GKI 构建说明](https://source.android.com/docs/setup/build/building-kernels)。
2. cd `< GKI 内核源代码目录 >`。
3. `curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -`。
4. 构建内核。

### 构建管理器应用

Android Studio / Gradle

### 讨论

[@KernelSU](https://t.me/KernelSU)

## 许可证

[GPL-3](http://www.gnu.org/copyleft/gpl.html)

## 鸣谢

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/)：KernelSU 的灵感。
- [true](https://github.com/brevent/genuine/)：apk v2 签名验证。
- [Diamorphine](https://github.com/m0nad/Diamorphine)：一些 rootkit 技巧。
- [Magisk](https://github.com/topjohnwu/Magisk)：sepolicy 的实现。
