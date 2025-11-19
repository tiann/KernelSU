# App Profile

App Profile adalah mekanisme yang disediakan KernelSU untuk menyesuaikan konfigurasi beragam aplikasi.

Untuk aplikasi yang diberi izin root (misalnya dapat memakai `su`), App Profile juga dapat disebut Root Profile. Ia memungkinkan kita menyesuaikan `uid`, `gid`, `groups`, `capabilities`, dan aturan `SELinux` dari perintah `su`, sehingga hak istimewa pengguna root dapat dibatasi. Contohnya, hanya memberikan izin jaringan kepada aplikasi firewall sambil menolak izin akses berkas, atau memberikan izin shell alih-alih akses root penuh untuk aplikasi pembeku: **menjaga kekuatan agar tetap terkungkung dalam prinsip least privilege.**

Untuk aplikasi biasa tanpa izin root, App Profile dapat mengendalikan bagaimana kernel dan sistem modul bersikap terhadap aplikasi tersebut. Misalnya, App Profile bisa menentukan apakah perubahan yang disebabkan oleh modul harus tetap terlihat. Kernel dan sistem modul kemudian dapat membuat keputusan berdasarkan konfigurasi ini, seperti melakukan operasi serupa “menyembunyikan”.

## Root Profile

### UID, GID, dan Groups

Di sistem Linux ada dua konsep: pengguna dan grup. Setiap pengguna memiliki user ID (UID), dan seorang pengguna bisa menjadi anggota banyak grup yang masing-masing memiliki group ID (GID). ID inilah yang dipakai untuk mengenali pengguna di sistem dan menentukan sumber daya apa yang boleh mereka akses.

Pengguna dengan UID 0 disebut pengguna root, dan grup dengan GID 0 disebut grup root. Grup root biasanya memiliki hak istimewa tertinggi dalam sistem.

Dalam sistem Android, tiap aplikasi bertindak sebagai pengguna terpisah (kecuali pada kasus shared UID) dengan UID unik. Misalnya `0` adalah root, `1000` adalah `system`, `2000` adalah shell ADB, dan UID dari `10000` hingga `19999` mewakili aplikasi biasa.

::: info
UID yang dimaksud di sini berbeda dengan konsep multi-user atau work profile di Android. Work profile sebenarnya diimplementasikan dengan mempartisi rentang UID. Contohnya, 10000-19999 adalah pengguna utama, sementara 110000-119999 adalah work profile. Setiap aplikasi biasa di dalamnya tetap mempunyai UID unik.
:::

Setiap aplikasi dapat memiliki beberapa grup, dengan GID sebagai grup utama yang biasanya sama dengan UID. Grup lain disebut grup tambahan (supplementary groups). Sejumlah izin dikendalikan oleh keanggotaan grup, misalnya izin akses jaringan atau Bluetooth.

Sebagai contoh, apabila kita menjalankan perintah `id` di shell ADB, hasilnya mungkin seperti ini:

```sh
oriole:/ $ id
uid=2000(shell) gid=2000(shell) groups=2000(shell),1004(input),1007(log),1011(adb),1015(sdcard_rw),1028(sdcard_r),1078(ext_data_rw),1079(ext_obb_rw),3001(net_bt_admin),3002(net_bt),3003(inet),3006(net_bw_stats),3009(readproc),3011(uhid),3012(readtracefs) context=u:r:shell:s0
```

Di sini UID-nya `2000`, dan GID (ID grup utama) juga `2000`. Selain itu ia berada di beberapa grup tambahan seperti `inet` (boleh membuat soket `AF_INET` dan `AF_INET6`, artinya boleh mengakses jaringan) dan `sdcard_rw` (boleh baca/tulis kartu SD).

Root Profile milik KernelSU memungkinkan kita menyesuaikan UID, GID, dan grup untuk proses root setelah menjalankan `su`. Misalnya, Root Profile sebuah aplikasi root dapat mengatur UID menjadi `2000`, sehingga ketika menggunakan `su`, hak sebenarnya setara dengan shell ADB. Kita juga dapat menghapus grup `inet` agar perintah `su` tidak bisa mengakses jaringan.

::: tip CATATAN
App Profile hanya mengendalikan hak proses root setelah menggunakan `su`, bukan izin asli aplikasi. Jika aplikasi meminta izin akses jaringan, ia tetap dapat mengakses jaringan meski tidak menggunakan `su`. Menghapus grup `inet` dari `su` hanya membuat `su` tidak bisa mengakses jaringan.
:::

Root Profile dipaksakan oleh kernel dan tidak bergantung pada itikad aplikasi root, berbeda dengan mengganti pengguna atau grup secara manual melalui `su`. Pemberian izin `su` sepenuhnya berada di tangan pengguna, bukan pengembang.

### Capabilities

Capabilities adalah mekanisme pemisahan hak istimewa di Linux.

Untuk keperluan pemeriksaan izin, implementasi `UNIX` tradisional membedakan dua jenis proses: proses istimewa (effective UID `0`, disebut superuser atau root) dan proses biasa (effective UID bukan nol). Proses istimewa melewati semua pemeriksaan izin kernel, sedangkan proses biasa tunduk pada pemeriksaan penuh berdasarkan kredensial proses (biasanya effective UID, effective GID, dan daftar grup tambahan).

Sejak Linux 2.2, hak istimewa yang biasanya dimiliki superuser dipecah menjadi unit-unit terpisah yang disebut capability, dan masing-masing dapat diaktifkan atau dinonaktifkan secara independen.

Setiap capability mewakili satu atau lebih hak. Misalnya, `CAP_DAC_READ_SEARCH` mewakili kemampuan melewati pemeriksaan izin untuk membaca berkas serta izin baca dan eksekusi direktori. Jika seorang pengguna dengan effective UID `0` (root) tidak memiliki capability `CAP_DAC_READ_SEARCH` atau capability di atasnya, maka meskipun ia root ia tidak bisa sembarang membaca berkas.

Root Profile KernelSU memungkinkan kita menyesuaikan capability proses root setelah menjalankan `su`, sehingga yang diberikan hanyalah “hak root parsial”. Berbeda dengan UID dan GID di atas, beberapa aplikasi root memang membutuhkan UID `0` setelah memakai `su`. Pada kasus seperti ini, membatasi capability pengguna root dengan UID `0` dapat membatasi operasi yang boleh dilakukan.

::: tip SANGAT DISARANKAN
Dokumentasi resmi capability Linux [dapat dibaca di sini](https://man7.org/linux/man-pages/man7/capabilities.7.html) dan menjelaskan detail hak yang diwakili tiap capability. Jika Anda ingin menyesuaikan capability, sangat disarankan membaca dokumen ini terlebih dahulu.
:::

### SELinux

SELinux adalah mekanisme Mandatory Access Control (MAC) yang sangat kuat. Ia beroperasi berdasarkan prinsip **default deny**: tindakan apa pun yang tidak diizinkan secara eksplisit akan ditolak.

SELinux memiliki dua mode global:

1. Mode Permissive: penolakan hanya dicatat di log, tidak ditegakkan.
2. Mode Enforcing: penolakan dicatat dan ditegakkan.

::: warning
Android modern sangat bergantung pada SELinux untuk menjaga keamanan sistem secara keseluruhan. Sangat disarankan untuk tidak memakai sistem kustom yang berjalan dalam mode “Permissive”, karena hampir tidak menawarkan keuntungan dibanding sistem yang sepenuhnya terbuka.
:::

Menjelaskan SELinux secara tuntas sangatlah kompleks dan berada di luar cakupan dokumen ini. Disarankan mempelajari cara kerjanya melalui sumber berikut:

1. [Wikipedia](https://en.wikipedia.org/wiki/Security-Enhanced_Linux)
2. [Red Hat: What Is SELinux?](https://www.redhat.com/en/topics/linux/what-is-selinux)
3. [ArchLinux: SELinux](https://wiki.archlinux.org/title/SELinux)

Root Profile KernelSU memungkinkan kita menyesuaikan konteks SELinux dari proses root setelah menjalankan `su`. Kita bisa menetapkan aturan kontrol akses khusus untuk konteks ini sehingga hak root dapat diatur secara sangat granular.

Dalam skenario umum, ketika sebuah aplikasi menjalankan `su`, prosesnya berpindah ke domain SELinux dengan **akses tidak terbatas**, seperti `u:r:su:s0`. Melalui Root Profile, domain ini bisa diganti menjadi domain kustom seperti `u:r:app1:s0`, lalu serangkaian aturan dapat ditetapkan untuk domain tersebut:

```sh
type app1
enforce app1
typeattribute app1 mlstrustedsubject
allow app1 * * *
```

Perlu dicatat, aturan `allow app1 * * *` di atas hanya untuk contoh. Dalam praktiknya aturan ini tidak boleh digunakan secara luas karena hampir sama saja dengan mode Permissive.

### Eskalasi

Jika konfigurasi Root Profile tidak ditetapkan dengan benar, skenario eskalasi bisa terjadi. Pembatasan yang diberikan App Profile bisa gagal tanpa sengaja.

Sebagai contoh, apabila Anda memberikan izin root kepada pengguna shell ADB (kasus yang cukup umum) dan kemudian memberikan izin root kepada aplikasi biasa, namun mengonfigurasi Root Profile aplikasi tersebut dengan UID 2000 (UID milik shell ADB), aplikasi itu bisa mendapatkan akses root penuh dengan menjalankan perintah `su` dua kali:

1. Eksekusi `su` pertama akan tunduk pada App Profile dan mengubah UID menjadi `2000` (shell ADB) alih-alih `0` (root).
2. Eksekusi `su` kedua, karena UID-nya sudah `2000` dan UID `2000` (shell ADB) memang diberikan akses root pada konfigurasi, maka aplikasi itu akan memperoleh hak root penuh.

::: warning CATATAN
Perilaku ini sepenuhnya sesuai ekspektasi dan bukan bug. Karena itu kami menyarankan hal berikut:

Jika Anda benar-benar perlu memberikan izin root kepada ADB (misalnya sebagai pengembang), jangan mengubah UID menjadi `2000` saat mengonfigurasi Root Profile. Gunakan `1000` (system) agar lebih aman.
:::

## Non-root profile

### Umount modules

KernelSU menyediakan mekanisme systemless untuk memodifikasi partisi sistem dengan memasang OverlayFS. Namun beberapa aplikasi peka terhadap perilaku ini. Dalam kasus tersebut, kita dapat membongkar (umount) modul yang dimuat di aplikasi tertentu dengan mengaktifkan opsi “Umount modules”.

Selain itu, antarmuka pengaturan KernelSU Manager menyediakan opsi “Umount modules by default”. Secara bawaan opsi ini **aktif**, artinya KernelSU atau modul tertentu akan membongkar modul untuk aplikasi ini kecuali ada pengaturan tambahan. Jika Anda tidak menginginkan perilaku ini atau jika aplikasi tertentu terpengaruh, ada dua pendekatan:

1. Biarkan “Umount modules by default” tetap aktif dan nonaktifkan opsi “Umount modules” di App Profile untuk aplikasi yang memang perlu memuat modul (berperan sebagai daftar putih).
2. Nonaktifkan “Umount modules by default” lalu aktifkan opsi “Umount modules” di App Profile hanya untuk aplikasi yang harus dibongkar modulnya (berperan sebagai daftar hitam).

::: info
Pada perangkat dengan kernel versi 5.10 ke atas, kernel akan membongkar modul tanpa tindakan tambahan. Namun di perangkat dengan kernel di bawah 5.10, opsi ini hanya berupa konfigurasi dan KernelSU sendiri tidak mengambil tindakan apa pun. Jika ingin memakai opsi “Umount modules” pada kernel sebelum 5.10 Anda harus backport fungsi `path_umount` di `fs/namespace.c`. Anda bisa menemukan info lebih lanjut di bagian akhir halaman [Integrate for non-GKI devices](https://kernelsu.org/guide/how-to-integrate-for-non-gki.html#how-to-backport-path_umount). Beberapa modul seperti Zygisksu juga memakai opsi ini untuk menentukan apakah modul perlu dibongkar.
:::
