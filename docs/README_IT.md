[English](REAME.md) | [Español](README_ES.md) | [简体中文](README_CN.md) | [繁體中文](README_TW.md) | [日本語](README_JP.md) | [한국어](README_KR.md) | [Polski](README_PL.md) | [Português (Brasil)](README_PT-BR.md) | [Türkçe](README_TR.md) | [Русский](README_RU.md) | [Tiếng Việt](README_VI.md) | [Indonesia](README_ID.md) | [עברית](README_IW.md) | [हिंदी](README_IN.md) | **Italiano**

# KernelSU

<img src="https://kernelsu.org/logo.png" style="width: 96px;" alt="logo">

Una soluzione per il root basata sul kernel per i dispositivi Android. 

[![Latest release](https://img.shields.io/github/v/release/tiann/KernelSU?label=Release&logo=github)](https://github.com/tiann/KernelSU/releases/latest)
[![Weblate](https://img.shields.io/badge/Localization-Weblate-teal?logo=weblate)](https://hosted.weblate.org/engage/kernelsu)
[![Canale Telegraml](https://img.shields.io/badge/Follow-Telegram-blue.svg?logo=telegram)](https://t.me/KernelSU)
[![Licenza componenti kernel: GPL v2](https://img.shields.io/badge/License-GPL%20v2-orange.svg?logo=gnu)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
[![Licenza elementi non kern](https://img.shields.io/github/license/tiann/KernelSU?logo=gnu)](/LICENSE)

## Funzionalità

1. `su` e accesso root basato sul kernel.
2. Sistema di moduli per la modifica del sistema basato su [OverlayFS](https://en.wikipedia.org/wiki/OverlayFS).
3. [App profile](https://kernelsu.org/guide/app-profile.html): Limita i poteri dell'accesso root a permessi specifici.

## Compatibilità

KernelSU supporta ufficialmente i dispositivi Android GKI 2.0 (kernel 5.10 o superiore). I kernel precedenti (kernel 4.14+) sono anche compatibili, ma il kernel deve essere compilato manualmente.

Questo implica che WSA, ChromeOS e tutti le varianti di Android basate su container e virtualizzazione sono supportate.

Allo stato attuale solo le architetture a 64-bit ARM (arm64-v8a) e x86 (x86_64) sono supportate.

## Utilizzo

- [Istruzioni per l'installazione](https://kernelsu.org/guide/installation.html)
- [Come compilare manualmente?](https://kernelsu.org/guide/how-to-build.html)
- [Sito web ufficiale](https://kernelsu.org/)

## Traduzioni

Per aiutare a tradurre KernelSU o migliorare le traduzioni esistenti, si è pregati di utilizzare 
To help translate KernelSU or improve existing translations, please use [Weblate](https://hosted.weblate.org/engage/kernelsu/). Le richieste di pull delle traduzioni del manager non saranno più accettate perché sarebbero in conflitto con Weblate.

## Discussione

- Telegram: [@KernelSU](https://t.me/KernelSU)

## Securezza

Per informazioni riguardo la segnalazione di vulnerabilità di sicurezza per KernelSU, leggi [SECURITY.md](/SECURITY.md).

## Licenza

- I file nella cartella `kernel` sono forniti secondo la licenza [GPL-2.0-only](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html).
- Tutte le altre parti, ad eccezione della certella `kernel`, seguono la licenza [GPL-3.0-or-later](https://www.gnu.org/licenses/gpl-3.0.html).

## Riconoscimenti e attribuzioni

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/): l'idea alla base di KernelSU.
- [Magisk](https://github.com/topjohnwu/Magisk): la potente utilità per il root.
- [genuine](https://github.com/brevent/genuine/): verifica della firma apk v2.
- [Diamorphine](https://github.com/m0nad/Diamorphine): alcune capacità di rootkit.
