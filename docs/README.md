
**English** | [Español](README_ES.md) | [简体中文](README_CN.md) | [繁體中文](README_TW.md) | [日本語](README_JP.md) | [Polski](README_PL.md) | [Portuguese-Brazil](README_PT-BR.md) | [Türkçe](README_TR.md) | [Русский](README_RU.md) | [Tiếng Việt](README_VI.md) | [Indonesia](README_ID.md) | [עברית](README_iw.md) | [हिंदी](README_IN.md)

# KernelSU

A Kernel-based root solution for Android devices.

## Features

1. Kernel-based `su` and root access management.
2. Module system based on overlayfs.
3. [App Profile](https://kernelsu.org/guide/app-profile.html): Lock up the root power in a cage.

## Compatibility State

KernelSU officially supports Android GKI 2.0 devices (kernel 5.10+). Older kernels (4.14+) are also compatible, but the kernel will have to be built manually.

With this, WSA, ChromeOS, and container-based Android are all supported.

Currently, only `arm64-v8a` and `x86_64` are supported.

## Usage

- [Installation Instruction](https://kernelsu.org/guide/installation.html)
- [How to build?](https://kernelsu.org/guide/how-to-build.html)
- [Official Website](https://kernelsu.org/)

## Translation

To help translate KernelSU or improve existing translations, please use [Weblate](https://hosted.weblate.org/engage/kernelsu/).

## Discussion

- Telegram: [@KernelSU](https://t.me/KernelSU)

## License

- Files under the `kernel` directory are [GPL-2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
- All other parts except the `kernel` directory are [GPL-3](https://www.gnu.org/licenses/gpl-3.0.html)

## Credits

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/): the KernelSU idea.
- [Magisk](https://github.com/topjohnwu/Magisk): the powerful root tool.
- [genuine](https://github.com/brevent/genuine/): apk v2 signature validation.
- [Diamorphine](https://github.com/m0nad/Diamorphine): some rootkit skills.
