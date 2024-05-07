[English](README.md) | [Español](README_ES.md) | [简体中文](README_CN.md) | [繁體中文](README_TW.md) | [日本語](README_JP.md) | [한국어](README_KR.md) | [Polski](README_PL.md) | [Português (Brasil)](README_PT-BR.md) | [Türkçe](README_TR.md) | [Русский](README_RU.md) | **Tiếng Việt** | [Indonesia](README_ID.md) | [עברית](README_IW.md) | [हिंदी](README_IN.md)

# KernelSU

<img src="https://kernelsu.org/logo.png" style="width: 96px;" alt="logo">

Giải pháp root thông qua thay đổi trên Kernel hệ điều hành cho các thiết bị Android.

[![Latest release](https://img.shields.io/github/v/release/tiann/KernelSU?label=Release&logo=github)](https://github.com/tiann/KernelSU/releases/latest)
[![Weblate](https://img.shields.io/badge/Localization-Weblate-teal?logo=weblate)](https://hosted.weblate.org/engage/kernelsu)
[![Channel](https://img.shields.io/badge/Follow-Telegram-blue.svg?logo=telegram)](https://t.me/KernelSU)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-orange.svg?logo=gnu)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
[![GitHub License](https://img.shields.io/github/license/tiann/KernelSU?logo=gnu)](/LICENSE)

## Tính năng

1. Hỗ trợ gói thực thi `su` và quản lý quyền root.
2. Hệ thống mô-đun thông qua [OverlayFS](https://en.wikipedia.org/wiki/OverlayFS).
3. [App Profile](https://kernelsu.org/guide/app-profile.html): Hạn chế quyền root của ứng dụng.

## Tình trạng tương thích

KernelSU chính thức hỗ trợ các thiết bị Android với kernel GKI 2.0 (phiên bản kernel 5.10+), các phiên bản kernel cũ hơn (4.14+) cũng tương thích, nhưng bạn cần phải tự biên dịch.

WSA, ChromeOS và Android dựa trên container(container-based) cũng được hỗ trợ bởi KernelSU.

Hiên tại Giao diện nhị phân của ứng dụng (ABI) được hỗ trợ bao gồm `arm64-v8a` và `x86_64`.

## Sử dụng

- [Hướng dẫn cài đặt](https://kernelsu.org/vi_VN/guide/installation.html)
- [Cách để build?](https://kernelsu.org/vi_VN/guide/how-to-build.html)
- [Website Chính Thức](https://kernelsu.org/vi_VN/)

## Hỗ trợ dịch

Nếu bạn muốn hỗ trợ dịch KernelSU sang một ngôn ngữ khác hoặc cải thiện các bản dịch trước, vui lòng sử dụng [Weblate](https://hosted.weblate.org/engage/kernelsu/).

## Thảo luận

- Telegram: [@KernelSU](https://t.me/KernelSU)

## Giấy phép

- Tất cả các file trong thư mục `kernel` dùng giấy phép [GPL-2-only](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html).
- Tất cả các thành phần khác ngoại trừ thư mục `kernel` dùng giấy phép [GPL-3-or-later](https://www.gnu.org/licenses/gpl-3.0.html).

## Lời cảm ơn

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/): ý tưởng cho KernelSU.
- [Magisk](https://github.com/topjohnwu/Magisk): công cụ root mạnh mẽ.
- [genuine](https://github.com/brevent/genuine/): phương pháp xác thực apk v2.
- [Diamorphine](https://github.com/m0nad/Diamorphine): các phương pháp ẩn của rootkit.
