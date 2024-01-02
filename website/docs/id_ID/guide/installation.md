# Instalasi

## Periksa apakah perangkat Anda didukung

Unduh aplikasi manajer KernelSU dari [github releases](https://github.com/tiann/KernelSU/releases), lalu instal aplikasi ke perangkat dan buka aplikasi:

- Jika aplikasi menunjukkan `Unsupported`, itu berarti **Anda harus mengkompilasi kernel sendiri**, KernelSU tidak akan dan tidak pernah menyediakan boot image untuk Anda flash.
- Jika aplikasi menunjukkan `Not installed`, maka perangkat Anda secara resmi didukung oleh KernelSU.

## Temukan boot.img yang tepat

KernelSU menyediakan boot.img umum untuk perangkat GKI, Anda harus mem-flash boot.img ke partisi boot perangkat Anda.

Anda dapat mengunduh boot.img dari [github actions for kernel] (https://github.com/tiann/KernelSU/actions/workflows/build-kernel.yml), perlu diketahui bahwa Anda harus menggunakan versi boot.img yang tepat. Sebagai contoh, jika perangkat Anda menunjukkan bahwa kernelnya adalah `5.10.101`, maka Anda harus mengunduh `5.10.101-xxxx.boot.xxx`.

Dan juga, silakan periksa format boot.img Anda, Anda harus menggunakan format yang tepat, seperti `lz4`、`gz`.

## Flash boot.img ke perangkat

Hubungkan perangkat Anda dengan `adb` lalu jalankan `adb reboot bootloader` untuk masuk ke mode fastboot, lalu gunakan perintah ini untuk mem-flash KernelSU:

```sh
fastboot flash boot boot.img
```

## Reboot

Ketika di-flash, Anda harus menyalakan ulang perangkat Anda:

```sh
fastboot reboot
```
