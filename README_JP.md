[English](README.md) | [简体中文](README_CN.md) | [繁體中文](README_TW.md) | **日本語** | [Portuguese-Brazil](README_PT-BR.md) | [Türkçe](README_TR.md)

# KernelSU

Android におけるカーネルベースの root ソリューションです。

## 特徴

1. カーネルベースの `su` と権限管理
2. OverlayFS に基づくモジュールシステム

## 対応状況

KernelSU は GKI 2.0 デバイス（カーネルバージョン 5.10 以上）を公式にサポートしています。古いカーネル（4.14以上）とも互換性がありますが、自分でカーネルをビルドする必要があります。

WSA とコンテナ上で動作する Android でも KernelSU を統合して動かせます。

現在サポートしているアーキテクチャは `arm64-v8a` および `x86_64` です。

## 使用方法

[インストール方法はこちら](https://kernelsu.org/ja_JP/guide/installation.html)

## ビルド

[ビルド方法はこちら](https://kernelsu.org/guide/how-to-build.html)

### ディスカッション

- Telegram: [@KernelSU](https://t.me/KernelSU)

## ライセンス

- `kernel` ディレクトリの下にあるすべてのファイル： [GPL-2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
- `kernel` ディレクトリ以外のすべてのファイル： [GPL-3](https://www.gnu.org/licenses/gpl-3.0.html)

## クレジット

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/)：KernelSU のアイデア元
- [genuine](https://github.com/brevent/genuine/)：apk v2 の署名検証
- [Diamorphine](https://github.com/m0nad/Diamorphine): rootkit のスキル
- [Magisk](https://github.com/topjohnwu/Magisk)：sepolicy の実装
