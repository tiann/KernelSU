# Instalasi

## Periksa apakah perangkat Anda didukung

Unduh manajer KernelSU dari [GitHub Releases](https://github.com/tiann/KernelSU/releases) dan instal ke perangkat Anda:

- Jika aplikasi menunjukkan `Unsupported`, itu berarti **Anda harus mengkompilasi kernel sendiri**, KernelSU tidak akan dan tidak pernah menyediakan file boot.img untuk Anda flash.
- Jika aplikasi menunjukkan `Not installed`, maka perangkat Anda secara resmi didukung oleh KernelSU.

::: info
Untuk perangkat yang menunjukkan `Unsupported`, Anda dapat memeriksa daftar [Perangkat yang didukung tidak resmi](unofficially-support-devices.md). Anda dapat mengkompilasi kernel sendiri.
:::

## Cadangkan boot.img stok

Sebelum flashing, sangat penting untuk mencadangkan boot.img stok Anda. Jika Anda mengalami bootloop, Anda selalu dapat memulihkan sistem dengan mem-flash kembali ke boot pabrik stok menggunakan fastboot.

::: warning
Flashing dapat menyebabkan kehilangan data. Pastikan untuk melakukan langkah ini dengan baik sebelum melanjutkan ke langkah berikutnya! Anda juga dapat mencadangkan semua data di perangkat Anda jika diperlukan.
:::

## Pengetahuan yang diperlukan

### ADB dan fastboot

Secara default, Anda akan menggunakan alat ADB dan fastboot dalam tutorial ini, jadi jika Anda tidak mengetahuinya, kami sarankan menggunakan mesin pencari untuk mempelajarinya terlebih dahulu.

### KMI

Kernel Module Interface (KMI), versi kernel dengan KMI yang sama **kompatibel**, inilah yang dimaksud dengan "general" dalam GKI; sebaliknya, jika KMI berbeda, maka kernel ini tidak kompatibel satu sama lain, dan mem-flash image kernel dengan KMI yang berbeda dari perangkat Anda dapat menyebabkan bootloop.

Secara khusus, untuk perangkat GKI, format versi kernel harus sebagai berikut:

```txt
KernelRelease :=
Version.PatchLevel.SubLevel-AndroidRelease-KmiGeneration-suffix
w      .x         .y       -zzz           -k            -something
```

`w.x-zzz-k` adalah versi KMI. Misalnya, jika versi kernel perangkat adalah `5.10.101-android12-9-g30979850fc20`, maka KMI-nya adalah `5.10-android12-9`. Secara teoritis, dapat boot secara normal dengan kernel KMI lainnya.

::: tip
Perhatikan bahwa SubLevel dalam versi kernel bukan bagian dari KMI! Ini berarti `5.10.101-android12-9-g30979850fc20` memiliki KMI yang sama dengan `5.10.137-android12-9-g30979850fc20`!
:::

### Tingkat patch keamanan {#security-patch-level}

Perangkat Android yang lebih baru mungkin memiliki mekanisme anti-rollback yang mencegah flashing image boot dengan tingkat patch keamanan lama. Misalnya, jika kernel perangkat Anda adalah `5.10.101-android12-9-g30979850fc20`, tingkat patch keamanan adalah `2023-11`; bahkan jika Anda mem-flash kernel yang sesuai dengan KMI, jika tingkat patch keamanan lebih lama dari `2023-11` (seperti `2023-06`), ini dapat menyebabkan bootloop.

Oleh karena itu, kernel dengan tingkat patch keamanan terbaru lebih disukai untuk menjaga kompatibilitas dengan KMI.

### Versi kernel vs versi Android

Harap dicatat: **Versi kernel dan versi Android tidak harus sama!**

Jika Anda menemukan bahwa versi kernel Anda adalah `android12-5.10.101`, tetapi versi sistem Android Anda adalah Android 13 atau lainnya, jangan heran, karena nomor versi sistem Android tidak harus sama dengan nomor versi kernel Linux. Nomor versi kernel Linux umumnya sesuai dengan versi sistem Android yang disertakan dengan **perangkat saat dikirim**. Jika sistem Android diupgrade nanti, versi kernel umumnya tidak akan berubah. Jadi, sebelum mem-flash apa pun, **selalu rujuk versi kernel!**

## Pendahuluan

Sejak versi [0.9.0](https://github.com/tiann/KernelSU/releases/tag/v0.9.0), KernelSU mendukung dua mode berjalan pada perangkat GKI:

1. `GKI`: Ganti kernel asli perangkat dengan **Generic Kernel Image** (GKI) yang disediakan oleh KernelSU.
2. `LKM`: Muat **Loadable Kernel Module** (LKM) ke dalam kernel perangkat tanpa mengganti kernel asli.

Kedua mode ini cocok untuk skenario yang berbeda, dan Anda dapat memilih salah satu sesuai kebutuhan Anda.

### Mode GKI {#gki-mode}

Dalam mode GKI, kernel asli perangkat akan diganti dengan image kernel generik yang disediakan oleh KernelSU. Keuntungan mode GKI adalah:

1. Universalitas yang kuat, cocok untuk sebagian besar perangkat. Misalnya, Samsung telah mengaktifkan perangkat KNOX, dan mode LKM tidak dapat berfungsi. Ada juga beberapa perangkat yang dimodifikasi khusus yang hanya dapat menggunakan mode GKI.
2. Dapat digunakan tanpa bergantung pada firmware resmi, dan tidak perlu menunggu pembaruan firmware resmi, selama KMI konsisten, dapat digunakan.

### Mode LKM {#lkm-mode}

Dalam mode LKM, kernel asli perangkat tidak akan diganti, tetapi loadable kernel module akan dimuat ke dalam kernel perangkat. Keuntungan mode LKM adalah:

1. Tidak akan mengganti kernel asli perangkat. Jika Anda memiliki persyaratan khusus untuk kernel asli perangkat, atau Anda ingin menggunakan KernelSU sambil menggunakan kernel pihak ketiga, Anda dapat menggunakan mode LKM.
2. Lebih nyaman untuk upgrade dan OTA. Saat mengupgrade KernelSU, Anda dapat langsung menginstalnya di manajer tanpa flashing manual. Setelah OTA sistem, Anda dapat langsung menginstalnya ke slot kedua tanpa flashing manual.
3. Cocok untuk beberapa skenario khusus. Misalnya, LKM juga dapat dimuat dengan izin root sementara. Karena tidak perlu mengganti partisi boot, tidak akan memicu AVB dan tidak akan menyebabkan perangkat menjadi brick.
4. LKM dapat di-uninstall sementara. Jika Anda ingin menonaktifkan akses root sementara, Anda dapat meng-uninstall LKM. Proses ini tidak memerlukan flashing partisi, atau bahkan me-reboot perangkat. Jika Anda ingin mengaktifkan root lagi, cukup reboot perangkat.

::: tip KOEKSISTENSI DUA MODE
Setelah membuka manajer, Anda dapat melihat mode perangkat saat ini di beranda. Perhatikan bahwa prioritas mode GKI lebih tinggi daripada LKM. Misalnya, jika Anda menggunakan kernel GKI untuk mengganti kernel asli, dan menggunakan LKM untuk mem-patch kernel GKI, LKM akan diabaikan, dan perangkat akan selalu berjalan dalam mode GKI.
:::

### Yang mana yang harus dipilih? {#which-one}

Jika perangkat Anda adalah ponsel, kami sarankan Anda memprioritaskan mode LKM. Jika perangkat Anda adalah emulator, WSA, atau Waydroid, kami sarankan Anda memprioritaskan mode GKI.

## Instalasi LKM

### Dapatkan firmware resmi

Untuk menggunakan mode LKM, Anda perlu mendapatkan firmware resmi dan mem-patch-nya berdasarkan firmware resmi. Jika Anda menggunakan kernel pihak ketiga, Anda dapat menggunakan `boot.img` dari kernel pihak ketiga sebagai firmware resmi.

Ada banyak cara untuk mendapatkan firmware resmi. Jika perangkat Anda mendukung `fastboot boot`, kami merekomendasikan **metode yang paling direkomendasikan dan paling sederhana** adalah menggunakan `fastboot boot` untuk boot sementara kernel GKI yang disediakan oleh KernelSU, lalu instal manajer, dan akhirnya instal langsung di manajer. Metode ini tidak memerlukan pengunduhan firmware resmi secara manual atau ekstraksi boot secara manual.

Jika perangkat Anda tidak mendukung `fastboot boot`, Anda mungkin perlu mengunduh paket firmware resmi secara manual dan mengekstrak boot darinya.

Tidak seperti mode GKI, mode LKM memodifikasi `ramdisk`. Oleh karena itu, pada perangkat dengan Android 13, perlu mem-patch partisi `init_boot` alih-alih partisi `boot`, sedangkan mode GKI selalu beroperasi pada partisi `boot`.

### Gunakan manajer

Buka manajer, klik ikon instalasi di sudut kanan atas, dan beberapa opsi akan muncul:

1. Pilih file. Jika perangkat Anda tidak memiliki hak root, Anda dapat memilih opsi ini lalu pilih firmware resmi Anda. Manajer akan secara otomatis mem-patch-nya. Setelah itu, flash file yang di-patch ini untuk mendapatkan hak root secara permanen.
2. Instalasi langsung. Jika perangkat Anda sudah di-root, Anda dapat memilih opsi ini. Manajer akan secara otomatis mendapatkan informasi perangkat Anda, lalu secara otomatis mem-patch firmware resmi, dan mem-flash-nya secara otomatis. Anda dapat mempertimbangkan menggunakan `fastboot boot` kernel GKI KernelSU untuk mendapatkan root sementara dan menginstal manajer, lalu gunakan opsi ini. Ini juga merupakan cara utama untuk mengupgrade KernelSU.
3. Instal ke slot yang tidak aktif. Jika perangkat Anda mendukung partisi A/B, Anda dapat memilih opsi ini. Manajer akan secara otomatis mem-patch firmware resmi dan menginstalnya ke partisi lain. Metode ini cocok untuk perangkat setelah OTA, Anda dapat langsung menginstalnya ke partisi lain setelah OTA, lalu restart perangkat.

### Gunakan baris perintah

Jika Anda tidak ingin menggunakan manajer, Anda juga dapat menggunakan baris perintah untuk menginstal LKM. Alat `ksud` yang disediakan oleh KernelSU dapat membantu Anda mem-patch firmware resmi dengan cepat lalu mem-flash-nya.

Alat ini mendukung macOS, Linux, dan Windows. Anda dapat mengunduh versi yang sesuai dari [GitHub Release](https://github.com/tiann/KernelSU/releases).

Penggunaan: `ksud boot-patch` Anda dapat memeriksa bantuan baris perintah untuk opsi spesifik.

```sh
oriole:/ # ksud boot-patch -h
Patch boot or init_boot images to apply KernelSU

Usage: ksud boot-patch [OPTIONS]

Options:
  -b, --boot <BOOT>              Boot image path. If not specified, it will try to find the boot image automatically
  -k, --kernel <KERNEL>          Kernel image path to be replaced
  -m, --module <MODULE>          LKM module path to be replaced. If not specified, the built-in module will be used
  -i, --init <INIT>              init to be replaced
  -u, --ota                      Will use another slot if the boot image is not specified
  -f, --flash                    Flash it to boot partition after patch
  -o, --out <OUT>                Output path. If not specified, the current directory will be used
      --magiskboot <MAGISKBOOT>  magiskboot path. If not specified, the built-in version will be used
      --kmi <KMI>                KMI version. If specified, the indicated KMI will be used
  -h, --help                     Print help
```

Beberapa opsi yang perlu dijelaskan:

1. Opsi `--magiskboot` dapat menentukan path magiskboot. Jika tidak ditentukan, ksud akan mencarinya di variabel lingkungan. Jika Anda tidak tahu cara mendapatkan magiskboot, Anda dapat memeriksa [di sini](#patch-boot-image).
2. Opsi `--kmi` dapat menentukan versi `KMI`. Jika nama kernel perangkat Anda tidak mengikuti spesifikasi KMI, Anda dapat menentukannya menggunakan opsi ini.

Penggunaan paling umum adalah:

```sh
ksud boot-patch -b <boot.img> --kmi android13-5.10
```

## Instalasi mode GKI

Ada beberapa metode instalasi untuk mode GKI, masing-masing cocok untuk skenario yang berbeda, jadi pilih sesuai:

1. Instal dengan fastboot menggunakan boot.img yang disediakan oleh KernelSU.
2. Instal dengan aplikasi flash kernel, seperti [Kernel Flasher](https://github.com/capntrips/KernelFlasher/releases).
3. Perbaiki boot.img secara manual dan instal.
4. Instal dengan Recovery kustom (misalnya, TWRP).

## Instal dengan boot.img yang disediakan oleh KernelSU

Jika `boot.img` perangkat Anda menggunakan format kompresi yang umum digunakan, Anda dapat menggunakan image GKI yang disediakan oleh KernelSU untuk mem-flash-nya langsung. Ini tidak memerlukan TWRP atau self-patching image.

### Temukan boot.img yang tepat

KernelSU menyediakan boot.img generik untuk perangkat GKI, dan Anda harus mem-flash boot.img ke partisi boot perangkat.

Anda dapat mengunduh boot.img dari [GitHub Release](https://github.com/tiann/KernelSU/releases). Harap dicatat bahwa Anda harus menggunakan versi boot.img yang benar. Jika Anda tidak tahu file mana yang harus diunduh, baca dengan cermat deskripsi [KMI](#kmi) dan [Tingkat patch keamanan](#security-patch-level) dalam dokumen ini.

Biasanya, ada tiga file boot dalam format berbeda untuk KMI dan tingkat patch keamanan yang sama. Mereka identik kecuali format kompresi kernel. Harap periksa format kompresi kernel dari boot.img asli Anda. Anda harus menggunakan format yang benar, seperti `lz4`, `gz`. Jika Anda menggunakan format kompresi yang salah, Anda mungkin mengalami bootloop setelah mem-flash boot.img.

::: info FORMAT KOMPRESI BOOT.IMG
1. Anda dapat menggunakan magiskboot untuk mendapatkan format kompresi dari boot.img asli Anda. Atau, Anda juga dapat bertanya kepada anggota atau pengembang di komunitas yang memiliki model perangkat yang sama. Juga, format kompresi kernel biasanya tidak berubah, jadi jika Anda boot sukses dengan format kompresi tertentu, Anda dapat mencoba format itu nanti juga.
2. Perangkat Xiaomi biasanya menggunakan `gz` atau `uncompressed`.
3. Untuk perangkat Pixel, ikuti instruksi di bawah ini:
:::

### Flash boot.img ke perangkat

Gunakan `adb` untuk menghubungkan perangkat Anda, lalu jalankan `adb reboot bootloader` untuk masuk ke mode fastboot, dan gunakan perintah ini untuk mem-flash KernelSU:

```sh
fastboot flash boot boot.img
```

::: info
Jika perangkat Anda mendukung `fastboot boot`, Anda dapat terlebih dahulu menggunakan `fastboot boot boot.img` untuk mencoba menggunakan boot.img untuk boot sistem terlebih dahulu. Jika ada yang tidak terduga terjadi, restart lagi untuk boot.
:::

### Reboot

Setelah flash selesai, Anda harus me-reboot perangkat Anda:

```sh
fastboot reboot
```

## Instal dengan Kernel Flasher

Langkah-langkah:

1. Unduh ZIP AnyKernel3. Jika Anda tidak tahu file mana yang harus diunduh, baca dengan cermat deskripsi [KMI](#kmi) dan [Tingkat patch keamanan](#security-patch-level) dalam dokumen ini.
2. Buka aplikasi Kernel Flasher, berikan izin root yang diperlukan, dan gunakan ZIP AnyKernel3 yang disediakan untuk mem-flash.

Dengan cara ini memerlukan aplikasi Kernel Flasher memiliki izin root. Anda dapat menggunakan metode berikut untuk mencapai ini:

1. Perangkat Anda telah di-root. Misalnya, Anda telah menginstal KernelSU dan ingin mengupgrade ke versi terbaru atau Anda telah di-root melalui metode lain (seperti Magisk).
2. Jika perangkat Anda tidak di-root, tetapi perangkat mendukung metode boot sementara `fastboot boot boot.img`, Anda dapat menggunakan image GKI yang disediakan oleh KernelSU untuk boot sementara perangkat Anda, mendapatkan izin root sementara, lalu gunakan aplikasi Kernel Flash untuk mendapatkan hak root permanen.

Beberapa aplikasi flashing kernel yang dapat digunakan untuk ini:

1. [Kernel Flasher](https://github.com/capntrips/KernelFlasher/releases)
2. [Franco Kernel Manager](https://play.google.com/store/apps/details?id=com.franco.kernel)
3. [Ex Kernel Manager](https://play.google.com/store/apps/details?id=flar2.exkernelmanager)

Catatan: Metode ini lebih nyaman saat mengupgrade KernelSU dan dapat dilakukan tanpa komputer (buat cadangan terlebih dahulu).

## Patch boot.img secara manual {#patch-boot-image}

Untuk beberapa perangkat, format boot.img tidak seperti `lz4`, `gz`, dan `uncompressed` yang umum. Contoh khas adalah Pixel, di mana boot.img dikompresi dalam format `lz4_legacy`, sedangkan ramdisk mungkin dalam `gz` atau juga dikompresi dalam `lz4_legacy`. Saat ini, jika Anda langsung mem-flash boot.img yang disediakan oleh KernelSU, perangkat mungkin tidak dapat boot. Dalam hal ini, Anda dapat mem-patch boot.img secara manual untuk mencapai ini.

Selalu disarankan untuk menggunakan `magiskboot` untuk mem-patch image, ada dua cara:

1. [magiskboot](https://github.com/topjohnwu/Magisk/releases)
2. [magiskboot_build](https://github.com/ookiineko/magiskboot_build/releases/tag/last-ci)

Build resmi `magiskboot` hanya dapat berjalan di perangkat Android, jika Anda ingin menjalankannya di PC, Anda dapat mencoba opsi kedua.

::: tip
Android-Image-Kitchen tidak direkomendasikan untuk saat ini karena tidak menangani metadata boot (seperti tingkat patch keamanan) dengan benar. Oleh karena itu, mungkin tidak berfungsi pada beberapa perangkat.
:::

### Persiapan

1. Dapatkan boot.img stok perangkat Anda. Anda bisa mendapatkannya dari produsen perangkat Anda. Anda mungkin memerlukan [payload-dumper-go](https://github.com/ssut/payload-dumper-go).
2. Unduh file ZIP AnyKernel3 yang disediakan oleh KernelSU yang cocok dengan versi KMI perangkat Anda. Anda dapat merujuk ke [Instal dengan Recovery kustom](#install-with-custom-recovery).
3. Unpack paket AnyKernel3 dan dapatkan file `Image`, yang merupakan file kernel KernelSU.

### Menggunakan magiskboot di perangkat Android {#using-magiskboot-on-Android-devices}

1. Unduh Magisk terbaru dari [GitHub Releases](https://github.com/topjohnwu/Magisk/releases).
2. Ubah nama `Magisk-*(version).apk` menjadi `Magisk-*.zip` dan unzip.
3. Push `Magisk-*/lib/arm64-v8a/libmagiskboot.so` ke perangkat Anda melalui ADB: `adb push Magisk-*/lib/arm64-v8a/libmagiskboot.so /data/local/tmp/magiskboot`
4. Push stok boot.img dan Image di AnyKernel3 ke perangkat Anda.
5. Masuk ke shell ADB dan jalankan direktori `cd /data/local/tmp/`, lalu `chmod +x magiskboot`
6. Masuk ke shell ADB dan jalankan direktori `cd /data/local/tmp/`, jalankan `./magiskboot unpack boot.img` untuk unpack `boot.img`, Anda akan mendapatkan file `kernel`, ini adalah kernel stok Anda.
7. Ganti `kernel` dengan `Image` dengan menjalankan perintah: `mv -f Image kernel`.
8. Jalankan `./magiskboot repack boot.img` untuk repack image boot, dan Anda akan mendapatkan file `new-boot.img`, flash file ini ke perangkat melalui fastboot.

### Menggunakan magiskboot di Windows/macOS/Linux PC {#using-magiskboot-on-PC}

1. Unduh binary `magiskboot` yang sesuai untuk OS Anda dari [magiskboot_build](https://github.com/ookiineko/magiskboot_build/releases/tag/last-ci).
2. Siapkan stok `boot.img` dan `Image` di PC Anda.
3. Jalankan `chmod +x magiskboot`.
4. Masuk ke direktori yang sesuai, jalankan `./magiskboot unpack boot.img` untuk unpack `boot.img`, Anda akan mendapatkan file `kernel`, ini adalah kernel stok Anda.
5. Ganti `kernel` dengan `Image` dengan menjalankan perintah: `mv -f Image kernel`.
6. Jalankan `./magiskboot repack boot.img` untuk repack image boot, dan Anda akan mendapatkan file `new-boot.img`, flash file ini ke perangkat melalui fastboot.

::: info
`magiskboot` resmi dapat berjalan di lingkungan `Linux` secara normal, jika Anda pengguna Linux, Anda dapat menggunakan build resmi.
:::

## Instal dengan Recovery kustom {#install-with-custom-recovery}

Prasyarat: Perangkat Anda harus memiliki Recovery kustom, seperti TWRP. Jika tidak ada Recovery kustom yang tersedia untuk perangkat Anda, gunakan metode lain.

Langkah-langkah:

1. Di [GitHub Releases](https://github.com/tiann/KernelSU/releases), unduh paket ZIP yang dimulai dengan `AnyKernel3` yang cocok dengan versi perangkat Anda. Misalnya, jika versi kernel perangkat adalah `android12-5.10.66`, maka Anda harus mengunduh file `AnyKernel3-android12-5.10.66_yyyy-MM.zip` (di mana `yyyy` adalah tahun dan `MM` adalah bulan).
2. Reboot perangkat ke TWRP.
3. Gunakan ADB untuk menempatkan AnyKernel3-*.zip ke lokasi `/sdcard` perangkat dan pilih untuk menginstalnya di GUI TWRP, atau Anda dapat langsung menjalankan `adb sideload AnyKernel-*.zip` untuk menginstal.

Catatan: Metode ini cocok untuk instalasi apa pun (tidak terbatas pada instalasi awal atau upgrade selanjutnya), selama Anda menggunakan TWRP.

## Metode lain

Faktanya, semua metode instalasi ini hanya memiliki satu ide utama, yaitu **mengganti kernel asli dengan yang disediakan oleh KernelSU**, selama ini dapat dicapai, dapat diinstal. Berikut adalah metode lain yang mungkin:

1. Pertama, instal Magisk, dapatkan hak root melalui Magisk, lalu gunakan Kernel Flasher untuk mem-flash AnyKernel3 ZIP dari KernelSU.
2. Gunakan toolkit flashing apa pun di PC untuk mem-flash kernel yang disediakan oleh KernelSU.

Namun, jika tidak berhasil, coba pendekatan `magiskboot`.

## Pasca-Instalasi: Dukungan Modul

::: warning METAMODULE UNTUK MODIFIKASI FILE SISTEM
Jika Anda ingin menggunakan modul yang memodifikasi file `/system`, Anda perlu menginstal **metamodule** setelah menginstal KernelSU. Modul yang hanya menggunakan skrip, sepolicy, atau system.prop bekerja tanpa metamodule.
:::

**Untuk dukungan modifikasi `/system`**, silakan lihat [Panduan Metamodule](metamodule.md) untuk:
- Memahami apa itu metamodule dan mengapa diperlukan
- Menginstal metamodule `meta-overlayfs` resmi
- Pelajari tentang opsi metamodule lainnya
