[English](README.md) | **简体中文** | [繁體中文](README_TW.md) | [日本語](README_JP.md) | [Portuguese-Brazil](README_PT-BR.md)

# KernelSU

一个 Android 上基于内核的 root 方案。

## 特性

- 基于内核的 su 和权限管理。
- 基于 overlayfs 的模块系统。

## 兼容状态

KernelSU 官方支持 GKI 2.0 的设备（内核版本5.10以上）；旧内核也是兼容的（最低4.14+），不过需要自己编译内核。

WSA 和运行在容器上的 Android 也可以与 KernelSU 一起工作。

目前支持架构 : `arm64-v8a` 和 `x86_64`

## 使用方法

[安装教程](https://kernelsu.org/zh_CN/guide/installation.html)

## 构建

[如何构建？](https://kernelsu.org/zh_CN/guide/how-to-build.html)

### 讨论

- Telegram: [@KernelSU](https://t.me/KernelSU)

## 许可证

- 目录 `kernel` 下所有文件为 [GPL-2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
- 除 `kernel` 目录的其他部分均为 [GPL-3](https://www.gnu.org/licenses/gpl-3.0.html)

## 鸣谢

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/)：KernelSU 的灵感。
- [genuine](https://github.com/brevent/genuine/)：apk v2 签名验证。
- [Diamorphine](https://github.com/m0nad/Diamorphine)：一些 rootkit 技巧。
- [Magisk](https://github.com/topjohnwu/Magisk)：sepolicy 的实现。
