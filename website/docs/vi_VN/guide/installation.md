# Cách cài đặt

## Kiểm tra xem thiết bị có hỗ trợ không

Tải KernelSU Manager từ [trang Releases trên Github](https://github.com/tiann/KernelSU/releases) hoặc [Github Actions](https://github.com/tiann/KernelSU/actions/workflows/build-manager.yml)

- Nếu ứng dụng hiện `Unsupported`, nghĩa là **Bạn phải tự compile kernel**, KernelSU sẽ không và không bao giờ cung cấp cho bạn một boot image dành riêng cho bạn để flash.
- Nếu ứng dụng hiện `Not installed`, thì thiết bị của bạn đã chính thức hỗ trợ bởi KernelSU.

## Tìm một boot.img

KernelSU cung cấp một boot.img chung cho các thiết bị GKI, bạn nên flash boot.img này vào trong phân vùng boot của bạn.

Bạn có thể tải boot.img từ [Github Actions cho Kernel](https://github.com/tiann/KernelSU/actions/workflows/build-kernel.yml), lưu ý là hãy dùng đúng phiên bản boot.img. Ví dụ, nếu phiên bản kernel bạn dùng là `5.10.101`, thì bạn nên sử dụng `5.10.101-xxxx.boot.xxxx`.

Và tiện thể hãy kiểm tra định dạng gốc của boot.img trong máy bạn, bạn nên sử dụng đúng định dạng như là `lz4` hoặc `gz`.

## Flash boot.img này vào thiết bị

Kết nối thiết bị với `adb` và chạy `adb reboot bootloader` để vào chế độ fastboot, và rồi dùng câu lệnh này để flash KernelSU:

```sh
fastboot flash boot boot.img
```

## Khởi động lại

Khi flash xong, hãy khởi động lại thiết bị :

```sh
fastboot reboot
```
