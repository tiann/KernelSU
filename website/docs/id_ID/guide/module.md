# Panduan module

KernelSU menyediakan mekanisme modul yang mencapai efek memodifikasi direktori sistem dengan tetap menjaga integritas partisi sistem. Mekanisme ini umumnya dikenal sebagai "tanpa sistem".

Mekanisme modul KernelSU hampir sama dengan Magisk. Jika Anda terbiasa dengan pengembangan modul Magisk, mengembangkan modul KernelSU sangat mirip. Anda dapat melewati pengenalan modul di bawah ini dan hanya perlu membaca [difference-with-magisk](difference-with-magisk.md).

## Busybox

KernelSU dikirimkan dengan fitur biner BusyBox yang lengkap (termasuk dukungan penuh SELinux). Eksekusi terletak di `/data/adb/ksu/bin/busybox`. BusyBox KernelSU mendukung "Mode Shell Standalone Shell" yang dapat dialihkan waktu proses. Apa yang dimaksud dengan mode mandiri ini adalah bahwa ketika dijalankan di shell `ash` dari BusyBox, setiap perintah akan langsung menggunakan applet di dalam BusyBox, terlepas dari apa yang ditetapkan sebagai `PATH`. Misalnya, perintah seperti `ls`, `rm`, `chmod` **TIDAK** akan menggunakan apa yang ada di `PATH` (dalam kasus Android secara default akan menjadi `/system/bin/ls`, ` /system/bin/rm`, dan `/system/bin/chmod` masing-masing), tetapi akan langsung memanggil applet BusyBox internal. Ini memastikan bahwa skrip selalu berjalan di lingkungan yang dapat diprediksi dan selalu memiliki rangkaian perintah lengkap, apa pun versi Android yang menjalankannya. Untuk memaksa perintah _not_ menggunakan BusyBox, Anda harus memanggil yang dapat dieksekusi dengan path lengkap.

Setiap skrip shell tunggal yang berjalan dalam konteks KernelSU akan dieksekusi di shell `ash` BusyBox dengan mode mandiri diaktifkan. Untuk apa yang relevan dengan pengembang pihak ke-3, ini termasuk semua skrip boot dan skrip instalasi modul.

Bagi yang ingin menggunakan fitur “Standalone Mode” ini di luar KernelSU, ada 2 cara untuk mengaktifkannya:

1. Tetapkan variabel lingkungan `ASH_STANDALONE` ke `1`<br>Contoh: `ASH_STANDALONE=1 /data/adb/ksu/bin/busybox sh <script>`
2. Beralih dengan opsi baris perintah:<br>`/data/adb/ksu/bin/busybox sh -o mandiri <script>`

Untuk memastikan semua shell `sh` selanjutnya dijalankan juga dalam mode mandiri, opsi 1 adalah metode yang lebih disukai (dan inilah yang digunakan secara internal oleh KernelSU dan manajer KernelSU) karena variabel lingkungan diwariskan ke proses anak.

::: perbedaan tip dengan Magisk

BusyBox KernelSU sekarang menggunakan file biner yang dikompilasi langsung dari proyek Magisk. **Berkat Magisk!** Oleh karena itu, Anda tidak perlu khawatir tentang masalah kompatibilitas antara skrip BusyBox di Magisk dan KernelSU karena keduanya persis sama!
:::

## KernelSU module

Modul KernelSU adalah folder yang ditempatkan di `/data/adb/modules` dengan struktur di bawah ini:

```txt
/data/adb/modules
├── .
├── .
|
├── $MODID                  <--- The folder is named with the ID of the module
│   │
│   │      *** Module Identity ***
│   │
│   ├── module.prop         <--- This file stores the metadata of the module
│   │
│   │      *** Main Contents ***
│   │
│   ├── system              <--- This folder will be mounted if skip_mount does not exist
│   │   ├── ...
│   │   ├── ...
│   │   └── ...
│   │
│   │      *** Status Flags ***
│   │
│   ├── skip_mount          <--- If exists, KernelSU will NOT mount your system folder
│   ├── disable             <--- If exists, the module will be disabled
│   ├── remove              <--- If exists, the module will be removed next reboot
│   │
│   │      *** Optional Files ***
│   │
│   ├── post-fs-data.sh     <--- This script will be executed in post-fs-data
│   ├── service.sh          <--- This script will be executed in late_start service
|   ├── uninstall.sh        <--- This script will be executed when KernelSU removes your module
│   ├── system.prop         <--- Properties in this file will be loaded as system properties by resetprop
│   ├── sepolicy.rule       <--- Additional custom sepolicy rules
│   │
│   │      *** Auto Generated, DO NOT MANUALLY CREATE OR MODIFY ***
│   │
│   ├── vendor              <--- A symlink to $MODID/system/vendor
│   ├── product             <--- A symlink to $MODID/system/product
│   ├── system_ext          <--- A symlink to $MODID/system/system_ext
│   │
│   │      *** Any additional files / folders are allowed ***
│   │
│   ├── ...
│   └── ...
|
├── another_module
│   ├── .
│   └── .
├── .
├── .
```

::: perbedaan tip dengan Magisk
KernelSU tidak memiliki dukungan bawaan untuk Zygisk, jadi tidak ada konten terkait Zygisk dalam modul. Namun, Anda dapat menggunakan [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext) untuk mendukung modul Zygisk. Dalam hal ini, konten modul Zygisk identik dengan yang didukung oleh Magisk.
:::

### module.prop

module.prop adalah file konfigurasi untuk sebuah modul. Di KernelSU, jika modul tidak berisi file ini, maka tidak akan dikenali sebagai modul. Format file ini adalah sebagai berikut:

```txt
id=<string>
name=<string>
version=<string>
versioncode=<int>
author=<string>
description=<string>
```

- `id` harus cocok dengan ekspresi reguler ini: `^[a-zA-Z][a-zA-Z0-9._-]+$`<br>
   contoh: ✓ `a_module`, ✓ `a.module`, ✓ `module-101`, ✗ `a module`, ✗ `1_module`, ✗ `-a-module`<br>
   Ini adalah **pengidentifikasi unik** modul Anda. Anda tidak boleh mengubahnya setelah dipublikasikan.
- `versionCode` harus berupa **integer**. Ini digunakan untuk membandingkan versi
- Lainnya yang tidak disebutkan di atas dapat berupa string **satu baris**.
- Pastikan untuk menggunakan tipe jeda baris `UNIX (LF)` dan bukan `Windows (CR+LF)` atau `Macintosh (CR)`.

### Shell skrip

Harap baca bagian [Boot Scripts](#boot-scripts) untuk memahami perbedaan antara `post-fs-data.sh` dan `service.sh`. Untuk sebagian besar pengembang modul, `service.sh` sudah cukup baik jika Anda hanya perlu menjalankan skrip boot.

Di semua skrip modul Anda, harap gunakan `MODDIR=${0%/*}` untuk mendapatkan jalur direktori dasar modul Anda; lakukan **TIDAK** hardcode jalur modul Anda dalam skrip.

::: perbedaan tip dengan Magisk
Anda dapat menggunakan variabel lingkungan KSU untuk menentukan apakah skrip berjalan di KernelSU atau Magisk. Jika berjalan di KernelSU, nilai ini akan disetel ke true.
:::

### `system` directory

Isi direktori ini akan dihamparkan di atas partisi sistem /sistem menggunakan overlayfs setelah sistem di-boot. Ini berarti bahwa:

1. File dengan nama yang sama dengan yang ada di direktori terkait di sistem akan ditimpa oleh file di direktori ini.
2. Folder dengan nama yang sama dengan yang ada di direktori terkait di sistem akan digabungkan dengan folder di direktori ini.

Jika Anda ingin menghapus file atau folder di direktori sistem asli, Anda perlu membuat file dengan nama yang sama dengan file/folder di direktori modul menggunakan `mknod filename c 0 0`. Dengan cara ini, sistem overlayfs akan secara otomatis "memutihkan" file ini seolah-olah telah dihapus (partisi / sistem sebenarnya tidak diubah).

Anda juga dapat mendeklarasikan variabel bernama `REMOVE` yang berisi daftar direktori di `customize.sh` untuk menjalankan operasi penghapusan, dan KernelSU akan secara otomatis mengeksekusi `mknod <TARGET> c 0 0` di direktori modul yang sesuai. Misalnya:

``` sh
HAPUS = "
/sistem/aplikasi/YouTube
/system/app/Bloatware
"
```

Daftar di atas akan mengeksekusi `mknod $MODPATH/system/app/YouTuBe c 0 0` dan `mknod $MODPATH/system/app/Bloatware c 0 0`; dan `/system/app/YouTube` dan `/system/app/Bloatware` akan dihapus setelah modul berlaku.

Jika Anda ingin mengganti direktori di sistem, Anda perlu membuat direktori dengan jalur yang sama di direktori modul Anda, lalu atur atribut `setfattr -n trusted.overlay.opaque -v y <TARGET>` untuk direktori ini. Dengan cara ini, sistem overlayfs akan secara otomatis mengganti direktori terkait di sistem (tanpa mengubah partisi /sistem).

Anda dapat mendeklarasikan variabel bernama `REPLACE` di file `customize.sh` Anda, yang menyertakan daftar direktori yang akan diganti, dan KernelSU akan secara otomatis melakukan operasi yang sesuai di direktori modul Anda. Misalnya:

REPLACE="
/system/app/YouTube
/system/app/Bloatware
"

Daftar ini akan secara otomatis membuat direktori `$MODPATH/system/app/YouTube` dan `$MODPATH/system/app/Bloatware`, lalu jalankan `setfattr -n trusted.overlay.opaque -v y $MODPATH/system/app/ YouTube` dan `setfattr -n trusted.overlay.opaque -v y $MODPATH/system/app/Bloatware`. Setelah modul berlaku, `/system/app/YouTube` dan `/system/app/Bloatware` akan diganti dengan direktori kosong.

::: perbedaan tip dengan Magisk

Mekanisme tanpa sistem KernelSU diimplementasikan melalui overlay kernel, sementara Magisk saat ini menggunakan magic mount (bind mount). Kedua metode implementasi tersebut memiliki perbedaan yang signifikan, tetapi tujuan utamanya sama: untuk memodifikasi file / sistem tanpa memodifikasi partisi / sistem secara fisik.
:::

Jika Anda tertarik dengan overlayfs, disarankan untuk membaca [dokumentasi overlayfs](https://docs.kernel.org/filesystems/overlayfs.html) Kernel Linux.

### system.prop

File ini mengikuti format yang sama dengan `build.prop`. Setiap baris terdiri dari `[kunci]=[nilai]`.

### sepolicy.rule

Jika modul Anda memerlukan beberapa tambalan sepolicy tambahan, harap tambahkan aturan tersebut ke dalam file ini. Setiap baris dalam file ini akan diperlakukan sebagai pernyataan kebijakan.

## Pemasangan module

Penginstal modul KernelSU adalah modul KernelSU yang dikemas dalam file zip yang dapat di-flash di aplikasi pengelola KernelSU. Pemasang modul KernelSU yang paling sederhana hanyalah modul KernelSU yang dikemas sebagai file zip.

```txt
module.zip
│
├── customize.sh                       <--- (Optional, more details later)
│                                           This script will be sourced by update-binary
├── ...
├── ...  /* The rest of module's files */
│
```

:::peringatan
Modul KernelSU TIDAK didukung untuk diinstal dalam pemulihan kustom!!
:::

### Kostumisasi

Jika Anda perlu menyesuaikan proses penginstalan modul, secara opsional Anda dapat membuat skrip di penginstal bernama `customize.sh`. Skrip ini akan _sourced_ (tidak dijalankan!) oleh skrip penginstal modul setelah semua file diekstrak dan izin default serta konteks sekon diterapkan. Ini sangat berguna jika modul Anda memerlukan penyiapan tambahan berdasarkan ABI perangkat, atau Anda perlu menyetel izin khusus/konteks kedua untuk beberapa file modul Anda.

Jika Anda ingin sepenuhnya mengontrol dan menyesuaikan proses penginstalan, nyatakan `SKIPUNZIP=1` di `customize.sh` untuk melewati semua langkah penginstalan default. Dengan melakukannya, `customize.sh` Anda akan bertanggung jawab untuk menginstal semuanya dengan sendirinya.

Skrip `customize.sh` berjalan di shell BusyBox `ash` KernelSU dengan "Mode Mandiri" diaktifkan. Variabel dan fungsi berikut tersedia:

#### Variable

- `KSU` (bool): variabel untuk menandai bahwa skrip berjalan di lingkungan KernelSU, dan nilai variabel ini akan selalu benar. Anda dapat menggunakannya untuk membedakan antara KernelSU dan Magisk.
- `KSU_VER` (string): string versi dari KernelSU yang diinstal saat ini (mis. `v0.4.0`)
- `KSU_VER_CODE` (int): kode versi KernelSU yang terpasang saat ini di ruang pengguna (mis. `10672`)
- `KSU_KERNEL_VER_CODE` (int): kode versi KernelSU yang terpasang saat ini di ruang kernel (mis. `10672`)
- `BOOTMODE` (bool): selalu `true` di KernelSU
- `MODPATH` (jalur): jalur tempat file modul Anda harus diinstal
- `TMPDIR` (jalur): tempat di mana Anda dapat menyimpan file untuk sementara
- `ZIPFILE` (jalur): zip instalasi modul Anda
- `ARCH` (string): arsitektur CPU perangkat. Nilainya adalah `arm`, `arm64`, `x86`, atau `x64`
- `IS64BIT` (bool): `true` jika `$ARCH` adalah `arm64` atau `x64`
- `API` (int): level API (versi Android) perangkat (mis. `23` untuk Android 6.0)

::: peringatan
Di KernelSU, MAGISK_VER_CODE selalu 25200 dan MAGISK_VER selalu v25.2. Tolong jangan gunakan kedua variabel ini untuk menentukan apakah itu berjalan di KernelSU atau tidak.
:::

#### Fungsi

```txt
ui_print <msg>
    print <msg> to console
    Avoid using 'echo' as it will not display in custom recovery's console

abort <msg>
    print error message <msg> to console and terminate the installation
    Avoid using 'exit' as it will skip the termination cleanup steps

set_perm <target> <owner> <group> <permission> [context]
    if [context] is not set, the default is "u:object_r:system_file:s0"
    this function is a shorthand for the following commands:
       chown owner.group target
       chmod permission target
       chcon context target

set_perm_recursive <directory> <owner> <group> <dirpermission> <filepermission> [context]
    if [context] is not set, the default is "u:object_r:system_file:s0"
    for all files in <directory>, it will call:
       set_perm file owner group filepermission context
    for all directories in <directory> (including itself), it will call:
       set_perm dir owner group dirpermission context
```

## Boot scripts

Di KernelSU, skrip dibagi menjadi dua jenis berdasarkan mode operasinya: mode post-fs-data dan mode layanan late_start:

- mode pasca-fs-data
   - Tahap ini adalah BLOKIR. Proses boot dijeda sebelum eksekusi selesai, atau 10 detik telah berlalu.
   - Skrip dijalankan sebelum modul apa pun dipasang. Ini memungkinkan pengembang modul untuk menyesuaikan modul mereka secara dinamis sebelum dipasang.
   - Tahap ini terjadi sebelum Zygote dimulai, yang berarti segalanya di Android
   - **PERINGATAN:** menggunakan `setprop` akan menghentikan proses booting! Silakan gunakan `resetprop -n <prop_name> <prop_value>` sebagai gantinya.
   - **Hanya jalankan skrip dalam mode ini jika perlu.**
- mode layanan late_start
   - Tahap ini NON-BLOCKING. Skrip Anda berjalan paralel dengan proses booting lainnya.
   - **Ini adalah tahap yang disarankan untuk menjalankan sebagian besar skrip.**

Di KernelSU, skrip startup dibagi menjadi dua jenis berdasarkan lokasi penyimpanannya: skrip umum dan skrip modul:

- Skrip Umum
   - Ditempatkan di `/data/adb/post-fs-data.d` atau `/data/adb/service.d`
   - Hanya dieksekusi jika skrip disetel sebagai dapat dieksekusi (`chmod +x script.sh`)
   - Skrip di `post-fs-data.d` berjalan dalam mode post-fs-data, dan skrip di `service.d` berjalan di mode layanan late_start.
   - Modul seharusnya **TIDAK** menambahkan skrip umum selama instalasi
- Skrip Modul
   - Ditempatkan di folder modul itu sendiri
   - Hanya dijalankan jika modul diaktifkan
   - `post-fs-data.sh` berjalan dalam mode post-fs-data, dan `service.sh` berjalan dalam mode layanan late_start.

Semua skrip boot akan berjalan di shell BusyBox `ash` KernelSU dengan "Mode Mandiri" diaktifkan.
