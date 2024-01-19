[English](README.md) | [Español](README_ES.md) | [简体中文](README_CN.md) | [繁體中文](README_TW.md) | [日本語](README_JP.md) | **Polski** | [Português (Brasil)](README_PT-BR.md) | [Türkçe](README_TR.md) | [Русский](README_RU.md) | [Tiếng Việt](README_VI.md) | [Indonesia](README_ID.md) | [עברית](README_IW.md) | [हिंदी](README_IN.md)

# KernelSU

<img src="https://kernelsu.org/logo.png" style="width: 96px;" alt="logo">

Rozwiązanie root oparte na jądrze dla urządzeń z systemem Android.

[![latest release badge](https://img.shields.io/github/v/release/tiann/KernelSU?label=Release&logo=github)](https://github.com/tiann/KernelSU/releases/latest)
[![weblate](https://img.shields.io/badge/Localization-Weblate-teal?logo=weblate)](https://hosted.weblate.org/engage/kernelsu)
[![Channel](https://img.shields.io/badge/Follow-Telegram-blue.svg?logo=telegram)](https://t.me/KernelSU)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-orange.svg?logo=gnu)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
[![GitHub License](https://img.shields.io/github/license/tiann/KernelSU?logo=gnu)](/LICENSE)

## Cechy

1. Oparte na jądrze `su` i zarządzanie dostępem roota.
2. System modułów oparty na OverlayFS.

## Kompatybilność

KernelSU oficjalnie obsługuje urządzenia z Androidem GKI 2.0 (z jądrem 5.10+), starsze jądra (4.14+) są również kompatybilne, ale musisz sam skompilować jądro.

WSA i Android oparty na kontenerach również powinny działać ze zintegrowanym KernelSU.

Aktualnie obsługiwane ABI to : `arm64-v8a` i `x86_64`.

## Użycie

[Instalacja](https://kernelsu.org/guide/installation.html)

## Kompilacja

[Jak skompilować?](https://kernelsu.org/guide/how-to-build.html)

## Dyskusja

- Telegram: [@KernelSU](https://t.me/KernelSU)

## Licencja

- Pliki w katalogu `kernel` są na licencji [GPL-2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html).
- Wszystkie inne części poza katalogiem `kernel` są na licencji [GPL-3](https://www.gnu.org/licenses/gpl-3.0.html).

## Podziękowania

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/): pomysłodawca KernelSU.
- [genuine](https://github.com/brevent/genuine/): walidacja podpisu apk v2.
- [Diamorphine](https://github.com/m0nad/Diamorphine): cenna znajomość rootkitów.
- [Magisk](https://github.com/topjohnwu/Magisk): implementacja sepolicy.
