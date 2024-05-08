[English](README.md) | [Español](README_ES.md) | [简体中文](README_CN.md) | [繁體中文](README_TW.md) | [日本語](README_JP.md) | [한국어](README_KR.md) | [Polski](README_PL.md) | [Português (Brasil)](README_PT-BR.md) | [Türkçe](README_TR.md) | [Русский](README_RU.md) | [Tiếng Việt](README_VI.md) | [Indonesia](README_ID.md) | **עברית** | [हिंदी](README_IN.md)

# KernelSU

<img src="https://kernelsu.org/logo.png" style="width: 96px;" alt="logo">

פתרון לניהול root מבוסס על Kernel עבור מכשירי Android.

[![Latest release](https://img.shields.io/github/v/release/tiann/KernelSU?label=Release&logo=github)](https://github.com/tiann/KernelSU/releases/latest)
[![Weblate](https://img.shields.io/badge/Localization-Weblate-teal?logo=weblate)](https://hosted.weblate.org/engage/kernelsu)
[![Channel](https://img.shields.io/badge/Follow-Telegram-blue.svg?logo=telegram)](https://t.me/KernelSU)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-orange.svg?logo=gnu)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
[![GitHub License](https://img.shields.io/github/license/tiann/KernelSU?logo=gnu)](/LICENSE)

## תכונות

1. ניהול root ו־`su` מבוססים על Kernel.
2. מערכת מודולים מבוססת [OverlayFS](https://en.wikipedia.org/wiki/OverlayFS).
3. [פרופיל אפליקציה](https://kernelsu.org/guide/app-profile.html): נעילת גישת root בכלוב.

## מצב תאימות

KernelSU תומך במכשירי Android GKI 2.0 (kernel 5.10+) באופן רשמי. לליבות ישנות (4.14+) יש גם תאימות, אך יידרש לבנות את הליבה באופן ידני.

באמצעות זה, תמיכה זמינה גם ל-WSA, ChromeOS ומכשירי Android המבוססים על מיכלים.

כרגע, רק `arm64-v8a` ו־`x86_64` נתמכים.

## שימוש

- [הוראות התקנה](https://kernelsu.org/guide/installation.html)
- [איך לבנות?](https://kernelsu.org/guide/how-to-build.html)
- [האתר רשמי](https://kernelsu.org/)

## תרגום

כדי לעזור בתרגום של KernelSU או לשפר תרגומים קיימים, יש להשתמש ב-[Weblate](https://hosted.weblate.org/engage/kernelsu/).

## דיון

- Telegram: [@KernelSU](https://t.me/KernelSU)

## רשיון

- קבצים תחת הספרייה `kernel` מוגנים על פי [GPL-2.0-only](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html).
- כל החלקים האחרים, למעט הספרייה `kernel`, מוגנים על פי [GPL-3.0-or-later](https://www.gnu.org/licenses/gpl-3.0.html).

## קרדיטים

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/): הרעיון של KernelSU.
- [Magisk](https://github.com/topjohnwu/Magisk): הכלי הסופר חזק לניהול root.
- [genuine](https://github.com/brevent/genuine/): אימות חתימת apk v2.
- [Diamorphine](https://github.com/m0nad/Diamorphine): כמה יכולות רוט.
