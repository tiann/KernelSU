[English](README.md) | [Español](README_ES.md) | [简体中文](README_CN.md) | [繁體中文](README_TW.md) | **日本語** | [한국어](README_KR.md) | [Polski](README_PL.md) | [Português (Brasil)](README_PT-BR.md) | [Türkçe](README_TR.md) | [Русский](README_RU.md) | [Tiếng Việt](README_VI.md) | [Indonesia](README_ID.md) | [עברית](README_IW.md) | [हिंदी](README_IN.md)

# KernelSU

<img src="https://kernelsu.org/logo.png" style="width: 96px;" alt="logo">

Android におけるカーネルベースの root ソリューションです。

[![Latest release](https://img.shields.io/github/v/release/tiann/KernelSU?label=Release&logo=github)](https://github.com/tiann/KernelSU/releases/latest)
[![Weblate](https://img.shields.io/badge/Localization-Weblate-teal?logo=weblate)](https://hosted.weblate.org/engage/kernelsu)
[![Channel](https://img.shields.io/badge/Follow-Telegram-blue.svg?logo=telegram)](https://t.me/KernelSU)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-orange.svg?logo=gnu)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
[![GitHub License](https://img.shields.io/github/license/tiann/KernelSU?logo=gnu)](/LICENSE)

## 特徴

1. カーネルベースの `su` と権限管理。
2. [OverlayFS](https://en.wikipedia.org/wiki/OverlayFS) に基づくモジュールシステム。
3. [アプリのプロファイル](https://kernelsu.org/guide/app-profile.html): root の権限をケージ内に閉じ込めます。

## 対応状況

KernelSU は GKI 2.0 デバイス（カーネルバージョン 5.10 以上）を公式にサポートしています。古いカーネル（4.14以上）とも互換性がありますが、自分でカーネルをビルドする必要があります。

WSA 、ChromeOS とコンテナ上で動作する Android でも KernelSU を統合して動かせます。

現在サポートしているアーキテクチャは `arm64-v8a` および `x86_64` です。

## 使用方法

- [インストール方法はこちら](https://kernelsu.org/ja_JP/guide/installation.html)
- [ビルド方法はこちら](https://kernelsu.org/guide/how-to-build.html)
- [公式サイト](https://kernelsu.org/ja_JP/)

## 翻訳

KernelSU をあなたの言語に翻訳するか、既存の翻訳を改善するには、[Weblate](https://hosted.weblate.org/engage/kernelsu/) を使用してください。Manager翻訳した PR は、Weblate と競合するため受け入れられなくなりました。

## ディスカッション

- Telegram: [@KernelSU](https://t.me/KernelSU)

## ライセンス

- `kernel` ディレクトリの下にあるすべてのファイル： [GPL-2-only](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)。
- `kernel` ディレクトリ以外のすべてのファイル： [GPL-3.0-or-later](https://www.gnu.org/licenses/gpl-3.0.html)。

## クレジット

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/)：KernelSU のアイデア元。
- [Magisk](https://github.com/topjohnwu/Magisk)：強力な root ツール。
- [genuine](https://github.com/brevent/genuine/)：apk v2 の署名検証。
- [Diamorphine](https://github.com/m0nad/Diamorphine): rootkit のスキル。
