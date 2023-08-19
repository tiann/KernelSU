**English** | [Español](README_ES.md) | [简体中文](README_CN.md) | [繁體中文](README_TW.md) | [日本語](README_JP.md) | [Polski](README_PL.md) | [Portuguese-Brazil](README_PT-BR.md) | [Türkçe](README_TR.md) | [Русский](README_RU.md) | [Tiếng Việt](README_VI.md)

# KernelSU

Giải pháp root thông qua thay đổi trên nhân hệ điều hành cho các thiết bị Android.

## Tính năng

1. Hỗ trợ gói thực thi `su` và quản lý quyền root.
2. Hệ thống mô-đun thông qua overlayfs.
3. [Hồ sơ ứng dụng](https://kernelsu.org/guide/app-profile.html): Hạn chế quyền root của ứng dụng.

## Tình trạng tương thích

KernelSU chính thức hỗ trợ các thiết bị Android với nhân GKI 2.0(phiên bản nhân 5.10+), các phiên bản nhân cũ hơn(4.14+) cũng tương thích, nhưng bạn cần phải tự biên dịch.

WSA, ChromeOS và Android dựa trên container(container-based) cũng được hỗ trợ bởi KernelSU.

Hiên tại Giao diện nhị phân của ứng dụng (ABI) được hỗ trợ bao gồm `arm64-v8a` và `x86_64`

## Sử dụng

- [Hướng dẫn cài đặt](https://kernelsu.org/vi_VN/guide/installation.html)
- [Cách để build?](https://kernelsu.org/vi_VN/guide/how-to-build.html)
- [Website chính thức](https://kernelsu.org/)

## Hỗ trợ dịch

Nếu bạn muốn hỗ trợ dịch KernelSU sang một ngôn ngữ khác hoặc cải thiện bản dịch trước, vui lòng sử dụng [Weblate](https://hosted.weblate.org/engage/kernelsu/).

### Thảo luận

- Telegram: [@KernelSU](https://t.me/KernelSU)

## Giấy phép

- Tất cả các file trong thư mục `kernel` dùng giấy phép [GPL-2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
- Tất cả các thành phần khác ngoại trừ thư mục `kernel` dùng giấy phép [GPL-3](https://www.gnu.org/licenses/gpl-3.0.html)

## Lời cảm ơn

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/): vì ý tưởng cho KernelSU.
- [Magisk](https://github.com/topjohnwu/Magisk): một công cụ root mạnh mẽ.
- [genuine](https://github.com/brevent/genuine/): vì phương pháp xác thực apk v2.
- [Diamorphine](https://github.com/m0nad/Diamorphine): vì các phương pháp che giấu của rootkit .
