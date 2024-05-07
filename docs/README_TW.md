[English](README.md) | [Español](README_ES.md) | [简体中文](README_CN.md) | **繁體中文** | [日本語](README_JP.md) | [한국어](README_KR.md) | [Polski](README_PL.md) | [Português (Brasil)](README_PT-BR.md) | [Türkçe](README_TR.md) | [Русский](README_RU.md) | [Tiếng Việt](README_VI.md) | [Indonesia](README_ID.md) | [עברית](README_IW.md) | [हिंदी](README_IN.md)

# KernelSU

<img src="https://kernelsu.org/logo.png" style="width: 96px;" alt="logo">

一個基於核心的 Android 裝置 Root 解決方案

[![Latest release](https://img.shields.io/github/v/release/tiann/KernelSU?label=Release&logo=github)](https://github.com/tiann/KernelSU/releases/latest)
[![Weblate](https://img.shields.io/badge/Localization-Weblate-teal?logo=weblate)](https://hosted.weblate.org/engage/kernelsu)
[![Channel](https://img.shields.io/badge/Follow-Telegram-blue.svg?logo=telegram)](https://t.me/KernelSU)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-orange.svg?logo=gnu)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
[![GitHub License](https://img.shields.io/github/license/tiann/KernelSU?logo=gnu)](/LICENSE)

## 功能

- 基於核心的 `su` 和 Root 存取權管理。
- 基於 [OverlayFS](https://en.wikipedia.org/wiki/OverlayFS) 的模組系統。
- [App Profile](https://kernelsu.org/zh_TW/guide/app-profile.html): 將 Root 的權限鎖在牢籠中.

## 相容性狀態

KernelSU 官方支援 Android GKI 2.0 的裝置 (核心版本 5.10+ )；舊版核心同樣相容 (最低 4.14+ )，但需要自行編譯核心。

WSA和ChromeOS和執行在容器中的 Android 也可以與 KernelSU 一同運作。

目前支援架構：`arm64-v8a` 和 `x86_64`。

## 使用方法

- [安裝教學](https://kernelsu.org/zh_TW/guide/installation.html)
- [如何建置？](https://kernelsu.org/zh_TW/guide/how-to-build.html)
- [官方網站](https://kernelsu.org/zh_TW/)

## 翻譯

若要協助翻譯 KernelSU 或改進現有翻譯，請使用 [Weblate](https://hosted.weblate.org/engage/kernelsu/)。 翻譯管理器的PR不再被接受，因為它會與Weblate衝突。

### 討論

- Telegram：[@KernelSU](https://t.me/KernelSU)

## 安全
有關報告 KernelSU 中的安全漏洞的資訊，請參閱 [SECURITY.md](/SECURITY.md)。

## 授權

- 目錄 `kernel` 下所有檔案為 [GPL-2-only](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)。
- 除 `kernel` 目錄的其他部分均為 [GPL-3-or-later](https://www.gnu.org/licenses/gpl-3.0.html)。

## 致謝

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/)：KernelSU 的靈感。
- [Magisk](https://github.com/topjohnwu/Magisk)：sepolicy 實作。
- [genuine](https://github.com/brevent/genuine/)：apk v2 簽章驗證。
- [Diamorphine](https://github.com/m0nad/Diamorphine)：一些 rootkit 技巧。
