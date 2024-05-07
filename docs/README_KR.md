[English](README.md) | [Español](README_ES.md) | [简体中文](README_CN.md) | [繁體中文](README_TW.md) | [日本語](README_JP.md) | **한국어** | [Polski](README_PL.md) | [Português (Brasil)](README_PT-BR.md) | [Türkçe](README_TR.md) | [Русский](README_RU.md) | [Tiếng Việt](README_VI.md) | [Indonesia](README_ID.md) | [עברית](README_IW.md) | [हिंदी](README_IN.md)

# KernelSU

<img src="https://kernelsu.org/logo.png" style="width: 96px;" alt="logo">

안드로이드 기기에서 사용되는 커널 기반 루팅 솔루션입니다.

[![Latest release](https://img.shields.io/github/v/release/tiann/KernelSU?label=Release&logo=github)](https://github.com/tiann/KernelSU/releases/latest)
[![Weblate](https://img.shields.io/badge/Localization-Weblate-teal?logo=weblate)](https://hosted.weblate.org/engage/kernelsu)
[![Channel](https://img.shields.io/badge/Follow-Telegram-blue.svg?logo=telegram)](https://t.me/KernelSU)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-orange.svg?logo=gnu)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
[![GitHub License](https://img.shields.io/github/license/tiann/KernelSU?logo=gnu)](/LICENSE)

## 기능들

1. 커널 기반 `su` 및 루트 액세스 관리.
2. [OverlayFS](https://en.wikipedia.org/wiki/OverlayFS) 기반 모듈 시스템.
3. [App Profile](https://kernelsu.org/guide/app-profile.html): 루트 권한을 케이지에 가둡니다.

## 호환 상태

KernelSU는 공식적으로 안드로이드 GKI 2.0 디바이스(커널 5.10 이상)를 지원합니다. 오래된 커널(4.14 이상)도 사용할 수 있지만, 커널을 수동으로 빌드해야 합니다.

KernelSU는 WSA, ChromeOS, 컨테이너 기반 안드로이드 모두를 지원합니다.

현재는 `arm64-v8a`와 `x86_64`만 지원됩니다.

## 사용 방법

- [설치 방법](https://kernelsu.org/guide/installation.html)
- [어떻게 빌드하나요?](https://kernelsu.org/guide/how-to-build.html)
- [공식 웹사이트](https://kernelsu.org/)

## 번역

KernelSU 번역을 돕거나 기존 번역을 개선하려면 [Weblate](https://hosted.weblate.org/engage/kernelsu/)를 이용해 주세요. 매니저의 번역은 Weblate와 충돌할 수 있으므로 더 이상 허용되지 않습니다.

## 토론

- 텔레그램: [@KernelSU](https://t.me/KernelSU)

## 보안

KernelSU의 보안 취약점 보고에 대한 자세한 내용은 [SECURITY.md](/SECURITY.md)를 참조하세요.

## 저작권

- `kernel` 디렉터리 아래의 파일은 [GPL-2.0 전용](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)입니다.
- `kernel` 디렉토리를 제외한 다른 모든 부분은 [GPL-3.0-이상](https://www.gnu.org/licenses/gpl-3.0.html)입니다.

## 크래딧

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/): KernelSU의 아이디어.
- [Magisk](https://github.com/topjohnwu/Magisk): 강력한 루팅 도구.
- [genuine](https://github.com/brevent/genuine/): apk v2 서명 유효성 검사.
- [Diamorphine](https://github.com/m0nad/Diamorphine): 일부 rootkit 스킬.
