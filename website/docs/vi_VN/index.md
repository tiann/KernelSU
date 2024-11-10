---
layout: home
title: Giải pháp root dựa trên kernel dành cho Android

hero:
  name: KernelSU
  text: Giải pháp root dựa trên kernel dành cho Android
  tagline: ""
  image:
    src: /logo.png
    alt: KernelSU
  actions:
    - theme: brand
      text: Bắt Đầu
      link: /guide/what-is-kernelsu
    - theme: alt
      text: Xem trên GitHub
      link: https://github.com/tiann/KernelSU

features:
  - title: Dựa trên Kernel
    details: KernelSU đang hoạt động ở chế độ kernel Linux, nó có nhiều quyền kiểm soát hơn đối với các ứng dụng userspace.
  - title: Kiểm soát truy cập bằng whitelist
    details: Chỉ ứng dụng được cấp quyền root mới có thể truy cập `su`, các ứng dụng khác không thể nhận được su.
  - title: Quyền root bị hạn chế
    details: KernelSU cho phép bạn tùy chỉnh uid, gid, group, capabilities và các quy tắc SELinux của su. Giới hạn sức mạnh của root.
  - title: Mô-đun & Nguồn mở
    details: KernelSU hỗ trợ sửa đổi /system một cách systemless bằng overlayfs và nó có nguồn mở theo GPL-3.

