# FAQ

## KernelSU có hỗ trợ thiết bị của tôi không?

KernelSU hỗ trợ các thiết bị chạy Android với bootloader đã mở khóa. Tuy nhiên, hỗ trợ chính thức chỉ dành cho GKI Linux Kernel 5.10+ (trong thực tế, điều này có nghĩa là thiết bị của bạn cần có Android 12 out-of-the-box để được hỗ trợ).

Bạn có thể dễ dàng kiểm tra hỗ trợ cho thiết bị của mình thông qua ứng dụng quản lý KernelSU, có sẵn [tại đây](https://github.com/tiann/KernelSU/releases).

Nếu ứng dụng hiển thị `Not installed`, điều đó có nghĩa là thiết bị của bạn được KernelSU hỗ trợ chính thức.

Nếu ứng dụng hiển thị `Unsupported`, điều đó có nghĩa là thiết bị của bạn hiện không được hỗ trợ chính thức. Tuy nhiên, bạn có thể build mã nguồn kernel và tích hợp KernelSU để làm cho nó hoạt động, hoặc sử dụng [Thiết bị được hỗ trợ không chính thức](unofficially-support-devices).

## KernelSU có cần mở khóa bootloader không?

Chắc chắn rồi.

## KernelSU có hỗ trợ module không?

Có, hầu hết các module Magisk hoạt động trên KernelSU. Tuy nhiên, nếu module của bạn cần sửa đổi các tệp `/system`, bạn cần cài đặt [metamodule](metamodule.md) (chẳng hạn như `meta-overlayfs`). Các tính năng module khác hoạt động mà không cần metamodule. Kiểm tra [Hướng dẫn Module](module.md) để biết thêm thông tin.

## KernelSU có hỗ trợ Xposed không?

Có, bạn có thể sử dụng LSPosed (hoặc các phái sinh Xposed hiện đại khác) với [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext).

## KernelSU có hỗ trợ Zygisk không?

KernelSU không có hỗ trợ Zygisk tích hợp sẵn, nhưng bạn có thể sử dụng module như [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext) để hỗ trợ nó.

## KernelSU có tương thích với Magisk không?

Hệ thống module của KernelSU xung đột với magic mount của Magisk. Nếu có bất kỳ module nào được kích hoạt trong KernelSU, toàn bộ Magisk sẽ ngừng hoạt động.

Tuy nhiên, nếu bạn chỉ sử dụng `su` của KernelSU, nó sẽ hoạt động tốt với Magisk. KernelSU sửa đổi `kernel`, trong khi Magisk sửa đổi `ramdisk`, cho phép cả hai hoạt động cùng nhau.

## KernelSU sẽ thay thế Magisk?

Chúng tôi tin rằng không, và đó không phải là mục tiêu của chúng tôi. Magisk đã đủ tốt cho giải pháp root userspace và sẽ tồn tại lâu dài. Mục tiêu của KernelSU là cung cấp giao diện kernel cho người dùng, không phải để thay thế Magisk.

## KernelSU có thể hỗ trợ các thiết bị không phải GKI không?

Có thể. Nhưng bạn cần tải xuống mã nguồn kernel và tích hợp KernelSU vào source tree, sau đó tự biên dịch kernel.

## KernelSU có thể hỗ trợ các thiết bị dưới Android 12 không?

Chính kernel thiết bị ảnh hưởng đến khả năng tương thích của KernelSU, và nó không liên quan gì đến phiên bản Android. Hạn chế duy nhất là các thiết bị được ra mắt với Android 12 phải có phiên bản kernel 5.10+ (thiết bị GKI). Vì vậy:

1. Các thiết bị được ra mắt với Android 12 phải được hỗ trợ.
2. Các thiết bị có kernel cũ (một số thiết bị với Android 12 cũng có kernel cũ) tương thích (bạn cần tự build kernel).

## KernelSU có thể hỗ trợ kernel cũ không?

Có thể. KernelSU hiện đã được backport cho kernel 4.14. Đối với các kernel cũ hơn, bạn cần tự backport, và PR luôn được chào đón!

## Làm cách nào để tích hợp KernelSU cho kernel cũ?

Vui lòng kiểm tra hướng dẫn [Tích hợp cho thiết bị không phải GKI](how-to-integrate-for-non-gki).

## Tại sao phiên bản Android của tôi là 13, nhưng kernel hiển thị "android12-5.10"?

Phiên bản kernel không liên quan gì đến phiên bản Android. Nếu bạn cần flash kernel, luôn sử dụng phiên bản kernel; phiên bản Android không quan trọng bằng.

## Tôi là GKI 1.0, tôi có thể sử dụng điều này không?

GKI 1.0 hoàn toàn khác với GKI 2.0, bạn phải tự biên dịch kernel.

## Làm cách nào để làm cho `/system` RW?

Chúng tôi không khuyến nghị bạn sửa đổi trực tiếp phân vùng hệ thống. Vui lòng kiểm tra [Hướng dẫn Module](module.md) để sửa đổi nó một cách systemless. Nếu bạn khăng khăng làm điều này, hãy kiểm tra [magisk_overlayfs](https://github.com/HuskyDG/magic_overlayfs).

## KernelSU có thể sửa đổi hosts không? Làm cách nào để sử dụng AdAway?

Tất nhiên. Nhưng KernelSU không có hỗ trợ hosts tích hợp sẵn, bạn có thể cài đặt module như [systemless-hosts](https://github.com/symbuzzer/systemless-hosts-KernelSU-module) để thực hiện.

## Tại sao các module của tôi không hoạt động sau khi cài đặt mới?

Nếu các module của bạn cần sửa đổi các tệp `/system`, bạn cần cài đặt [metamodule](metamodule.md) để mount thư mục `system`. Các tính năng module khác (scripts, sepolicy, system.prop) hoạt động mà không cần metamodule.

**Giải pháp**: Xem [Hướng dẫn Metamodule](metamodule.md) để biết hướng dẫn cài đặt.

## Metamodule là gì và tại sao tôi cần nó?

Metamodule là một module đặc biệt cung cấp cơ sở hạ tầng để mount các module thông thường. Xem [Hướng dẫn Metamodule](metamodule.md) để biết giải thích đầy đủ.
