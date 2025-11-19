# FAQ

## Apakah KernelSU mendukung perangkat saya?

KernelSU mendukung perangkat yang menjalankan Android dengan bootloader yang tidak terkunci. Namun, dukungan resmi hanya untuk GKI Linux Kernel 5.10+ (dalam praktiknya, ini berarti perangkat Anda harus memiliki Android 12 out-of-the-box agar didukung).

Anda dapat dengan mudah memeriksa dukungan untuk perangkat Anda melalui aplikasi manajer KernelSU, yang tersedia [di sini](https://github.com/tiann/KernelSU/releases).

Jika aplikasi menunjukkan `Not installed`, berarti perangkat Anda secara resmi didukung oleh KernelSU.

Jika aplikasi menunjukkan `Unsupported`, berarti perangkat Anda tidak didukung secara resmi saat ini. Namun, Anda dapat membangun kode sumber kernel dan mengintegrasikan KernelSU untuk membuatnya bekerja, atau gunakan [Perangkat yang didukung tidak resmi](unofficially-support-devices).

## Apakah KernelSU membutuhkan buka bootloader?

Ya, tentu saja.

## Apakah KernelSU mendukung modul?

Ya, sebagian besar modul Magisk bekerja di KernelSU. Namun, jika modul Anda perlu memodifikasi file `/system`, Anda perlu menginstal [metamodule](metamodule.md) (seperti `meta-overlayfs`). Fitur modul lainnya bekerja tanpa metamodule. Periksa [Panduan modul](module.md) untuk info lebih lanjut.

## Apakah KernelSU mendukung Xposed?

Ya, Anda dapat menggunakan LSPosed (atau turunan Xposed modern lainnya) dengan [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext).

## Apakah KernelSU mendukung Zygisk?

KernelSU tidak memiliki dukungan Zygisk bawaan, tetapi Anda dapat menggunakan modul seperti [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext) untuk mendukungnya.

## Apakah KernelSU kompatibel dengan Magisk?

Sistem modul KernelSU bertentangan dengan magic mount Magisk. Jika ada modul yang diaktifkan di KernelSU, maka seluruh Magisk akan berhenti bekerja.

Namun, jika Anda hanya menggunakan `su` dari KernelSU, ini akan bekerja dengan baik dengan Magisk. KernelSU memodifikasi `kernel`, sedangkan Magisk memodifikasi `ramdisk`, memungkinkan keduanya bekerja bersama.

## Akankah KernelSU menggantikan Magisk?

Kami percaya tidak, dan itu bukan tujuan kami. Magisk sudah cukup baik untuk solusi root userspace dan akan memiliki umur yang panjang. Tujuan KernelSU adalah untuk menyediakan antarmuka kernel kepada pengguna, bukan untuk menggantikan Magisk.

## Dapatkah KernelSU mendukung perangkat non-GKI?

Ada kemungkinan. Tetapi Anda harus mengunduh sumber kernel dan mengintegrasikan KernelSU ke dalam source tree, dan mengkompilasi kernel sendiri.

## Dapatkah KernelSU mendukung perangkat di bawah Android 12?

Kernel perangkat yang mempengaruhi kompatibilitas KernelSU, dan tidak ada hubungannya dengan versi Android. Satu-satunya batasan adalah bahwa perangkat yang diluncurkan dengan Android 12 harus memiliki versi kernel 5.10+ (perangkat GKI). Jadi:

1. Perangkat yang diluncurkan dengan Android 12 harus didukung.
2. Perangkat dengan kernel lama (beberapa perangkat dengan Android 12 juga memiliki kernel lama) kompatibel (Anda harus membangun kernel sendiri).

## Dapatkah KernelSU mendukung kernel lama?

Ada kemungkinan. KernelSU sekarang telah di-backport ke kernel 4.14. Untuk kernel yang lebih lama, Anda perlu melakukan backport secara manual, dan PR selalu diterima!

## Bagaimana cara mengintegrasikan KernelSU untuk kernel lama?

Silakan periksa panduan [Integrasi untuk perangkat non-GKI](how-to-integrate-for-non-gki).

## Mengapa versi Android saya 13, dan kernel menunjukkan "android12-5.10"?

Versi kernel tidak ada hubungannya dengan versi Android. Jika Anda perlu mem-flash kernel, selalu gunakan versi kernel; versi Android tidak sepenting itu.

## Saya GKI 1.0, bisakah saya menggunakan ini?

GKI 1.0 sama sekali berbeda dari GKI 2.0, Anda harus mengkompilasi kernel sendiri.

## Bagaimana cara membuat `/system` RW?

Kami tidak merekomendasikan Anda memodifikasi partisi sistem secara langsung. Silakan periksa [Panduan modul](module.md) untuk memodifikasinya secara systemless. Jika Anda bersikeras melakukan ini, periksa [magisk_overlayfs](https://github.com/HuskyDG/magic_overlayfs).

## Bisakah KernelSU memodifikasi hosts? Bagaimana cara menggunakan AdAway?

Tentu saja. Tetapi KernelSU tidak memiliki dukungan hosts bawaan, Anda dapat menginstal modul seperti [systemless-hosts](https://github.com/symbuzzer/systemless-hosts-KernelSU-module) untuk melakukannya.

## Mengapa modul saya tidak bekerja setelah instalasi baru?

Jika modul Anda perlu memodifikasi file `/system`, Anda perlu menginstal [metamodule](metamodule.md) untuk me-mount direktori `system`. Fitur modul lainnya (skrip, sepolicy, system.prop) bekerja tanpa metamodule.

**Solusi**: Lihat [Panduan Metamodule](metamodule.md) untuk instruksi instalasi.

## Apa itu metamodule dan mengapa saya membutuhkannya?

Metamodule adalah modul khusus yang menyediakan infrastruktur untuk me-mount modul reguler. Lihat [Panduan Metamodule](metamodule.md) untuk penjelasan lengkap.
