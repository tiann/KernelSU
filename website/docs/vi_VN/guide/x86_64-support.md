# Hỗ trợ x86_64

KernelSU hỗ trợ đầy đủ kiến trúc `x86_64`. Tuy nhiên, do các thay đổi bảo mật gần đây từ upstream kernel, việc tích hợp KernelSU trên các kernel `x86_64` hiện đại cần thêm xử lý để unified syscall dispatcher của chúng ta hoạt động chính xác.

## Vì sao nó bị hỏng?

Trong các phiên bản kernel mới hơn, một [commit](https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/commit/?id=1e3ad78334a69b36e107232e337f9d693dcc9df2) đã được thêm vào để tăng cường bảo vệ syscall table. Thay đổi này chuyển các nhánh gián tiếp trong đường đi của system call thành một loạt nhánh điều kiện trực tiếp.

Cơ chế `syscall_hook` của KernelSU dựa vào việc sửa đổi các mục trong syscall table để các system call bị chặn có thể được chuyển đến unified dispatcher. Vì cơ chế hardening mới thay đổi đường đi của system call, kernel sẽ bỏ qua các thay đổi đó đối với syscall table. Nếu KernelSU cố tải và hook syscall table mà không xử lý giới hạn này đúng cách, nó sẽ không thể định tuyến lời gọi và sẽ chủ động hủy khởi tạo để tránh kernel panic.

## Cách khắc phục?

Hiện có hai cách được hỗ trợ để xử lý vấn đề syscall hook trên `x86_64`:

1. Bật tùy chọn biên dịch kernel `KSU_X86_PATCH_SYSCALL_DISPATCHER`.
2. Tiếp tục dùng phương pháp vá mã nguồn kernel như trước.

Bạn chỉ cần dùng một trong hai. Không áp dụng cả hai cùng lúc.

### Cách 1: Bật `KSU_X86_PATCH_SYSCALL_DISPATCHER`

KernelSU 3.2.6 đã giới thiệu cơ chế chính thức mới cho `x86_64`: tùy chọn build `KSU_X86_PATCH_SYSCALL_DISPATCHER`.

Khi tùy chọn này được bật, KernelSU sẽ tự động vá động hardened syscall dispatcher trong lúc chạy để syscall hook hoạt động mà không cần bộ vá mã nguồn kernel cũ. Đây là cách được khuyến nghị nếu bạn đang build kernel với KernelSU 3.2.6 hoặc mới hơn.

### Cách 2: Áp dụng bộ vá mã nguồn kernel cũ

Nếu bạn không muốn bật `KSU_X86_PATCH_SYSCALL_DISPATCHER`, bạn vẫn có thể tiếp tục dùng cách vá kernel như trước.

Để KernelSU hoạt động trên các kernel mới này, hãy áp dụng bản vá cho phép bỏ qua cơ chế syscall hardening cụ thể này.

::: danger CẢNH BÁO BẢO MẬT
Khi sử dụng một trong hai giải pháp này, bạn đang chủ động bỏ qua hoặc làm suy yếu một cơ chế giảm thiểu được thiết kế để chống lại các lỗ hổng speculative execution.

Điều này sẽ mở lại bề mặt tấn công nhánh gián tiếp cho system call. **Không sử dụng một trong hai giải pháp này nếu bạn đang chạy máy chủ production hoặc hệ thống yêu cầu bảo mật side-channel nghiêm ngặt.** Các giải pháp này chỉ dành cho môi trường thử nghiệm, nơi quyền root thông qua KernelSU được ưu tiên hơn cơ chế giảm thiểu lỗ hổng phần cứng cụ thể này.
:::

Hãy chọn và áp dụng các bản vá phù hợp với phiên bản kernel của bạn dưới đây. Các bản vá này tạo ra một tính năng có tên `X86_FEATURE_INDIRECT_SAFE` và có thể được kích hoạt bằng tham số dòng lệnh kernel `syscall_hardening=off`.

```
For kernel 6.6:
https://github.com/android-generic/kernel_common/commit/fe9a9b4c320577c30e1f22d04039e414c6a3cdec
https://github.com/android-generic/kernel_common/commit/df772e99e392f24b395ceaf7b26974e3e4828ee9

For kernel 6.12:
https://github.com/android-generic/kernel-zenith/commit/dd2c602268fdc81f4d3b662f6a15142ac0ec7bcd
https://github.com/android-generic/kernel-zenith/commit/7d99237ae5da61c19447138da3282ae37d43857b

For kernel 6.18:
https://github.com/android-generic/kernel-zenith/commit/40b1c323d1ad29c86e041d665c7f089b9a3ccfb5
https://github.com/android-generic/kernel-zenith/commit/f5813e10b7630e1ccd86fc2c4cf30eef60b64a82
```

## Nên chọn cách nào?

- Nếu bạn đang dùng KernelSU 3.2.6 trở lên và có thể thay đổi cấu hình build của KernelSU, hãy bật `KSU_X86_PATCH_SYSCALL_DISPATCHER`.
- Nếu bạn muốn giữ quy trình vá kernel hiện tại, hãy tiếp tục dùng bộ vá mã nguồn ở trên.
