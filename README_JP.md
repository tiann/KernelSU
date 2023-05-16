[English](README.md) | [简体中文](README_CN.md) | [繁體中文](README_TW.md) | **日本語**

# KernelSU

Androidにおけるカーネルベースのルートソリューション。

## 特徴

- カーネルベースのsuと権限管理。
- overlayfsに基づくモジュールシステム。

## 対応状況

KernelSUはGKI 2.0デバイス（カーネルバージョン5.10以上）を公式にサポートしています。古いカーネルも互換性がありますが（最小4.14以上）、自分でカーネルをコンパイルする必要があります。

コンテナ上で動作するWSAとAndroidもKernelSUで動作します。

現在サポートされているアーキテクチャ : `arm64-v8a` および `x86_64` です。

## 使用方法

[インストールチュートリアル](https://kernelsu.org/zh_CN/guide/installation.html)

## 構築する

[どうやって構築するのでしょうか？](https://kernelsu.org/zh_CN/guide/how-to-build.html)

### ディスカッション

- Telegram: [@KernelSU](https://t.me/KernelSU)

## 許可証

- ディレクトリ `kernel` の下にある全てのファイルは [GPL-2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
- ディレクトリの `kernel` 以外の部分は全て [GPL-3](https://www.gnu.org/licenses/gpl-3.0.html)

## 謝辞

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/)：KernelSUのきっかけとなった。
- [genuine](https://github.com/brevent/genuine/)：apk v2の署名検証
- [Diamorphine](https://github.com/m0nad/Diamorphine): いくつかのrootkitのTips。
- [Magisk](https://github.com/topjohnwu/Magisk)：sepolicyの実装です。
