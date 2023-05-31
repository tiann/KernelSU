[English](README.md) | [简体中文](README_CN.md) | [繁體中文](README_TW.md) | [日本語](README_JP.md) | **Portuguese-Brazil**

# KernelSU

Uma solução raiz baseada em Kernel para dispositivos Android.

## Características

1. `su` baseado em kernel e gerenciamento de acesso root.

2. Sistema modular baseado em overlayfs.

## Estado de compatibilidade

O KernelSU suporta oficialmente dispositivos Android GKI 2.0 (com kernel 5.10+), kernels antigos (4.14+) também são compatíveis, mas você mesmo precisa construir o kernel.

O Android baseado em WSA e contêiner também deve funcionar com o KernelSU integrado.

E os ABIs atualmente suportados são: `arm64-v8a` e `x86_64`

## Uso

[Instalação](https://kernelsu.org/guide/installation.html)

## Construir

[Como construir?](https://kernelsu.org/guide/how-to-build.html)

### Discussão

- Telegram: [@KernelSU](https://t.me/KernelSU)

## Licença

- Os arquivos no diretório `kernel` são [GPL-2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)

- Todas as outras partes, exceto o diretório `kernel`, são [GPL-3](https://www.gnu.org/licenses/gpl-3.0.html)

## Créditos

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/): a ideia do KernelSU.

- [genuine](https://github.com/brevent/genuine/): validação de assinatura apk v2.

- [Diamorphine](https://github.com/m0nad/Diamorphine): algumas habilidades de rootkit.

- [Magisk](https://github.com/topjohnwu/Magisk): a implementação da sepolicy.
