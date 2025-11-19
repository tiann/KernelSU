# Module WebUI

Selain menjalankan skrip boot dan memodifikasi berkas sistem, modul KernelSU dapat menampilkan antarmuka dan berinteraksi langsung dengan pengguna.

Modul dapat mendefinisikan halaman HTML + CSS + JavaScript dengan teknologi web apa pun. Manajer KernelSU menampilkan halaman tersebut lewat WebView dan menyediakan API untuk berinteraksi dengan sistem, misalnya menjalankan perintah shell.

## Direktori `webroot`

Berkas sumber daya web harus ditempatkan di subdirektori `webroot` di direktori root modul, dan **HARUS** ada berkas bernama `index.html` sebagai pintu masuk halaman modul. Struktur modul paling sederhana yang mempunyai antarmuka web adalah sebagai berikut:

```txt
❯ tree .
.
|-- module.prop
`-- webroot
    `-- index.html
```

::: warning
Saat modul dipasang, KernelSU otomatis mengatur izin serta konteks SELinux untuk direktori ini. Jika Anda tidak paham benar apa yang dilakukan, jangan mengubah izin direktori ini sendiri!
:::

Jika halaman Anda memiliki CSS atau JavaScript, letakkan juga di direktori ini.

## JavaScript API

Jika hanya berupa halaman tampilan, ia akan berfungsi layaknya halaman web biasa. Namun yang paling penting, KernelSU menyediakan serangkaian API sistem sehingga modul bisa mewujudkan fungsi khususnya.

KernelSU menyediakan pustaka JavaScript yang dirilis di [npm](https://www.npmjs.com/package/kernelsu) dan bisa dipakai pada kode JavaScript halaman Anda.

Sebagai contoh, Anda bisa menjalankan perintah shell untuk memperoleh konfigurasi tertentu atau mengubah suatu properti:

```JavaScript
import { exec } from 'kernelsu';

const { errno, stdout } = exec("getprop ro.product.model");
```

Anda juga dapat membuat halaman menjadi layar penuh atau menampilkan toast.

[Dokumentasi API](https://www.npmjs.com/package/kernelsu)

Jika API yang ada belum memenuhi kebutuhan atau kurang nyaman digunakan, silakan beri kami masukan [di sini](https://github.com/tiann/KernelSU/issues)!

## Beberapa tips

1. Anda dapat menggunakan `localStorage` seperti biasa untuk menyimpan data, namun ingat bahwa data akan hilang jika aplikasi manager dihapus. Jika butuh penyimpanan permanen, simpan data secara manual pada direktori tertentu.
2. Untuk halaman sederhana kami menyarankan menggunakan [parceljs](https://parceljs.org/) untuk proses bundling. Ia tidak butuh konfigurasi awal dan sangat mudah dipakai. Namun jika Anda ahli front-end atau punya preferensi sendiri, silakan gunakan alat pilihan Anda!
