# KernelSU là gì?

KernelSU là một giải pháp root cho các thiết bị Android GKI, nó hoạt động ở chế độ kernel và cấp quyền root cho ứng dụng không gian người dùng trực tiếp trong không gian kernel.

## Tính năng

Tính năng chính của KernelSU là **Kernel-based** (dựa trên Kernel). KernelSU hoạt động ở chế độ kernel nên nó có thể cung cấp giao diện kernel mà chúng ta chưa từng có trước đây. Ví dụ: chúng ta có thể thêm điểm dừng phần cứng vào bất kỳ quy trình nào ở chế độ kernel; Chúng ta có thể truy cập bộ nhớ vật lý của bất kỳ quy trình nào mà không bị phát hiện; Chúng ta còn có thể chặn bất kỳ syscall nào trong không gian kernel; v.v.

Ngoài ra, KernelSU cung cấp [hệ thống metamodule](metamodule.md), đây là một kiến trúc có thể cắm để quản lý module. Không giống như các giải pháp root truyền thống tích hợp logic mount vào lõi, KernelSU ủy thác điều này cho metamodules. Điều này cho phép bạn cài đặt metamodules (như [meta-overlayfs](https://github.com/tiann/KernelSU/tree/main/userspace/meta-overlayfs)) để cung cấp các sửa đổi systemless cho phân vùng `/system` và các phân vùng khác.

## Hướng dẫn sử dụng

Xin hãy xem: [Cách cài đặt](installation)

## Cách để build

[Cách để build](how-to-build)

## Thảo luận

- Telegram: [@KernelSU](https://t.me/KernelSU)
