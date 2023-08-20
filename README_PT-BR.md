[English](README.md) | [Español](README_ES.md) | [简体中文](README_CN.md) | [繁體中文](README_TW.md) | [日本語](README_JP.md) | [Polski](README_PL.md) | **Portuguese-Brazil** | [Türkçe](README_TR.md) | [Русский](README_RU.md) | [Tiếng Việt](README_VI.md)

# KernelSU

Uma solução raiz baseada em Kernel para dispositivos Android.

## Características

1. `su` baseado em kernel e gerenciamento de acesso root.

2. Sistema modular baseado em overlayfs.

3. [App Perfil](https://kernelsu.org/guide/app-profile.html): Tranque o poder raiz em uma gaiola.

## Estado de compatibilidade

O KernelSU suporta oficialmente dispositivos Android GKI 2.0 (com kernel 5.10+), kernels antigos (4.14+) também são compatíveis, mas você mesmo precisa construir o kernel.

WSA, ChromeOS e Android baseado em contêiner também deve funcionar com o KernelSU integrado.

E os ABIs atualmente suportados são: `arm64-v8a` e `x86_64`

## Uso
- [Instalação](https://kernelsu.org/guide/installation.html)
 - [Como construir?](https://kernelsu.org/guide/how-to-build.html)
 - [Site Oficial](https://kernelsu.org/)

## Tradução
Para traduzir o KernelSU para o seu idioma, ou para melhorar uma tradução existente, use o [Weblate](https://hosted.weblate.org/engage/kernelsu/), por favor.

### Discussão

- Telegram: [@KernelSU](https://t.me/KernelSU)

## Licença

- Os arquivos no diretório `kernel` são [GPL-2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)

- Todas as outras partes, exceto o diretório `kernel`, são [GPL-3](https://www.gnu.org/licenses/gpl-3.0.html)

## Créditos

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/): a ideia do KernelSU.
- [Magisk](https://github.com/topjohnwu/Magisk): a implementação da sepolicy.
- [genuine](https://github.com/brevent/genuine/): validação de assinatura apk v2.
- [Diamorphine](https://github.com/m0nad/Diamorphine): algumas habilidades de rootkit.
