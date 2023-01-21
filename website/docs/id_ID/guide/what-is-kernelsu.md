# Apa itu KernelSU?

KernelSU adalah solusi root untuk perangkat GKI Android, ia bekerja dalam mode kernel dan memberikan izin root ke aplikasi userspace secara langsung di ruang kernel.

## Fitur

Fitur utama dari KernelSU adalah **berbasis kernel**. KernelSU bekerja dalam mode kernel, sehingga dapat menyediakan antarmuka kernel yang belum pernah kita miliki sebelumnya. Sebagai contoh, kita dapat menambahkan breakpoint perangkat keras ke proses apa pun dalam mode kernel; Kita dapat mengakses memori fisik dari proses apa pun tanpa diketahui oleh siapa pun; Kita dapat mencegat syscall apa pun di ruang kernel; dll.

Dan juga, KernelSU menyediakan sistem modul melalui overlayfs, yang memungkinkan Anda untuk memuat plugin kustom Anda ke dalam sistem. KernelSU juga menyediakan mekanisme untuk memodifikasi berkas-berkas pada partisi `/system`.

## Bagaimana cara menggunakannya

Silakan merujuk ke: [Installation](installation)

## Bagaimana cara men-buildnya

[How to build](how-to-build)

## Diskusi

- Telegram: [@KernelSU](https://t.me/KernelSU)
