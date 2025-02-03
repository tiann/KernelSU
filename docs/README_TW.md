[English](README.md) | [Español](README_ES.md) | [简体中文](README_CN.md) | **繁體中文** | [日本語](README_JP.md) | [한국어](README_KR.md) | [Polski](README_PL.md) | [Português (Brasil)](README_PT-BR.md) | [Türkçe](README_TR.md) | [Русский](README_RU.md) | [Tiếng Việt](README_VI.md) | [Indonesia](README_ID.md) | [עברית](README_IW.md) | [हिंदी](README_IN.md) | [Italiano](README_IT.md)

# KernelSU

<img src="https://kernelsu.org/logo.png" style="width: 96px;" alt="標誌">

一套基於 Android 裝置核心的 Root 解決方案。

[![最新版本](https://img.shields.io/github/v/release/tiann/KernelSU?label=%e7%99%bc%e8%a1%8c%e7%89%88%e6%9c%ac&logo=github)](https://github.com/tiann/KernelSU/releases/latest)
[![Weblate](https://img.shields.io/badge/%e6%9c%ac%e5%9c%9f%e5%8c%96%e7%bf%bb%e8%ad%af-Weblate-teal?logo=weblate)](https://hosted.weblate.org/engage/kernelsu)
[![頻道](https://img.shields.io/badge/%e8%bf%bd%e8%b9%a4-Telegram-blue.svg?logo=telegram)](https://t.me/KernelSU)
[![授權條款：《GPL v2》](https://img.shields.io/badge/%e6%8e%88%e6%ac%8a%e6%a2%9d%e6%ac%be-%E3%80%8AGPL%20v2%E3%80%8B-orange.svg?logo=gnu)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
[![GitHub 授權條款](https://img.shields.io/github/license/tiann/KernelSU?logo=gnu)](/LICENSE)

## 特色功能

1. 以核心內 `su` 管理 Root 存取。
2. 以 [OverlayFS](https://zh.wikipedia.org/zh-tw/OverlayFS) 運作模組系統。
3. [App Profile](https://kernelsu.org/zh_TW/guide/app-profile.html)：使 Root 掌握的生殺大權受制於此。

## 相容事態

理論上採以 Android GKI 2.0 的裝置（核心版本 5.10+），皆受 KernelSU 支援；採以老舊核心版本（4.14+）的裝置在手動建置核心後，亦受支援。

另可在 WSA、ChromeOS 一類的容器式 Android 中運作。

目前僅適用 `arm64-v8a` 以及 `x86_64` 架構。

## 使用手冊

- [安裝教學](https://kernelsu.org/zh_TW/guide/installation.html)
- [如何建置 KernelSU？](https://kernelsu.org/zh_TW/guide/how-to-build.html)
- [官方網站](https://kernelsu.org/zh_TW/)

## 多語翻譯

欲要協助 KernelSU 邁向多語化，抑或改進翻譯品質，請前往 [Weblate](https://hosted.weblate.org/engage/kernelsu/) 進行翻譯。為避免與 Weblate 上的翻譯發生衝突，現已不再受理翻譯相關的管理工具 PR。

## 綜合討論

- Telegram：[@KernelSU](https://t.me/KernelSU)

## 安全政策

欲要得知、回報 KernelSU 的安全性漏洞，請參閱 [SECURITY.md](/SECURITY.md)。

## 授權條款

- 位於 `kernel` 資料夾的檔案以[《GPL-2.0-only》](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)規範。
- 非位於 `kernel` 資料夾的其他檔案以[《GPL-3.0-or-later》](https://www.gnu.org/licenses/gpl-3.0.html)規範。

## 致謝名單

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/)：KernelSU 的靈感來源。
- [Magisk](https://github.com/topjohnwu/Magisk)：強而有力的 Root 工具。
- [genuine](https://github.com/brevent/genuine/)：用於確效 Apk v2 簽章。
- [Diamorphine](https://github.com/m0nad/Diamorphine): 用於增進 Rootkit 技巧。
