[English](README.md) | [Español](README_ES.md) | [简体中文](README_CN.md) | [繁體中文](README_TW.md) | [日本語](README_JP.md) | [Polski](README_PL.md) | [Portugis-Brasil](README_PT-BR.md) | [Türkçe](README_TR.md) | [Русский](README_RU.md) | [Tiếng Việt](README_VI.md) | **Indonesia** | [עברית](README_iw.md) | [हिंदी](README_IN.md)

# KernelSU

Solusi root berbasis Kernel untuk perangkat Android.

## Fitur

1. Manajemen akses root dan `su` berbasis kernel.
2. Sistem modul berdasarkan overlayfs.
3. [Profil Aplikasi](https://kernelsu.org/guide/app-profile.html): Kunci daya root di dalam sangkar.

## Status Kompatibilitas

KernelSU secara resmi mendukung perangkat Android GKI 2.0 (dengan kernel 5.10+), kernel lama (4.14+) juga kompatibel, tetapi Anda perlu membuat kernel sendiri.

WSA, ChromeOS, dan Android berbasis wadah juga dapat bekerja dengan KernelSU terintegrasi.

Dan ABI yang didukung saat ini adalah: `arm64-v8a` dan `x86_64`

## Penggunaan

- [Petunjuk Instalasi](https://kernelsu.org/guide/installation.html)
- [Bagaimana cara membuat?](https://kernelsu.org/guide/how-to-build.html)
- [Situs Web Resmi](https://kernelsu.org/)

## Terjemahan

Untuk menerjemahkan KernelSU ke dalam bahasa Anda atau menyempurnakan terjemahan yang sudah ada, harap gunakan [Weblat](https://hosted.weblate.org/engage/kernelsu/).

## Diskusi

- Telegram: [@KernelSU](https://t.me/KernelSU)

## Lisensi

- File di bawah direktori `kernel` adalah [GPL-2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
- Semua bagian lain kecuali direktori `kernel` adalah [GPL-3](https://www.gnu.org/licenses/gpl-3.0.html)

## Kredit

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/): ide KernelSU.
- [Magisk](https://github.com/topjohnwu/Magisk): alat root yang ampuh.
- [genuine](https://github.com/brevent/genuine/): validasi tanda tangan apk v2.
- [Diamorphine](https://github.com/m0nad/Diamorphine): beberapa keterampilan rootkit.
