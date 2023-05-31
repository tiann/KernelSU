[English](README.md) | [简体中文](README_CN.md) | **繁體中文** | [日本語](README_JP.md) | [Portuguese-Brazil](README_PT-BR.md)

# KernelSU

一個基於核心的 Android 裝置 Root 解決方案

## 功能

- 基於核心的 Su 和 Root 存取權管理。
- 基於 Overlayfs 的模組系統。

## 相容性狀態

KernelSU 官方支援 Android GKI 2.0 的裝置 (核心版本 5.10+)；舊版核心同樣相容 (最低 4.14+)，但需要自行編譯核心。

WSA 和執行在容器中的 Android 也可以與 KernelSU 一同運作。

目前支援架構：`arm64-v8a` 和 `x86_64`

## 使用方法

[安裝教學](https://kernelsu.org/zh_TW/guide/installation.html)

## 建置

[如何建置？](https://kernelsu.org/zh_TW/guide/how-to-build.html)

### 討論

- Telegram：[@KernelSU](https://t.me/KernelSU)

## 授權

- 目錄 `kernel` 下所有檔案為 [GPL-2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
- 除 `kernel` 目錄的其他部分均為 [GPL-3](https://www.gnu.org/licenses/gpl-3.0.html)

## 致謝

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/)：KernelSU 的靈感。
- [genuine](https://github.com/brevent/genuine/)：apk v2 簽章驗證。
- [Diamorphine](https://github.com/m0nad/Diamorphine)：一些 rootkit 技巧。
- [Magisk](https://github.com/topjohnwu/Magisk)：sepolicy 實作。
