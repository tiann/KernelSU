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

Jika Anda telah mem-flash modul yang telah terbukti aman tetapi menyebabkan perangkat Anda gagal booting, maka situasi ini dapat dipulihkan dengan mudah di KernelSU tanpa rasa khawatir. KernelSU memiliki mekanisme bawaan untuk menyelamatkan perangkat Anda, termasuk yang berikut:

1. Pembaruan AB
2. Selamatkan dengan menekan Volume Turun

#### Pembaruan AB

Pembaruan modul KernelSU menarik inspirasi dari mekanisme pembaruan AB sistem Android yang digunakan dalam pembaruan OTA. Jika Anda menginstal modul baru atau memperbarui modul yang sudah ada, itu tidak akan langsung mengubah file modul yang sedang digunakan. Sebagai gantinya, semua modul akan dibangun ke gambar pembaruan lainnya. Setelah sistem dimulai ulang, sistem akan mencoba untuk mulai menggunakan gambar pembaruan ini. Jika sistem Android berhasil melakukan booting, modul akan benar-benar diperbarui.

Oleh karena itu, metode paling sederhana dan paling umum digunakan untuk menyelamatkan perangkat Anda adalah dengan **memaksa reboot**. Jika Anda tidak dapat memulai sistem Anda setelah mem-flash modul, Anda dapat menekan dan menahan tombol daya selama lebih dari 10 detik, dan sistem akan melakukan reboot secara otomatis; setelah mem-boot ulang, itu akan kembali ke keadaan sebelum memperbarui modul, dan modul yang diperbarui sebelumnya akan dinonaktifkan secara otomatis.

#### Recovery dengan menekan Volume Bawah

Jika pembaruan AB masih tidak dapat menyelesaikan masalah, Anda dapat mencoba menggunakan **Safe Mode**. Dalam Safe Mode, semua modul dinonaktifkan.

Ada dua cara untuk masuk ke Safe Mode:

1. Mode Aman bawaan dari beberapa sistem; beberapa sistem memiliki Safe Mode bawaan yang dapat diakses dengan menekan lama tombol volume turun, sementara yang lain (seperti MIUI) dapat mengaktifkan Safe Mode di Recovery. Saat memasuki Safe Mode sistem, KernelSU juga akan masuk ke Safe Mode dan secara otomatis menonaktifkan modul.
2. Safe Mode bawaan dari KernelSU; metode pengoperasiannya adalah **tekan tombol volume turun secara terus-menerus selama lebih dari tiga kali** setelah layar boot pertama. Perhatikan bahwa ini adalah rilis pers, rilis pers, rilis pers, bukan tekan dan tahan.

Setelah memasuki mode aman, semua modul pada halaman modul KernelSU Manager dinonaktifkan, tetapi Anda dapat melakukan operasi "uninstall" untuk menghapus semua modul yang mungkin menyebabkan masalah.

Mode aman bawaan diimplementasikan di kernel, jadi tidak ada kemungkinan peristiwa penting yang hilang karena intersepsi. Namun, untuk kernel non-GKI, integrasi kode secara manual mungkin diperlukan, dan Anda dapat merujuk ke dokumentasi resmi untuk mendapatkan panduan.

### Module berbahaya

Jika metode di atas tidak dapat menyelamatkan perangkat Anda, kemungkinan besar modul yang Anda instal memiliki operasi jahat atau telah merusak perangkat Anda melalui cara lain. Dalam hal ini, hanya ada dua saran:

1. Hapus data dan flash sistem resmi.
2. Konsultasikan layanan purna jual.
