[ 🇬🇧 English](README.md) | 🇪🇸 **Español** | [🇨🇳 简体中文](README_CN.md) | [🇹🇼 繁體中文](README_TW.md) | [ 🇯🇵 日本語](README_JP.md) | [🇵🇱 Polski](README_PL.md) | [🇧🇷 Português (Brasil)](README_PT-BR.md) | [🇹🇷 Türkçe](README_TR.md) | [🇷🇺Русский](README_RU.md) | [🇻🇳Tiếng Việt](README_VI.md) | [ɪᴅ indonesia](README_ID.md) | [עברית](README_IW.md) | [🇮🇳हिंदी](README_IN.md)

# KernelSU

<img src="https://kernelsu.org/logo.png" style="width: 96px;" alt="logo">

Una solución root basada en el kernel para dispositivos Android.

[![latest release badge](https://img.shields.io/github/v/release/tiann/KernelSU?label=Release&logo=github)](https://github.com/tiann/KernelSU/releases/latest)
[![weblate](https://img.shields.io/badge/Localization-Weblate-teal?logo=weblate)](https://hosted.weblate.org/engage/kernelsu)
[![Channel](https://img.shields.io/badge/Follow-Telegram-blue.svg?logo=telegram)](https://t.me/KernelSU)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-orange.svg?logo=gnu)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
[![GitHub License](https://img.shields.io/github/license/tiann/KernelSU?logo=gnu)](/LICENSE)

## 🚀 Características

**1.** Binario `su` basado en el kernel y gestión de acceso root.<br/>
**2.** Sistema de módulos basado en OverlayFS.

## ✅ Estado de compatibilidad

**KernelSU** soporta de forma oficial dispositivos Android con **GKI 2.0** (a partir de la versión **5.10** del kernel). Los kernels antiguos (a partir de la versión **4.14**) también son compatibles, pero necesitas compilarlos por tu cuenta.

El **Subsistema de Windows para Android (WSA)** e implementaciones de Android basadas en contenedores, como **Waydroid**, también deberían funcionar con **KernelSU** integrado.

Actualmente se soportan las siguientes **ABIs**: `arm64-v8a`; `x86_64`.

## 📖 Uso

[¿Cómo instalarlo?](https://kernelsu.org/guide/installation.html)

## 🔨 Compilación

[¿Cómo compilarlo?](https://kernelsu.org/guide/how-to-build.html)

## 💬 Discusión

- Telegram: [@KernelSU](https://t.me/KernelSU)

## ⚖️ Licencia

- Los archivos bajo el directorio `kernel` están licenciados bajo [GPL-2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html).
- Todas las demás partes, a excepción del directorio `kernel`, están licenciados bajo [GPL-3](https://www.gnu.org/licenses/gpl-3.0.html).

## 👥 Créditos

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/): la idea de **KernelSU**.
- [genuine](https://github.com/brevent/genuine/): la validación del **esquema de firmas APK v2**.
- [Diamorphine](https://github.com/m0nad/Diamorphine): algunas habilidades de rootkit.
- [Magisk](https://github.com/topjohnwu/Magisk): la implementación de la **política de SELinux (SEPolicy)**.
