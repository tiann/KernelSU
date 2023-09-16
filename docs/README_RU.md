[English](README.md) | [Español](README_ES.md) | [简体中文](README_CN.md) | [繁體中文](README_TW.md) | [日本語](README_JP.md) | [Polski](README_PL.md) | [Portuguese-Brazil](README_PT-BR.md) | [Türkçe](README_TR.md) | **Русский** | [Tiếng Việt](README_VI.md) | [Indonesia](README_ID.md)

# KernelSU

Решение на основе ядра root для Android-устройств.

## Особенности

1. Управление `su` и root-доступом на основе ядра.
2. Система модулей на основе overlayfs.

## Совместимость

KernelSU официально поддерживает устройства на базе Android GKI 2.0 (с ядром 5.10+), старые ядра (4.14+) также совместимы, но для этого необходимо собрать ядро самостоятельно.

WSA и Android на основе контейнеров также должны работать с интегрированным KernelSU.

В настоящее время поддерживаются следующие ABI: `arm64-v8a` и `x86_64`.

## Использование

[Установка](https://kernelsu.org/ru_RU/guide/installation.html)

## Сборка

[Как собрать?](https://kernelsu.org/ru_RU/guide/how-to-build.html)

## Обсуждение

- Telegram: [@KernelSU](https://t.me/KernelSU)

## Лицензия

- Файлы в директории `kernel` - [GPL-2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
- Все остальные части, кроме директории `kernel` - [GPL-3](https://www.gnu.org/licenses/gpl-3.0.html)

## Благодарности

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/): идея KernelSU.
- [genuine](https://github.com/brevent/genuine/): проверка подписи apk v2.
- [Diamorphine](https://github.com/m0nad/Diamorphine): некоторые навыки руткита.
- [Magisk](https://github.com/topjohnwu/Magisk): реализация sepolicy.
