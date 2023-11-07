# FAQ

## KernelSU có hỗ trợ thiết bị của tôi không?

Đầu tiên, thiết bị của bạn sẽ có thể mở khóa bootloader. Nếu không thể thì nó không được hỗ trợ.

Sau đó, cài đặt Ứng dụng KernelSU manager vào thiết bị của bạn và mở nó, nếu nó hiển thị `Unsupported` thì thiết bị của bạn chưa được hỗ trợ ngay, nhưng bạn có thể tạo nguồn kernel và tích hợp KernelSU để nó hoạt động hoặc sử dụng [unofficially-support-devices](unofficially-support-devices).

## KernelSU có cần mở khóa Bootloader không?

Chắc chắn có.

## KernelSU có hỗ trợ các mô-đun không?

Có, nhưng đây là phiên bản đầu tiên nên có thể bị lỗi. Đợi nó ổn định nhé :)

## KernelSU có hỗ trợ Xposed không?

Có, [Dreamland](https://github.com/canyie/Dreamland) và [TaiChi](https://taichi.cool) hiện đã hoạt động. Đối với LSPosed, bạn có thể làm cho nó hoạt động bằng [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext)

## KernelSU có hỗ trợ Zygisk không?

KernelSU không có hỗ trợ Zygisk tích hợp sẵn nhưng thay vào đó, bạn có thể sử dụng [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext).

## KernelSU có tương thích với Magisk không?

Hệ thống mô-đun của KernelSU xung đột với magic mount của Magisk, nếu có bất kỳ mô-đun nào được kích hoạt trong KernelSU thì toàn bộ Magisk sẽ không hoạt động.

Nhưng nếu bạn chỉ sử dụng `su` của KernelSU thì nó sẽ hoạt động tốt với Magisk: KernelSU sửa đổi `kernel` và Magisk sửa đổi `ramdisk`, chúng có thể hoạt động cùng nhau.

## KernelSU sẽ thay thế Magisk?

Chúng tôi không nghĩ như vậy và đó không phải là mục tiêu của chúng tôi. Magisk đủ tốt cho giải pháp root userspace và nó sẽ tồn tại lâu dài. Mục tiêu của KernelSU là cung cấp giao diện kernel cho người dùng chứ không thay thế Magisk.

## KernelSU có thể hỗ trợ các thiết bị không phải GKI không?

Điều đó là có thể. Nhưng bạn nên tải nguồn kernel về và tích hợp KernelSU vào source tree rồi tự biên dịch kernel.

## KernelSU có thể hỗ trợ các thiết bị dưới Android 12 không?

Chính kernel của thiết bị ảnh hưởng đến khả năng tương thích của KernelSU và nó không liên quan gì đến phiên bản Android. Hạn chế duy nhất là các thiết bị chạy Android 12 phải là kernel 5.10+(thiết bị GKI). Vì thế:

1. Các thiết bị chạy Android 12 phải được hỗ trợ.
2. Các thiết bị có kernel cũ (Một số thiết bị Android 12 cũng là kernel cũ) tương thích (Bạn nên tự build kernel)

## KernelSU có thể hỗ trợ kernel cũ không?

Có thể, KernelSU hiện đã được backport sang kernel 4.14, đối với kernel cũ hơn, bạn cần backport nó một cách cẩn thận và PR rất đáng hoan nghênh!

## Làm cách nào để tích hợp KernelSU cho kernel cũ?

Vui lòng tham khảo [hướng dẫn](how-to-integrate-for-non-gki)

## Tại sao phiên bản Android của tôi là 13 và kernel hiển thị "android12-5.10"?

Phiên bản Kernel không liên quan gì đến phiên bản Android, nếu bạn cần flash kernel thì dùng luôn phiên bản kernel, phiên bản Android không quá quan trọng.

## Đã có mount namespace --mount-master/global trên KernelSU chưa?

Hiện tại thì không (có thể có trong tương lai), nhưng có nhiều cách để chuyển sang global mount namespace một cách thủ công, chẳng hạn như:

1. `nsenter -t 1 -m sh` để lấy shell trong global mount namespace.
2. Thêm `nsenter --mount=/proc/1/ns/mnt` vào lệnh bạn muốn thực thi, sau đó lệnh được thực thi trong global mount namespace. KernelSU cũng [sử dụng cách này](https://github.com/tiann/KernelSU/blob/77056a710073d7a5f7ee38f9e77c9fd0b3256576/manager/app/src/main/java/me/weishu/kernelsu/ui/util/KsuCli.kt#L115)

## Tôi là GKI1.0, tôi có thể sử dụng cái này không?

GKI1 khác hoàn toàn với GKI2, bạn phải tự biên dịch kernel.
