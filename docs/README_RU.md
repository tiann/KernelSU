[English](README.md) | [Español](README_ES.md) | [简体中文](README_CN.md) | [繁體中文](README_TW.md) | [日本語](README_JP.md) | [한국어](README_KR.md) | [Polski](README_PL.md) | [Português (Brasil)](README_PT-BR.md) | [Türkçe](README_TR.md) | **Русский** | [Tiếng Việt](README_VI.md) | [Indonesia](README_ID.md) | [עברית](README_IW.md) | [हिंदी](README_IN.md)

# KernelSU

<img src="https://kernelsu.org/logo.png" style="width: 96px;" alt="logo">

Решение на основе ядра root для Android-устройств.

[![Latest release](https://img.shields.io/github/v/release/tiann/KernelSU?label=Release&logo=github)](https://github.com/tiann/KernelSU/releases/latest)
[![Weblate](https://img.shields.io/badge/Localization-Weblate-teal?logo=weblate)](https://hosted.weblate.org/engage/kernelsu)
[![Channel](https://img.shields.io/badge/Follow-Telegram-blue.svg?logo=telegram)](https://t.me/KernelSU)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-orange.svg?logo=gnu)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
[![GitHub License](https://img.shields.io/github/license/tiann/KernelSU?logo=gnu)](/LICENSE)

## Особенности

1. Управление `su` и root-доступом на основе ядра.
2. Система модулей на основе [OverlayFS](https://en.wikipedia.org/wiki/OverlayFS).
3. [Профиль приложений](https://kernelsu.org/ru_RU/guide/app-profile.html): Запри корневую силу в клетке.

## Совместимость

KernelSU официально поддерживает устройства на базе Android GKI 2.0 (с ядром 5.10+), старые ядра (4.14+) также совместимы, но для этого необходимо собрать ядро самостоятельно.

WSA и Android на основе контейнеров также должны работать с интегрированным KernelSU.

В настоящее время поддерживаются следующие ABI: `arm64-v8a` и `x86_64`.

## Использование

- [Установка](https://kernelsu.org/ru_RU/guide/installation.html)
- [Как собрать?](https://kernelsu.org/ru_RU/guide/how-to-build.html)
- [официальный сайт](https://kernelsu.org/ru_RU/)

## Обсуждение

- Telegram: [@KernelSU](https://t.me/KernelSU)

## Лицензия

- Файлы в директории `kernel` [GPL-2-only](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html).
- Все остальные части, кроме директории `kernel` [GPL-3-or-later](https://www.gnu.org/licenses/gpl-3.0.html).

## Благодарности

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/): идея KernelSU.
- [Magisk](https://github.com/topjohnwu/Magisk): реализация sepolicy.
- [genuine](https://github.com/brevent/genuine/): проверка подписи apk v2.
- [Diamorphine](https://github.com/m0nad/Diamorphine): некоторые навыки руткита.
