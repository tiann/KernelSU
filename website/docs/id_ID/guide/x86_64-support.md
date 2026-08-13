# Dukungan x86_64

KernelSU mendukung arsitektur `x86_64` sepenuhnya. Namun, karena perubahan keamanan upstream kernel terbaru, integrasi KernelSU pada kernel `x86_64` modern memerlukan penanganan tambahan agar unified syscall dispatcher milik kita dapat berfungsi dengan benar.

## Mengapa ini rusak?

Pada versi kernel yang lebih baru, sebuah [commit](https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/commit/?id=1e3ad78334a69b36e107232e337f9d693dcc9df2) diperkenalkan untuk memperketat syscall table. Perubahan ini mengubah branch tidak langsung dalam jalur system call menjadi serangkaian branch kondisional langsung.

Mekanisme `syscall_hook` KernelSU bergantung pada modifikasi entri di syscall table agar system call yang dicegat dapat diarahkan ke unified dispatcher. Karena hardening baru mengubah jalur system call, kernel akan mengabaikan modifikasi pada syscall table tersebut. Jika KernelSU mencoba dimuat dan melakukan hook pada syscall table tanpa menangani pembatasan ini dengan benar, ia tidak akan dapat merutekan panggilan dan akan membatalkan inisialisasi untuk mencegah kernel panic.

## Bagaimana cara memperbaikinya?

Sekarang ada dua cara yang didukung untuk menangani masalah syscall hook pada `x86_64`:

1. Aktifkan opsi build kernel `KSU_X86_PATCH_SYSCALL_DISPATCHER`.
2. Tetap gunakan metode patch kode sumber kernel yang lama.

Anda hanya perlu memakai salah satunya. Jangan menerapkan keduanya sekaligus.

### Opsi 1: Aktifkan `KSU_X86_PATCH_SYSCALL_DISPATCHER`

KernelSU 3.2.6 memperkenalkan mekanisme resmi baru untuk `x86_64`, yaitu opsi build `KSU_X86_PATCH_SYSCALL_DISPATCHER`.

Saat opsi ini diaktifkan, KernelSU akan melakukan patch dinamis pada hardened syscall dispatcher saat runtime sehingga syscall hook dapat bekerja tanpa memerlukan kumpulan patch kode sumber kernel lama. Ini adalah pendekatan yang direkomendasikan jika Anda membangun kernel dengan KernelSU 3.2.6 atau yang lebih baru.

### Opsi 2: Terapkan patch kode sumber kernel lama

Jika Anda tidak ingin mengaktifkan `KSU_X86_PATCH_SYSCALL_DISPATCHER`, Anda tetap dapat menggunakan pendekatan patch kernel yang lama.

Agar KernelSU dapat bekerja pada kernel yang lebih baru ini, terapkan patch yang memungkinkan Anda melewati syscall hardening khusus ini.

::: danger PERINGATAN KEAMANAN
Dengan menggunakan salah satu dari dua solusi ini, Anda secara sengaja melewati atau melemahkan mekanisme mitigasi yang dirancang untuk melindungi dari kerentanan speculative execution.

Ini kembali membuka permukaan serangan indirect branch untuk system call. **Jangan gunakan salah satu dari dua solusi ini jika Anda menjalankan server produksi atau sistem yang membutuhkan keamanan side-channel yang ketat.** Solusi-solusi ini ditujukan untuk lingkungan pengujian, di mana akses root melalui KernelSU lebih diprioritaskan daripada mitigasi kerentanan perangkat keras tertentu ini.
:::

Pilih dan terapkan patch yang sesuai dengan versi kernel Anda di bawah ini. Patch ini membuat fitur bernama `X86_FEATURE_INDIRECT_SAFE` dan dapat diaktifkan menggunakan parameter kernel cmdline `syscall_hardening=off`.

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

## Metode mana yang sebaiknya dipilih?

- Jika Anda menggunakan KernelSU 3.2.6 atau lebih baru dan bisa mengubah konfigurasi build KernelSU, aktifkan `KSU_X86_PATCH_SYSCALL_DISPATCHER`.
- Jika Anda ingin mempertahankan alur kerja patch kernel saat ini, lanjutkan menggunakan patch kode sumber di atas.
