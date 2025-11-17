# Metamodul

Metamodul adalah fitur revolusioner di KernelSU yang mentransfer kemampuan sistem modul yang penting dari daemon inti ke modul yang dapat dipasang. Pergeseran arsitektur ini mempertahankan stabilitas dan keamanan KernelSU sambil melepaskan potensi inovasi yang lebih besar untuk ekosistem modul.

## Apa itu Metamodul?

Metamodul adalah jenis modul KernelSU khusus yang menyediakan fungsi infrastruktur inti untuk sistem modul. Tidak seperti modul biasa yang memodifikasi file sistem, metamodul mengontrol *bagaimana* modul biasa diinstal dan dipasang.

Metamodul adalah mekanisme ekstensi berbasis plugin yang memungkinkan kustomisasi lengkap infrastruktur manajemen modul KernelSU. Dengan mendelegasikan logika pemasangan dan instalasi ke metamodul, KernelSU menghindari menjadi titik deteksi yang rapuh sambil memungkinkan strategi implementasi yang beragam.

**Karakteristik utama:**

- **Peran infrastruktur**: Metamodul menyediakan layanan yang diandalkan modul biasa
- **Instans tunggal**: Hanya satu metamodul yang dapat diinstal pada satu waktu
- **Eksekusi prioritas**: Skrip metamodul berjalan sebelum skrip modul biasa
- **Hook khusus**: Menyediakan tiga skrip hook untuk instalasi, pemasangan, dan pembersihan

## Mengapa Metamodul?

Solusi root tradisional memasukkan logika pemasangan ke dalam inti mereka, membuat mereka lebih mudah dideteksi dan lebih sulit untuk berkembang. Arsitektur metamodul KernelSU memecahkan masalah ini melalui pemisahan perhatian.

**Keunggulan strategis:**

- **Mengurangi permukaan deteksi**: KernelSU sendiri tidak melakukan pemasangan, mengurangi vektor deteksi
- **Stabilitas**: Daemon inti tetap stabil sementara implementasi pemasangan dapat berkembang
- **Inovasi**: Komunitas dapat mengembangkan strategi pemasangan alternatif tanpa mem-fork KernelSU
- **Pilihan**: Pengguna dapat memilih implementasi yang paling sesuai dengan kebutuhan mereka

**Fleksibilitas pemasangan:**

- **Tanpa pemasangan**: Untuk pengguna dengan modul tanpa pemasangan saja, hindari overhead pemasangan sepenuhnya
- **Pemasangan OverlayFS**: Pendekatan tradisional dengan dukungan lapisan baca-tulis (melalui `meta-overlayfs`)
- **Magic mount**: Pemasangan kompatibel Magisk untuk kompatibilitas aplikasi yang lebih baik
- **Implementasi kustom**: Overlay berbasis FUSE, pemasangan VFS kustom, atau pendekatan yang sama sekali baru

**Melampaui pemasangan:**

- **Ekstensibilitas**: Tambahkan fitur seperti dukungan modul kernel tanpa memodifikasi inti KernelSU
- **Modularitas**: Perbarui implementasi secara independen dari rilis KernelSU
- **Kustomisasi**: Buat solusi khusus untuk perangkat atau kasus penggunaan tertentu

::: warning PENTING
Tanpa metamodul yang diinstal, modul **TIDAK** akan dipasang. Instalasi KernelSU yang baru memerlukan pemasangan metamodul (seperti `meta-overlayfs`) agar modul berfungsi.
:::

## Untuk Pengguna

### Menginstal Metamodul

Instal metamodul dengan cara yang sama seperti modul biasa:

1. Unduh file ZIP metamodul (misalnya, `meta-overlayfs.zip`)
2. Buka aplikasi KernelSU Manager
3. Ketuk tombol tindakan mengambang (➕)
4. Pilih file ZIP metamodul
5. Reboot perangkat Anda

Metamodul `meta-overlayfs` adalah implementasi referensi resmi yang menyediakan pemasangan modul berbasis overlayfs tradisional dengan dukungan image ext4.

### Memeriksa Metamodul Aktif

Anda dapat memeriksa metamodul mana yang saat ini aktif di halaman Modul aplikasi KernelSU Manager. Metamodul aktif akan ditampilkan di daftar modul Anda dengan penunjukan khususnya.

### Menghapus Instalasi Metamodul

::: danger PERINGATAN
Menghapus instalasi metamodul akan memengaruhi **SEMUA** modul. Setelah dihapus, modul tidak akan lagi dipasang hingga Anda menginstal metamodul lain.
:::

Untuk menghapus instalasi:

1. Buka KernelSU Manager
2. Temukan metamodul di daftar modul Anda
3. Ketuk hapus instalasi (Anda akan melihat peringatan khusus)
4. Konfirmasi tindakan
5. Reboot perangkat Anda

Setelah menghapus instalasi, Anda harus menginstal metamodul lain jika Anda ingin modul terus berfungsi.

### Batasan Metamodul Tunggal

Hanya satu metamodul yang dapat diinstal pada satu waktu. Jika Anda mencoba menginstal metamodul kedua, KernelSU akan mencegah instalasi untuk menghindari konflik.

Untuk beralih metamodul:

1. Hapus instalasi semua modul biasa
2. Hapus instalasi metamodul saat ini
3. Reboot
4. Instal metamodul baru
5. Instal ulang modul biasa Anda
6. Reboot lagi

## Untuk Pengembang Modul

Jika Anda mengembangkan modul KernelSU biasa, Anda tidak perlu terlalu khawatir tentang metamodul. Modul Anda akan berfungsi selama pengguna memiliki metamodul yang kompatibel (seperti `meta-overlayfs`) yang diinstal.

**Yang perlu Anda ketahui:**

- **Pemasangan memerlukan metamodul**: Direktori `system` di modul Anda hanya akan dipasang jika pengguna memiliki metamodul yang diinstal yang menyediakan fungsi pemasangan
- **Tidak perlu perubahan kode**: Modul yang ada terus berfungsi tanpa modifikasi

::: tip
Jika Anda terbiasa dengan pengembangan modul Magisk, modul Anda akan berfungsi dengan cara yang sama di KernelSU ketika metamodul diinstal, karena menyediakan pemasangan kompatibel Magisk.
:::

## Untuk Pengembang Metamodul

Membuat metamodul memungkinkan Anda untuk menyesuaikan bagaimana KernelSU menangani instalasi modul, pemasangan, dan penghapusan instalasi.

### Persyaratan Dasar

Metamodul diidentifikasi oleh properti khusus di `module.prop`:

```txt
id=my_metamodule
name=My Custom Metamodule
version=1.0
versionCode=1
author=Your Name
description=Custom module mounting implementation
metamodule=1
```

Properti `metamodule=1` (atau `metamodule=true`) menandai ini sebagai metamodul. Tanpa properti ini, modul akan diperlakukan sebagai modul biasa.

### Struktur File

Struktur metamodul:

```txt
my_metamodule/
├── module.prop              (harus menyertakan metamodule=1)
│
│      *** Hook khusus metamodul ***
├── metamount.sh             (opsional: handler mount kustom)
├── metainstall.sh           (opsional: hook instalasi untuk modul biasa)
├── metauninstall.sh         (opsional: hook pembersihan untuk modul biasa)
│
│      *** File modul standar (semua opsional) ***
├── customize.sh             (kustomisasi instalasi)
├── post-fs-data.sh          (skrip tahap post-fs-data)
├── service.sh               (skrip late_start service)
├── boot-completed.sh        (skrip boot selesai)
├── uninstall.sh             (skrip penghapusan instalasi metamodul sendiri)
├── system/                  (modifikasi systemless, jika diperlukan)
└── [file tambahan apa pun]
```

Metamodul dapat menggunakan semua fitur modul standar (skrip siklus hidup, dll.) selain hook metamodul khusus mereka.

### Skrip Hook

Metamodul dapat menyediakan hingga tiga skrip hook khusus:

#### 1. metamount.sh - Handler Mount

**Tujuan**: Mengontrol bagaimana modul dipasang selama boot.

**Kapan dieksekusi**: Selama tahap `post-fs-data`, sebelum skrip modul apa pun berjalan.

**Variabel lingkungan:**

- `MODDIR`: Path direktori metamodul (misalnya, `/data/adb/modules/my_metamodule`)
- Semua variabel lingkungan KernelSU standar

**Tanggung jawab:**

- Pasang semua modul yang diaktifkan secara systemless
- Periksa flag `skip_mount`
- Tangani persyaratan pemasangan khusus modul

::: danger PERSYARATAN KRITIS
Saat melakukan operasi mount, Anda **HARUS** mengatur nama sumber/perangkat ke `"KSU"`. Ini mengidentifikasi mount sebagai milik KernelSU.

**Contoh (benar):**

```sh
mount -t overlay -o lowerdir=/lower,upperdir=/upper,workdir=/work KSU /target
```

**Untuk API mount modern**, atur string sumber:

```rust
fsconfig_set_string(fs, "source", "KSU")?;
```

Ini penting agar KernelSU mengidentifikasi dan mengelola mount-nya dengan benar.
:::

**Contoh skrip:**

```sh
#!/system/bin/sh
MODDIR="${0%/*}"

# Contoh: Implementasi bind mount sederhana
for module in /data/adb/modules/*; do
    if [ -f "$module/disable" ] || [ -f "$module/skip_mount" ]; then
        continue
    fi

    if [ -d "$module/system" ]; then
        # Mount dengan source=KSU (DIPERLUKAN!)
        mount -o bind,dev=KSU "$module/system" /system
    fi
done
```

#### 2. metainstall.sh - Hook Instalasi

**Tujuan**: Sesuaikan bagaimana modul biasa diinstal.

**Kapan dieksekusi**: Selama instalasi modul, setelah file diekstrak tetapi sebelum instalasi selesai. Skrip ini **di-source** (tidak dieksekusi) oleh installer bawaan, mirip dengan cara kerja `customize.sh`.

**Variabel lingkungan dan fungsi:**

Skrip ini mewarisi semua variabel dan fungsi dari `install.sh` bawaan:

- **Variabel**: `MODPATH`, `TMPDIR`, `ZIPFILE`, `ARCH`, `API`, `IS64BIT`, `KSU`, `KSU_VER`, `KSU_VER_CODE`, `BOOTMODE`, dll.
- **Fungsi**:
  - `ui_print <msg>` - Cetak pesan ke konsol
  - `abort <msg>` - Cetak error dan hentikan instalasi
  - `set_perm <target> <owner> <group> <permission> [context]` - Atur izin file
  - `set_perm_recursive <directory> <owner> <group> <dirpermission> <filepermission> [context]` - Atur izin secara rekursif
  - `install_module` - Panggil proses instalasi modul bawaan

**Kasus penggunaan:**

- Proses file modul sebelum atau sesudah instalasi bawaan (panggil `install_module` ketika siap)
- Pindahkan file modul
- Validasi kompatibilitas modul
- Siapkan struktur direktori khusus
- Inisialisasi sumber daya khusus modul

**Catatan**: Skrip ini **TIDAK** dipanggil saat menginstal metamodul itu sendiri.

#### 3. metauninstall.sh - Hook Pembersihan

**Tujuan**: Bersihkan sumber daya ketika modul biasa dihapus instalasi.

**Kapan dieksekusi**: Selama penghapusan instalasi modul, sebelum direktori modul dihapus.

**Variabel lingkungan:**

- `MODULE_ID`: ID modul yang sedang dihapus instalasi

**Kasus penggunaan:**

- Proses file
- Bersihkan symlink
- Bebaskan sumber daya yang dialokasikan
- Perbarui pelacakan internal

**Contoh skrip:**

```sh
#!/system/bin/sh
# Dipanggil saat menghapus instalasi modul biasa
MODULE_ID="$1"
IMG_MNT="/data/adb/metamodule/mnt"

# Hapus file modul dari image
if [ -d "$IMG_MNT/$MODULE_ID" ]; then
    rm -rf "$IMG_MNT/$MODULE_ID"
fi
```

### Urutan Eksekusi

Memahami urutan eksekusi boot sangat penting untuk pengembangan metamodul:

```txt
tahap post-fs-data:
  1. Skrip post-fs-data.d umum dieksekusi
  2. Prune modul, restorecon, muat sepolicy.rule
  3. post-fs-data.sh metamodul dieksekusi (jika ada)
  4. post-fs-data.sh modul biasa dieksekusi
  5. Muat system.prop
  6. metamount.sh metamodul dieksekusi
     └─> Pasang semua modul secara systemless
  7. Tahap post-mount.d berjalan
     - Skrip post-mount.d umum
     - post-mount.sh metamodul (jika ada)
     - post-mount.sh modul biasa

tahap service:
  1. Skrip service.d umum dieksekusi
  2. service.sh metamodul dieksekusi (jika ada)
  3. service.sh modul biasa dieksekusi

tahap boot-completed:
  1. Skrip boot-completed.d umum dieksekusi
  2. boot-completed.sh metamodul dieksekusi (jika ada)
  3. boot-completed.sh modul biasa dieksekusi
```

**Poin penting:**

- `metamount.sh` berjalan **SETELAH** semua skrip post-fs-data (baik metamodul maupun modul biasa)
- Skrip siklus hidup metamodul (`post-fs-data.sh`, `service.sh`, `boot-completed.sh`) selalu berjalan sebelum skrip modul biasa
- Skrip umum di direktori `.d` berjalan sebelum skrip metamodul
- Tahap `post-mount` berjalan setelah pemasangan selesai

### Mekanisme Symlink

Ketika metamodul diinstal, KernelSU membuat symlink:

```sh
/data/adb/metamodule -> /data/adb/modules/<metamodule_id>
```

Ini menyediakan path yang stabil untuk mengakses metamodul aktif, terlepas dari ID-nya.

**Manfaat:**

- Path akses yang konsisten
- Deteksi mudah metamodul aktif
- Menyederhanakan konfigurasi

### Contoh Dunia Nyata: meta-overlayfs

Metamodul `meta-overlayfs` adalah implementasi referensi resmi. Ini menunjukkan praktik terbaik untuk pengembangan metamodul.

#### Arsitektur

`meta-overlayfs` menggunakan **arsitektur dual-directory**:

1. **Direktori metadata**: `/data/adb/modules/`
   - Berisi `module.prop`, `disable`, penanda `skip_mount`
   - Cepat untuk dipindai selama boot
   - Jejak penyimpanan kecil

2. **Direktori konten**: `/data/adb/metamodule/mnt/`
   - Berisi file modul aktual (system, vendor, product, dll.)
   - Disimpan dalam image ext4 (`modules.img`)
   - Dioptimalkan ruang dengan fitur ext4

#### Implementasi metamount.sh

Berikut adalah cara `meta-overlayfs` mengimplementasikan handler mount:

```sh
#!/system/bin/sh
MODDIR="${0%/*}"
IMG_FILE="$MODDIR/modules.img"
MNT_DIR="$MODDIR/mnt"

# Pasang image ext4 jika belum dipasang
if ! mountpoint -q "$MNT_DIR"; then
    mkdir -p "$MNT_DIR"
    mount -t ext4 -o loop,rw,noatime "$IMG_FILE" "$MNT_DIR"
fi

# Atur variabel lingkungan untuk dukungan dual-directory
export MODULE_METADATA_DIR="/data/adb/modules"
export MODULE_CONTENT_DIR="$MNT_DIR"

# Eksekusi binary mount
# (Logika pemasangan aktual ada di binary Rust)
"$MODDIR/meta-overlayfs"
```

#### Fitur Utama

**Pemasangan Overlayfs:**

- Menggunakan overlayfs kernel untuk modifikasi systemless yang sebenarnya
- Mendukung beberapa partisi (system, vendor, product, system_ext, odm, oem)
- Dukungan lapisan baca-tulis melalui `/data/adb/modules/.rw/`

**Identifikasi sumber:**

```rust
// Dari meta-overlayfs/src/mount.rs
fsconfig_set_string(fs, "source", "KSU")?;  // DIPERLUKAN!
```

Ini mengatur `dev=KSU` untuk semua mount overlay, memungkinkan identifikasi yang tepat.

### Praktik Terbaik

Saat mengembangkan metamodul:

1. **Selalu atur sumber ke "KSU"** untuk operasi mount - umount kernel dan umount zygisksu memerlukan ini untuk umount dengan benar
2. **Tangani error dengan baik** - proses boot sensitif terhadap waktu
3. **Hormati flag standar** - dukung `skip_mount` dan `disable`
4. **Log operasi** - gunakan `echo` atau logging untuk debugging
5. **Tes secara menyeluruh** - error pemasangan dapat menyebabkan boot loop
6. **Dokumentasikan perilaku** - jelaskan dengan jelas apa yang dilakukan metamodul Anda
7. **Sediakan jalur migrasi** - bantu pengguna beralih dari solusi lain

### Menguji Metamodul Anda

Sebelum merilis:

1. **Uji instalasi** pada pengaturan KernelSU yang bersih
2. **Verifikasi pemasangan** dengan berbagai jenis modul
3. **Periksa kompatibilitas** dengan modul umum
4. **Uji penghapusan instalasi** dan pembersihan
5. **Validasi kinerja boot** (metamount.sh memblokir!)
6. **Pastikan penanganan error yang tepat** untuk menghindari boot loop

## Pertanyaan yang Sering Diajukan

### Apakah saya memerlukan metamodul?

**Untuk pengguna**: Hanya jika Anda ingin menggunakan modul yang memerlukan pemasangan. Jika Anda hanya menggunakan modul yang menjalankan skrip tanpa memodifikasi file sistem, Anda tidak memerlukan metamodul.

**Untuk pengembang modul**: Tidak, Anda mengembangkan modul secara normal. Pengguna memerlukan metamodul hanya jika modul Anda memerlukan pemasangan.

**Untuk pengguna lanjutan**: Hanya jika Anda ingin menyesuaikan perilaku pemasangan atau membuat implementasi pemasangan alternatif.

### Bisakah saya memiliki beberapa metamodul?

Tidak. Hanya satu metamodul yang dapat diinstal pada satu waktu. Ini mencegah konflik dan memastikan perilaku yang dapat diprediksi.

### Apa yang terjadi jika saya menghapus instalasi satu-satunya metamodul saya?

Modul tidak akan lagi dipasang. Perangkat Anda akan boot secara normal, tetapi modifikasi modul tidak akan diterapkan hingga Anda menginstal metamodul lain.

### Apakah meta-overlayfs diperlukan?

Tidak. Ini menyediakan pemasangan overlayfs standar yang kompatibel dengan sebagian besar modul. Anda dapat membuat metamodul Anda sendiri jika Anda memerlukan perilaku yang berbeda.

## Lihat Juga

- [Panduan Modul](module.md) - Pengembangan modul umum
- [Perbedaan dengan Magisk](difference-with-magisk.md) - Membandingkan KernelSU dan Magisk
- [Cara Membangun](how-to-build.md) - Membangun KernelSU dari sumber
