[English](README.md) | [Español](README_ES.md) | [简体中文](README_CN.md) | [繁體中文](README_TW.md) | [日本語](README_JP.md) | [한국어](README_KR.md) | [Polski](README_PL.md) | **Português (Brasil)** | [Türkçe](README_TR.md) | [Русский](README_RU.md) | [Tiếng Việt](README_VI.md) | [Indonesia](README_ID.md) | [עברית](README_IW.md) | [हिंदी](README_IN.md) | [Italiano](README_IT.md)

# KernelSU

<img src="https://kernelsu.org/logo.png" style="width: 96px;" alt="logo">

Uma solução root baseada em kernel para dispositivos Android.

[![Latest release](https://img.shields.io/github/v/release/tiann/KernelSU?label=Release&logo=github)](https://github.com/tiann/KernelSU/releases/latest)
[![Weblate](https://img.shields.io/badge/Localização-Weblate-teal?logo=weblate)](https://hosted.weblate.org/engage/kernelsu)
[![Channel](https://img.shields.io/badge/Seguir-Telegram-blue.svg?logo=telegram)](https://t.me/KernelSU)
[![License: GPL v2](https://img.shields.io/badge/Licença-GPL%20v2-orange.svg?logo=gnu)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
[![GitHub License](https://img.shields.io/github/license/tiann/KernelSU?logo=gnu)](/LICENSE)

## Características

1. `su` e gerenciamento de acesso root baseado em kernel.
2. Sistema modular baseado em [OverlayFS](https://en.wikipedia.org/wiki/OverlayFS).
3. [Perfil do Aplicativo](https://kernelsu.org/pt_BR/guide/app-profile.html): Tranque o poder root em uma gaiola.

## Estado de compatibilidade

O KernelSU oferece suporte oficial a dispositivos Android GKI 2.0 (kernel 5.10+). Kernels mais antigos (4.14+) também são compatíveis, mas o kernel terá que ser construído manualmente.

Com isso, WSA, ChromeOS e Android baseado em contêiner são todos suportados.

Atualmente, apenas `arm64-v8a` e `x86_64` são suportados.

## Uso

 - [Instalação](https://kernelsu.org/pt_BR/guide/installation.html)
 - [Como compilar o KernelSU?](https://kernelsu.org/pt_BR/guide/how-to-build.html)
 - [Site oficial](https://kernelsu.org/pt_BR/)

## Tradução

Para contribuir com a tradução do KernelSU ou aprimorar traduções existentes, por favor, utilize o [Weblate](https://hosted.weblate.org/engage/kernelsu/). PR para a tradução do Gerenciador não são mais aceitas, pois podem entrar em conflito com o Weblate.

## Discussão

- Telegram: [@KernelSU](https://t.me/KernelSU)

## Segurança

Para obter informações sobre como relatar vulnerabilidades de segurança do KernelSU, consulte [SECURITY.md](/SECURITY.md).

## Licença

- Os arquivos no diretório `kernel` são [GPL-2.0-only](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html).
- Todas as outras partes, exceto o diretório `kernel` são [GPL-3.0-or-later](https://www.gnu.org/licenses/gpl-3.0.html).

## Créditos

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/): a ideia do KernelSU.
- [Magisk](https://github.com/topjohnwu/Magisk): a poderosa ferramenta root.
- [genuine](https://github.com/brevent/genuine/): validação de assinatura apk v2.
- [Diamorphine](https://github.com/m0nad/Diamorphine): algumas habilidades de rootkit.
