[English](README.md) | [Español](README_ES.md) | [简体中文](README_CN.md) | [繁體中文](README_TW.md) | [日本語](README_JP.md) | [한국어](README_KR.md) | [Polski](README_PL.md) | [Português (Brasil)](README_PT-BR.md) | **Türkçe** | [Русский](README_RU.md) | [Tiếng Việt](README_VI.md) | [Indonesia](README_ID.md) | [עברית](README_IW.md) | [हिंदी](README_IN.md) | [Italiano](README_IT.md)

# KernelSU

<img src="https://kernelsu.org/logo.png" style="width: 96px;" alt="logo">

Android cihazlar için kernel tabanlı root çözümü.

[![Latest release](https://img.shields.io/github/v/release/tiann/KernelSU?label=Release&logo=github)](https://github.com/tiann/KernelSU/releases/latest)
[![Weblate](https://img.shields.io/badge/Localization-Weblate-teal?logo=weblate)](https://hosted.weblate.org/engage/kernelsu)
[![Channel](https://img.shields.io/badge/Follow-Telegram-blue.svg?logo=telegram)](https://t.me/KernelSU)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-orange.svg?logo=gnu)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
[![GitHub License](https://img.shields.io/github/license/tiann/KernelSU?logo=gnu)](/LICENSE)

## Özellikler

1. Kernel-tabanlı `su` ve root erişimi yönetimi.
2. [OverlayFS](https://en.wikipedia.org/wiki/OverlayFS)'ye dayalı modül sistemi.
3. [Uygulama profili](https://kernelsu.org/guide/app-profile.html): Root gücünü bir kafese kapatın.

## Uyumluluk Durumu

KernelSU resmi olarak Android GKI 2.0 cihazlarını (5.10+ kernelli) destekler, eski kernellerle de (4.14+) uyumludur, ancak kerneli kendinizin derlemeniz gerekir.

Bununla birlikte; WSA, ChromeOS ve konteyner tabanlı Android'in tamamı desteklenmektedir.

Şimdilik sadece `arm64-v8a` ve `x86_64` desteklenmektedir.

## Kullanım

- [Yükleme yönergeleri](https://kernelsu.org/guide/installation.html)
- [Nasıl derlenir?](https://kernelsu.org/guide/how-to-build.html)
- [Resmi WEB sitesi](https://kernelsu.org/)

## Çeviri

KernelSU'nun başka dillere çevrilmesine veya mevcut çevirilerin iyileştirilmesine yardımcı olmak için lütfen [Weblate](https://hosted.weblate.org/engage/kernelsu/) kullanın. Yönetici uygulamasının PR ile çevirisi, Weblate ile çakışacağından artık kabul edilmeyecektir.

## Tartışma

- Telegram: [@KernelSU](https://t.me/KernelSU)

## Güvenlik

KernelSU'daki güvenlik açıklarını bildirme hakkında bilgi için, bkz [SECURITY.md](/SECURITY.md).

## Lisans

- `kernel` klasöründeki dosyalar [GPL-2-only](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html) lisansı altındadır.
- `kernel` klasörü dışındaki bütün diğer bölümler [GPL-3-veya-sonraki](https://www.gnu.org/licenses/gpl-3.0.html) lisansı altındadır.

## Krediler

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/): KernelSU fikri.
- [Magisk](https://github.com/topjohnwu/Magisk): güçlü root aracı.
- [genuine](https://github.com/brevent/genuine/): apk v2 imza doğrulaması.
- [Diamorphine](https://github.com/m0nad/Diamorphine): bazı rootkit becerileri.
