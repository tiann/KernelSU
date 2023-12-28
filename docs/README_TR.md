[English](README.md) | [Español](README_ES.md) | [简体中文](README_CN.md) | [繁體中文](README_TW.md) | [日本語](README_JP.md) | [Polski](README_PL.md) | [Portuguese-Brazil](README_PT-BR.md) | **Türkçe** | [Русский](README_RU.md) | [Tiếng Việt](README_VI.md) | [Indonesia](README_ID.md) | [עברית](README_iw.md) | [हिंदी](README_IN.md)

# KernelSU

<img src="https://kernelsu.org/logo.png" style="width: 96px;" alt="logo">

Android cihazlar için kernel tabanlı root çözümü.

[![latest release badge](https://img.shields.io/github/v/release/tiann/KernelSU?label=Release&logo=github)](https://github.com/tiann/KernelSU/releases/latest)
[![weblate](https://img.shields.io/badge/Localization-Weblate-teal?logo=weblate)](https://hosted.weblate.org/engage/kernelsu)
[![Channel](https://img.shields.io/badge/Follow-Telegram-blue.svg?logo=telegram)](https://t.me/KernelSU)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-orange.svg?logo=gnu)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
[![GitHub License](https://img.shields.io/github/license/tiann/KernelSU?logo=gnu)](/LICENSE)


## Özellikler

1. Kernel-tabanlı `su` ve root erişimi yönetimi.
2. Overlayfs'ye dayalı modül sistemi.
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

KernelSU'nun çevirisine veya mevcut çevirilerin iyileştirilmesine yardımcı olmak için lütfen [Weblate](https://hosted.weblate.org/engage/kernelsu/) kullanın. Yönetici uygulamasının PR ile çevirisi, Weblate ile çakışacağından artık kabul edilmeyecektir.

## Tartışma

- Telegram: [@KernelSU](https://t.me/KernelSU)

## Lisans

- `kernel` klasöründeki dosyalar [GPL-2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html) lisansı altındadır.
- `kernel` klasörü dışındaki bütün diğer bölümler [GPL-3](https://www.gnu.org/licenses/gpl-3.0.html) lisansı altındadır.

## Krediler

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/): KernelSU fikri.
- [Magisk](https://github.com/topjohnwu/Magisk): güçlü root aracı.
- [genuine](https://github.com/brevent/genuine/): apk v2 imza doğrulaması.
- [Diamorphine](https://github.com/m0nad/Diamorphine): bazı rootkit becerileri.
