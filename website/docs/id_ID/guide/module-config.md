# Konfigurasi Modul

KernelSU menyediakan sistem konfigurasi bawaan yang memungkinkan modul menyimpan pengaturan key-value persisten atau sementara. Konfigurasi disimpan dalam format biner di `/data/adb/ksu/module_configs/<module_id>/` dengan karakteristik berikut:

## Tipe Konfigurasi

- **Konfigurasi Persisten** (`persist.config`): Bertahan setelah reboot hingga dihapus secara eksplisit atau modul di-uninstall
- **Konfigurasi Sementara** (`tmp.config`): Otomatis dihapus selama tahap post-fs-data pada setiap boot

Saat membaca konfigurasi, nilai sementara lebih diprioritaskan daripada nilai persisten untuk key yang sama.

## Menggunakan Konfigurasi dalam Skrip Modul

Semua skrip modul (`post-fs-data.sh`, `service.sh`, `boot-completed.sh`, dll.) berjalan dengan variabel lingkungan `KSU_MODULE` diatur ke ID modul. Anda dapat menggunakan perintah `ksud module config` untuk mengelola konfigurasi modul Anda:

```bash
# Mendapatkan nilai konfigurasi
value=$(ksud module config get my_setting)

# Mengatur nilai konfigurasi persisten
ksud module config set my_setting "some value"

# Mengatur nilai konfigurasi sementara (dihapus setelah reboot)
ksud module config set --temp runtime_state "active"

# Mengatur nilai dari stdin (berguna untuk teks multiline atau data kompleks)
ksud module config set my_key <<EOF
teks multiline
nilai
EOF

# Atau alirkan dari perintah
echo "value" | ksud module config set my_key

# Bendera stdin eksplisit
cat file.json | ksud module config set json_data --stdin

# Daftar semua entri konfigurasi (gabungan persisten dan sementara)
ksud module config list

# Menghapus entri konfigurasi
ksud module config delete my_setting

# Menghapus entri konfigurasi sementara
ksud module config delete --temp runtime_state

# Menghapus semua konfigurasi persisten
ksud module config clear

# Menghapus semua konfigurasi sementara
ksud module config clear --temp
```

## Batasan Validasi

Sistem konfigurasi memberlakukan batasan berikut:

- **Panjang key maksimum**: 256 byte
- **Panjang nilai maksimum**: 1MB (1048576 byte)
- **Jumlah entri konfigurasi maksimum**: 32 per modul
- **Format key**: Harus cocok dengan `^[a-zA-Z][a-zA-Z0-9._-]+$` (seperti ID modul)
  - Harus dimulai dengan huruf
  - Dapat berisi huruf, angka, titik, garis bawah, atau tanda hubung
  - Panjang minimum: 2 karakter
- **Format nilai**: Tanpa batasan - dapat berisi karakter UTF-8 apa pun, termasuk jeda baris dan karakter kontrol
  - Disimpan dalam format biner dengan awalan panjang untuk penanganan data yang aman

## Siklus Hidup

- **Saat boot**: Semua konfigurasi sementara dihapus selama tahap post-fs-data
- **Saat uninstall modul**: Semua konfigurasi (persisten dan sementara) otomatis dihapus
- Konfigurasi disimpan dalam format biner dengan magic number `0x4b53554d` ("KSUM") dan validasi versi

## Kasus Penggunaan

Sistem konfigurasi ideal untuk:

- **Preferensi pengguna**: Menyimpan pengaturan modul yang dikonfigurasi pengguna melalui WebUI atau skrip action
- **Flag fitur**: Mengaktifkan/menonaktifkan fitur modul tanpa menginstal ulang
- **Status runtime**: Melacak status sementara yang harus direset saat reboot (gunakan konfigurasi sementara)
- **Pengaturan instalasi**: Mengingat pilihan yang dibuat saat instalasi modul
- **Data kompleks**: Menyimpan JSON, teks multiline, data terenkode Base64, atau konten terstruktur apa pun (hingga 1MB)

::: tip PRAKTIK TERBAIK
- Gunakan konfigurasi persisten untuk preferensi pengguna yang harus bertahan setelah reboot
- Gunakan konfigurasi sementara untuk status runtime atau flag fitur yang harus direset saat boot
- Validasi nilai konfigurasi dalam skrip Anda sebelum menggunakannya
- Gunakan perintah `ksud module config list` untuk men-debug masalah konfigurasi
:::

## Fitur Lanjutan

Sistem konfigurasi modul menyediakan kunci konfigurasi khusus untuk kasus penggunaan lanjutan:

### Mengganti Deskripsi Modul

Anda dapat mengganti field `description` dari `module.prop` secara dinamis dengan mengatur kunci konfigurasi `override.description`:

```bash
# Mengganti deskripsi modul
ksud module config set override.description "Deskripsi kustom yang ditampilkan di pengelola"
```

Saat mengambil daftar modul, jika konfigurasi `override.description` ada, itu akan menggantikan deskripsi asli dari `module.prop`. Ini berguna untuk:
- Menampilkan informasi status dinamis dalam deskripsi modul
- Menunjukkan detail konfigurasi runtime kepada pengguna
- Memperbarui deskripsi berdasarkan status modul tanpa menginstal ulang

### Mendeklarasikan Fitur yang Dikelola

Modul dapat mendeklarasikan fitur KernelSU mana yang mereka kelola menggunakan pola konfigurasi `manage.<feature>`. Fitur yang didukung sesuai dengan enum internal `FeatureId` KernelSU:

**Fitur yang Didukung:**
- `su_compat` - Mode kompatibilitas SU
- `kernel_umount` - Unmount otomatis kernel
- `enhanced_security` - Mode keamanan yang ditingkatkan

```bash
# Mendeklarasikan bahwa modul ini mengelola kompatibilitas SU dan mengaktifkannya
ksud module config set manage.su_compat true

# Mendeklarasikan bahwa modul ini mengelola unmount kernel dan menonaktifkannya
ksud module config set manage.kernel_umount false

# Menghapus pengelolaan fitur (modul tidak lagi mengontrol fitur ini)
ksud module config delete manage.su_compat
```

**Cara kerjanya:**
- Keberadaan kunci `manage.<feature>` menunjukkan bahwa modul mengelola fitur tersebut
- Nilai menunjukkan status yang diinginkan: `true`/`1` untuk diaktifkan, `false`/`0` (atau nilai lainnya) untuk dinonaktifkan
- Untuk berhenti mengelola fitur, hapus kunci konfigurasi sepenuhnya

Fitur yang dikelola diekspos melalui API daftar modul sebagai field `managedFeatures` (string yang dipisahkan koma). Ini memungkinkan:
- Pengelola KernelSU mendeteksi modul mana yang mengelola fitur KernelSU mana
- Pencegahan konflik ketika beberapa modul mencoba mengelola fitur yang sama
- Koordinasi yang lebih baik antara modul dan fungsionalitas inti KernelSU

::: warning HANYA FITUR YANG DIDUKUNG
Gunakan hanya nama fitur yang telah ditentukan sebelumnya yang tercantum di atas (`su_compat`, `kernel_umount`, `enhanced_security`). Ini sesuai dengan fitur internal KernelSU yang sebenarnya. Menggunakan nama fitur lain tidak akan menyebabkan error, tetapi tidak memiliki tujuan fungsional.
:::
