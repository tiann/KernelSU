[English](README.md) | [Español](README_ES.md) | [简体中文](README_CN.md) | [繁體中文](README_TW.md) | [日本語](README_JP.md) | [한국어](README_KR.md) | [Polski](README_PL.md) | [Português (Brasil)](README_PT-BR.md) | [Türkçe](README_TR.md) | [Русский](README_RU.md) | [Tiếng Việt](README_VI.md) | **Indonesia** | [עברית](README_IW.md) | [हिंदी](README_IN.md) | [Italiano](README_IT.md)

# KernelSU

<img src="https://kernelsu.org/logo.png" style="width: 96px;" alt="logo">

Solusi root berbasis kernel untuk perangkat Android.

[![Latest release](https://img.shields.io/github/v/release/tiann/KernelSU?label=Release&logo=github)](https://github.com/tiann/KernelSU/releases/latest)
[![Weblate](https://img.shields.io/badge/Localization-Weblate-teal?logo=weblate)](https://hosted.weblate.org/engage/kernelsu)
[![Channel](https://img.shields.io/badge/Follow-Telegram-blue.svg?logo=telegram)](https://t.me/KernelSU)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-orange.svg?logo=gnu)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
[![GitHub License](https://img.shields.io/github/license/tiann/KernelSU?logo=gnu)](/LICENSE)

## Fitur

1. Akses `su` dan manajemen hak akses root berbasis kernel.
2. Sistem modul berbasis [metamodules](https://kernelsu.org/id_ID/guide/metamodule.html): Infrastruktur yang dapat dicopot-pasang untuk modifikasi systemless.
3. [Profil Aplikasi](https://kernelsu.org/guide/app-profile.html): Kendalikan hak akses root secara aman.

## Status Kompatibilitas

KernelSU secara resmi mendukung perangkat Android GKI 2.0 (dengan kernel 5.10+), kernel lama (4.14+) juga didukung, tetapi kernel perlu dikompilasi secara manual.

Dengan dukungan ini WSA, ChromeOS, dan Android berbasis kontainer semuanya dapat dijalankan.

Saat ini, arsitektur `arm64-v8a` dan `x86_64` sudah didukung.

> [!CAUTION]
> Versi kernel terbaru menerapkan perubahan besar yang menyebabkan KernelSU gagal dan berpotensi memicu kernel panic pada `x86_64`! Cek situs web untuk informasi lebih lanjut!

## Penggunaan

- [Pemasangan](https://kernelsu.org/id_ID/guide/installation.html)
- [Cara Build](https://kernelsu.org/id_ID/guide/how-to-build.html)
- [Situs Web Resmi](https://kernelsu.org/id_ID/)

## Terjemahan

Untuk membantu penerjemahan KernelSU, kami tidak lagi menerima kontribusi terjemahan melalui Weblate. Semua terjemahan kini ditangani menggunakan LLM.

Jika Anda ingin menambahkan dukungan untuk bahasa baru, silakan buat PR (Pull Request). Harap diperhatikan bahwa perubahan pada terjemahan bahasa Inggris dan Mandarin yang sudah ada tidak akan diterima.

## Diskusi

- Telegram: [@KernelSU](https://t.me/KernelSU)

## Keamanan

Untuk informasi mengenai pelaporan kerentanan keamanan di KernelSU, lihat [SECURITY.md](/SECURITY.md).

## Lisensi

- Berkas di bawah direktori `kernel` berlisensi [GPL-2-only](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html).
- Seluruh bagian lainnya kecuali direktori `kernel` berlisensi [GPL-3.0-or-later](https://www.gnu.org/licenses/gpl-3.0.html).

## Kredit

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/): Gagasan utama KernelSU.
- [Magisk](https://github.com/topjohnwu/Magisk): Alat root yang andal.
- [genuine](https://github.com/brevent/genuine/): Validasi tanda tangan APK v2.
- [Diamorphine](https://github.com/m0nad/Diamorphine): Beberapa keahlian rootkit.
