# FAQ - Câu hỏi thường gặp

## KernelSU có hỗ trợ thiết bị của tôi không?

Trước tiên, bạn nên mở khóa bootloader . Nếu bạn không thể thì nó sẽ không được hỗ trợ.

Nếu có thể thì cài đặt KernelSU Manager vào thiết bị của bạn và mở nó, nếu nó hiển thị `Unsupported` thì thiết bị của bạn không được hỗ trợ và sẽ có khả năng không được hỗ trợ trong tương lai.

## KernelSU có cần mở khóa Bootloader không?

Chắc chắn là có.

## KernelSU có hỗ trợ các modules không?

Có, nhưng ở những phiên bản thử nghiệm này có thể có rất nhiều lỗi. Vậy nên tốt hơn hết là đợi nó ổn định đã :)

## KernelSU có hỗ trợ Xposed không?

Có, [Dreamland](https://github.com/canyie/Dreamland) và [TaiChi](https://taichi.cool) hiện đã hoạt động được một phần nào đó. Với Lsposed, bạn có thể thử [Zygisk trên KernelSU](https://github.com/Dr-TSNG/ZygiskOnKernelSU) 

## KernelSU có tương thích với Magisk không?

Hệ thống module của KernelSU xung đột với magic mount của Magisk, nếu có bất kỳ module nào được bật trong KernelSU thì toàn bộ Magisk sẽ không thể hoạt động.

Nhưng nếu bạn chỉ sử dụng `su` của KernelSU thì nó sẽ hoạt động tốt với Magisk: KernelSU sửa đổi `kernel` và Magisk sửa đổi `ramdisk`, chúng có thể hoạt động cùng nhau.

## KernelSU sẽ thay thế Magisk?

Chúng tôi không nghĩ như vậy và đó không phải là mục tiêu của chúng tôi. Magisk đã đủ tốt cho giải pháp userspace root và nó sẽ tồn tại lâu dài. Mục tiêu của KernelSU là cung cấp giao diện kernel cho người dùng chứ không phải để thay thế Magisk.

## KernelSU có thể hỗ trợ các thiết bị không sử dụng GKI không?

Điều đó là có thể. Nhưng bạn nên tải xuống mã nguồn kernel và tích hợp KernelSU vào source rồi tự compile.

## KernelSU có thể hỗ trợ các thiết bị chạy Android 12 trở xuống không?

Kernel của thiết bị ảnh hưởng đến khả năng tương thích của KernelSU và nó sẽ không liên quan gì đến phiên bản Android. Hạn chế duy nhất là các thiết bị chạy Android 12 phải là nhân 5.10 trở lên (thiết bị dùng GKI). Vì thế:

1. Các thiết bị chạy Android 12 phải được hỗ trợ.
2. Các thiết bị có kernel cũ (Một số thiết bị Android 12 cũng là kernel cũ) có thể tương thích (Bạn nên tự xây dựng kernel)

## KernelSU có thể hỗ trợ kernel cũ không?

Có thể, KernelSU hiện đã được backport xuống kernel 4.14, đối với kernel cũ hơn, bạn cần backport một cách thủ công và các Pull-Requests luôn được hoan nghênh!

## Làm cách nào để tích hợp KernelSU cho kernel cũ?

Vui lòng tham khảo [hướng dẫn này](how-to-integrate-for-non-gki)

## Tại sao tôi đang chạy Android 13 nhưng kernel lại ghi "android12-5.10" ?

Phiên bản kernel hoàn toàn không liên quan gì đến phiên bản Android, nếu bạn muốn flash kernel thì hãy luôn để ý đến **phiên bản kernel**, phiên bản Android ở phần đầu (VD : android12-\*) thường không quan trọng lắm.

## Đã có mount namespace --mount-master/global trên KernelSU chưa ?

Hiện tại chưa có (hoặc có thể sẽ có trong tương lại), nhưng bạn có thể dùng `nsenter -t 1 -m sh` để vào global mount namespace.

## KernelSU có hỗ trợ Zygisk không ?

KernelSU không có Zygisk bên trong, nhưng bạn có thể dùng [Zygisk trên KernelSU](https://github.com/Dr-TSNG/ZygiskOnKernelSU)

## Tôi đang ở GKI 1.0, tôi dùng được cái này chứ ?

GKI1 khác hoàn toàn với GKI2 nên bạn sẽ phải tự compile kernel cho mình.
