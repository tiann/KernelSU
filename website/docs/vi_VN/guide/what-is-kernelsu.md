# KernelSU là gì?

KernelSU là một giải pháp root dành cho các thiết bị Android GKI, nó hoạt động ở chế độ kernel và sẽ cho phép truy cập quyền root cho ứng dụng ở userspace ngay trên không gian của kernel.

## Tính năng

Tính năng chính của KernelSU là nó **Kernel-based** (dựa trên Kernel ?). KernelSU hoạt động ở chế độ kernel, vật nên nó sẽ cung cấp các giao diên của kernel mà từ trước tới nay ta chưa từng có. Ví dụ, chúng ta có thể thêm hardware breakpoint vào bất kì tiến trình nào trong chế độ kernel; Chúng ta có thể truy cập vào bố nhớ vật lí của bất kì tiến trình nào mà không ai có thể phát hiện ra; Hoặc chúng ta có thể chặn bất kì syscall nào ở không gian kernel; etc.

Đồng thời, KernelSU cung cấp một hệ thống module sử dụng overlayfs, cho phép bạn có thể thêm plugin của bạn vào trong hệ thống. Nó còn có thể cung cấp một cơ chế giúp chỉnh sửa được các file trên phân vùng `/system`

## Hướng dẫn sử dụng

Xin hãy xem: [Cách cài đặt](installation)

## Cách để build

[Cách để build](how-to-build)

## Thảo luận

- Telegram: [@KernelSU](https://t.me/KernelSU)
