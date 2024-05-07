[English](README.md) | **Español** | [简体中文](README_CN.md) | [繁體中文](README_TW.md) | [日本語](README_JP.md) | [한국어](README_KR.md) | [Polski](README_PL.md) | [Português (Brasil)](README_PT-BR.md) | [Türkçe](README_TR.md) | [Русский](README_RU.md) | [Tiếng Việt](README_VI.md) | [Indonesia](README_ID.md) | [עברית](README_IW.md) | [हिंदी](README_IN.md)

# KernelSU

<img src="https://kernelsu.org/logo.png" style="width: 96px;" alt="logo">

Una solución root basada en el kernel para dispositivos Android.

[![Latest release](https://img.shields.io/github/v/release/tiann/KernelSU?label=Release&logo=github)](https://github.com/tiann/KernelSU/releases/latest)
[![Weblate](https://img.shields.io/badge/Localización-Weblate-teal?logo=weblate)](https://hosted.weblate.org/engage/kernelsu)
[![Channel](https://img.shields.io/badge/Seguir-Telegram-blue.svg?logo=telegram)](https://t.me/KernelSU)
[![License: GPL v2](https://img.shields.io/badge/Licencia-GPL%20v2-orange.svg?logo=gnu)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
[![GitHub License](https://img.shields.io/github/license/tiann/KernelSU?logo=gnu)](/LICENSE)

## Características

1. Binario `su` basado en el kernel y gestión de acceso root.
2. Sistema de módulos basado en [OverlayFS](https://en.wikipedia.org/wiki/OverlayFS).

## Estado de compatibilidad

**KernelSU** soporta de forma oficial dispositivos Android con **GKI 2.0** (a partir de la versión **5.10** del kernel). Los kernels antiguos (a partir de la versión **4.14**) también son compatibles, pero necesitas compilarlos por tu cuenta.

Con esto, WSA, ChromeOS y Android basado en contenedores están todos compatibles.

Actualmente, solo se admiten las arquitecturas `arm64-v8a` y `x86_64`.

## Uso

- [¿Cómo instalarlo?](https://kernelsu.org/guide/installation.html)
- [¿Cómo compilarlo?](https://kernelsu.org/guide/how-to-build.html)
- [Site oficial](https://kernelsu.org/)

## Traducción

Para ayudar a traducir KernelSU o mejorar las traducciones existentes, utilice [Weblate](https://hosted.weblate.org/engage/kernelsu/). Ya no se aceptan PR de la traducción de Manager porque entrará en conflicto con Weblate.

## Discusión

- Telegram: [@KernelSU](https://t.me/KernelSU)

## Seguridad

Para obtener información sobre cómo informar vulnerabilidades de seguridad en KernelSU, consulte [SECURITY.md](/SECURITY.md).

##  Licencia

- Los archivos bajo el directorio `kernel` están licenciados bajo [GPL-2-only](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html).
- Todas las demás partes, a excepción del directorio `kernel`, están licenciados bajo [GPL-3-or-later](https://www.gnu.org/licenses/gpl-3.0.html).

## Créditos

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/): la idea de KernelSU.
- [Magisk](https://github.com/topjohnwu/Magisk): la poderosa herramienta root.
- [genuine](https://github.com/brevent/genuine/): validación de firma apk v2.
- [Diamorphine](https://github.com/m0nad/Diamorphine): algunas habilidades de rootkit.
