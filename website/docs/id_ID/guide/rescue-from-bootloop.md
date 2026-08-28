# Recovery dari bootloop

Saat mem-flash perangkat, kami mungkin menghadapi situasi di mana perangkat menjadi "bata". Secara teori, jika Anda hanya menggunakan fastboot untuk mem-flash partisi boot atau menginstal modul yang tidak sesuai yang menyebabkan perangkat gagal melakukan booting, ini dapat dipulihkan dengan operasi yang sesuai. Dokumen ini bertujuan untuk memberikan beberapa metode darurat untuk membantu Anda pulih dari perangkat "bricked".

## Brick saat memflash partisi boot

Di KernelSU, situasi berikut dapat menyebabkan bata boot saat mem-flash partisi boot:

1. Anda mem-flash image boot dalam format yang salah. Misalnya, jika format booting ponsel Anda adalah `gz`, tetapi Anda mem-flash image berformat `lz4`, maka ponsel tidak akan dapat melakukan booting.
2. Ponsel Anda perlu menonaktifkan verifikasi AVB agar dapat boot dengan benar (biasanya perlu menghapus semua data di ponsel).
3. Kernel Anda memiliki beberapa bug atau tidak cocok untuk flash ponsel Anda.

Apa pun situasinya, Anda dapat memulihkannya dengan **mem-flash gambar boot stok**. Oleh karena itu, di awal tutorial instalasi, kami sangat menyarankan Anda untuk mem-backup stock boot Anda sebelum melakukan flashing. Jika Anda belum mencadangkan, Anda dapat memperoleh boot pabrik asli dari pengguna lain dengan perangkat yang sama dengan Anda atau dari firmware resmi.

## Brick disebabkan modul

Memasang modul dapat menjadi penyebab yang lebih umum dari bricking perangkat Anda, tetapi kami harus memperingatkan Anda dengan serius: **Jangan memasang modul dari sumber yang tidak dikenal**! Karena modul memiliki hak akses root, mereka berpotensi menyebabkan kerusakan permanen pada perangkat Anda!

### Module normal

Jika Anda telah mem-flash modul yang telah terbukti aman tetapi menyebabkan perangkat Anda gagal booting, maka situasi ini dapat dipulihkan dengan mudah di KernelSU tanpa rasa khawatir. KernelSU memiliki mekanisme bawaan untuk menyelamatkan perangkat Anda:

#### Recovery dengan menekan Volume Bawah {#volume-down}

Anda dapat mencoba menggunakan **Safe Mode** untuk menyelamatkan perangkat Anda. Setelah masuk ke Safe Mode, semua modul dinonaktifkan.

Ada dua cara untuk masuk ke Safe Mode:

1. Mode Aman bawaan dari beberapa sistem; beberapa sistem memiliki Safe Mode bawaan yang dapat diakses dengan menekan lama tombol volume turun, sementara yang lain (seperti MIUI/HyperOS) dapat mengaktifkan Safe Mode di Recovery. Saat memasuki Safe Mode sistem, KernelSU juga akan masuk ke Safe Mode dan secara otomatis menonaktifkan modul.
2. Safe Mode bawaan dari KernelSU; metode pengoperasiannya adalah **tekan tombol volume turun secara terus-menerus selama lebih dari tiga kali** setelah layar boot pertama. Perhatikan bahwa ini adalah tekan-lepas, tekan-lepas, tekan-lepas, bukan tekan dan tahan.

Setelah memasuki mode aman, semua modul pada halaman modul KernelSU Manager dinonaktifkan, tetapi Anda dapat melakukan operasi "uninstall" untuk menghapus semua modul yang mungkin menyebabkan masalah.

Mode aman bawaan diimplementasikan di kernel, jadi tidak ada kemungkinan peristiwa penting yang hilang karena intersepsi. Namun, untuk kernel non-GKI, integrasi kode secara manual mungkin diperlukan, dan Anda dapat merujuk ke dokumentasi resmi untuk mendapatkan panduan.

::: warning
KernelSU mendaftarkan pendengar tombol volume selama inisialisasi modul kernel (dimuat saat kernel menjalankan proses init dalam mode LKM), dan membatalkan pendaftarannya pada tahap `on_post_fs_data` (sebelum animasi boot). Anda perlu memahami waktunya dan dengan cepat menekan tombol volume turun tiga kali setelah layar boot pertama. Jika perangkat boot dengan cepat atau operasinya tidak tepat waktu, safe mode mungkin tidak terpicu.

Jika modul menulis kode yang tidak masuk akal di initrc yang menyebabkan perangkat gagal boot, kode-kode ini masih akan dijalankan bahkan dalam safe mode.
:::

#### Penyelamatan Manual {#manual-rescue}

Saat safe mode tidak dapat menyelesaikan masalah, Anda dapat mencoba penyelamatan manual. Pilih metode berikut berdasarkan status perangkat.

**Metode 1: Gunakan ksud untuk mengelola modul melalui ADB**

Jika perangkat bisa mendapatkan root shell via ADB, Anda bisa menggunakan baris perintah `ksud` langsung untuk menonaktifkan atau menghapus instalan modul yang bermasalah:

::: tip
Setelah memasang partisi `metadata` dan `data`, Anda bisa menjalankan perintah `/data/adb/ksud` di bawah mode Recovery untuk mengelola modul.

Karena perangkat GKI berbagi `init`, modul kernel KernelSU masih akan dimuat di bawah mode Recovery, Anda seharusnya bisa menggunakan sebagian besar fitur `ksud` (seperti mengatur fitur) secara normal.
:::

```
adb shell
su
ksud module list          # Daftar semua modul
ksud module disable <id>  # Nonaktifkan modul yang bermasalah
ksud module uninstall <id> # Atau langsung uninstall
reboot
```

**Metode 2: Bersihkan manual melalui Recovery**

Jika Anda tidak dapat masuk ke sistem (bahkan ADB tidak dapat dihubungkan), Anda membutuhkan Recovery pihak ketiga (seperti TWRP) di perangkat.

Pemuatan modul KernelSU bergantung pada file injeksi init.rc di sisi kernel dan proses ksud di user space. Setelah menghapus file-file ini dan melakukan reboot, KernelSU tidak akan memuat modul apa pun.

**Langkah-langkah operasi:**

1. Masuk ke Recovery (seperti TWRP).
2. Pasang partisi data:
   ```
   mount /data
   ```
   (Anda mungkin perlu mendekripsi partisi data terlebih dahulu. Operasi spesifik tergantung pada perangkat dan metode dekripsi.)
3. Hapus ksud untuk mencegah pemuatan modul:
   ```
   rm -f /data/adb/ksud
   ```
4. (Opsional) Pasang partisi metadata dan hapus file injeksi init.rc yang dihasilkan oleh modul:
   ```
   mount /metadata
   rm -f /metadata/ksu/modules.rc
   rm -f /metadata/watchdog/ksu/modules.rc
   ```
5. Reboot perangkat:
   ```
   reboot
   ```

KernelSU akan melewati pemuatan semua modul setelah reboot. Setelah masuk ke sistem, Anda bisa membuka kembali KernelSU manager untuk menangani masalah modul.

### Format data atau modul berbahaya lainnya

Jika metode di atas tidak dapat menyelamatkan perangkat Anda, kemungkinan besar modul yang Anda instal memiliki operasi jahat atau telah merusak perangkat Anda melalui cara lain. Dalam hal ini, hanya ada dua saran:

1. Hapus data dan flash sistem resmi sepenuhnya.
2. Konsultasikan layanan purna jual.
