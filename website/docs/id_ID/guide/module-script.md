# Skrip sekali jalan

Skrip sekali jalan (one-shot) adalah ZIP yang **dijalankan sekali** oleh manager KernelSU alih-alih dipasang. Skrip ini dikemas seperti modul, tetapi tidak ada yang tertinggal di perangkat: manager mengekstrak ZIP ke direktori privat, meminta parameter yang dideklarasikannya, menampilkan kode sumber `action.sh`, lalu menjalankannya di shell root. Gunakan untuk tugas seperti membuka pengaturan tersembunyi atau mengekspor log; apa pun yang harus berjalan saat boot atau memodifikasi partisi sistem sebaiknya dibuat sebagai [modul](module.md).

## Struktur paket

```txt
<id>-<version>.zip
├── module.prop      # the usual module.prop fields + oneshot=true
├── action.sh        # required: the script the manager runs
├── params.json      # optional: parameters asked before running
├── customize.sh     # optional: only used if the ZIP is installed as a module
└── other files      # data the script reads via $MODPATH
```

`module.prop` menggunakan field yang sama seperti [modul](module.md#module-prop) dan menambahkan `oneshot`:

```txt
id=example_script
name=Example Script
version=v1
versionCode=1
author=Alice
description=Does a thing once
oneshot=true
```

`oneshot` dibaca dengan cara yang sama seperti `metamodule`: nilainya, tanpa spasi di sekitarnya, adalah `1` atau `true` (tidak peka huruf besar/kecil). Nilai lain, atau tidak adanya baris `oneshot`, berarti modul biasa.

Jangan sertakan file khusus modul seperti `post-fs-data.sh`, `service.sh`, `system.prop`, `sepolicy.rule` atau direktori `system/`: file-file tersebut tidak pernah digunakan oleh eksekusi sekali jalan. Skrip sekali jalan tidak dapat menjadi metamodule.

## Menjalankan skrip

Ada dua cara untuk menjalankan skrip sekali jalan:

- **Dari toko modul**: entri registri [KernelSU Modules](https://modules.kernelsu.org) dengan `kind: script` menampilkan lencana "Script" dan tombol **Run** alih-alih **Pasang**. Unduhan diperiksa terhadap ukuran dan SHA-256 dari registri, dan `module.prop` harus memuat `id` yang terdaftar.
- **Dari ZIP lokal**: jika Anda memilih satu ZIP dari penyimpanan (atau membukanya dari aplikasi lain) dan `module.prop`-nya memiliki `oneshot=true`, halaman eksekusi akan terbuka alih-alih penginstal. Halaman ini juga menawarkan **Install as module anyway**.

Sebelum menjalankan, manager memeriksa bahwa `module.prop` mendeklarasikan `oneshot` dengan `id` yang valid dan bahwa `action.sh` ada di root ZIP, lalu mengekstrak ZIP. Path absolut, `..`, symlink, dan file khusus ditolak, dan isi yang diekstrak dibatasi hingga 64 MiB dan 4096 entri.

Halaman eksekusi menampilkan formulir parameter, kode sumber `action.sh`, dan eksekusi terakhir di perangkat ini. Setelah konfirmasi, manager menjalankan skrip di shell root baru dalam namespace mount global, seperti tombol Action pada modul:

```sh
cd <dir> && <env> /data/adb/ksu/bin/busybox sh ./action.sh </dev/null
```

Output ditampilkan seperti output Action dan dapat disimpan. Direktori hasil ekstraksi dihapus setelahnya. Manager hanya mengingat eksekusi terakhir (versi dan kode keluar); tidak ada status terpasang dan tidak ada pemeriksaan pembaruan.

### Variabel lingkungan

| Variabel | Nilai |
| --- | --- |
| `ASH_STANDALONE` | `1` |
| `KSU` | `true` |
| `KSU_VER`, `KSU_VER_CODE`, `KSU_KERNEL_VER_CODE` | Versi KernelSU, sama seperti untuk modul |
| `KSU_LATE_LOAD` | `1` hanya dalam [mode late-load](module.md#late-load-mode) |
| `KSU_MODULE` | ID skrip |
| `KSU_ONESHOT` | `1`, untuk membedakan eksekusi sekali jalan dari tombol Action modul yang terpasang |
| `MODPATH` | direktori ekstraksi; baca file data Anda dari sini |
| `PATH` | `$PATH:/data/adb/ksu/bin` |
| `KSU_PARAM_<KEY>` | satu untuk setiap parameter, lihat [`params.json`](#params-json) |

Stdin adalah `/dev/null`, sehingga skrip tidak dapat membaca input secara interaktif; deklarasikan parameter sebagai gantinya.

### Kode keluar

| Kode keluar | Arti |
| --- | --- |
| `0` | Berhasil. |
| `2` | Tidak berlaku untuk perangkat ini; ditampilkan sebagai pemberitahuan, bukan sebagai kegagalan. |
| lainnya | Gagal. |

## `params.json` {#params-json}

`params.json` bersifat opsional. File ini berada di root ZIP, berupa JSON UTF-8, dan berukuran maksimal 64 KiB. Manager membuat formulir darinya dan meneruskan setiap nilai ke `action.sh` sebagai `KSU_PARAM_<KEY>`.

```json
{
  "schemaVersion": 1,
  "params": [
    {
      "key": "ENABLE_FASTBOOT",
      "type": "bool",
      "label": "Also enable fastboot",
      "description": "Optional longer help text",
      "default": true
    },
    { "key": "MODE", "type": "enum", "label": "Mode", "default": "a",
      "options": [ { "value": "a", "label": "A" }, { "value": "b" } ] },
    { "key": "NAME", "type": "string", "label": "Name", "default": "", "maxLength": 64,
      "required": false },
    { "key": "COUNT", "type": "int", "label": "Count", "default": 1, "min": 0, "max": 10 }
  ]
}
```

| Field | Aturan |
| --- | --- |
| `schemaVersion` | Harus `1`. |
| `params` | Hingga 16 parameter. |
| `key` | `^[A-Z][A-Z0-9_]{0,31}$`, unik. Nilainya diterima skrip sebagai `KSU_PARAM_<key>`. |
| `type` | `bool`, `enum`, `string` atau `int`. |
| `label` | Wajib, 1–200 karakter. Label ditampilkan sesuai yang ditulis (satu bahasa). |
| `description` | Opsional, aturan sama seperti `label`. |

Per tipe:

| Tipe | Field | Diteruskan sebagai |
| --- | --- | --- |
| `bool` | `default`: `true` / `false` (default `false`). | `true` / `false` |
| `enum` | `options`: 1–32 entri `{value, label?}`; `value` cocok dengan `^[A-Za-z0-9._-]{1,64}$` dan unik. `default` harus salah satu dari nilai tersebut (default: opsi pertama). | nilai opsi |
| `string` | `default` (default `""`), `maxLength` 1–1024 (default 256), `required` (default `false`; nilai kosong diperbolehkan kecuali wajib). Nilai tidak boleh berisi NUL atau baris baru. | sesuai yang dimasukkan |
| `int` | `min` / `max` dan `default` opsional (default `min`, atau `0` tanpa `min`); bilangan bulat bertanda 32-bit. | desimal |

`default` dari setiap parameter harus memenuhi batasannya sendiri. Jika `params.json` tidak valid, manager menolak menjalankan skrip.

::: warning
Tidak ada pemeriksaan format untuk nilai `string`. Setiap nilai diteruskan dalam tanda kutip tunggal, sehingga tidak pernah diekspansi oleh shell yang menjalankan `action.sh`, tetapi `action.sh` tetap harus memvalidasinya dan memberi tanda kutip saat menggunakannya.
:::

## Contoh

```sh
#!/system/bin/sh
# action.sh
MODPATH=${MODPATH:-${0%/*}}

if [ "$(getprop ro.miui.ui.version.name)" = "" ]; then
  echo "Not a MIUI device"
  exit 2
fi

echo "Mode: ${KSU_PARAM_MODE:-a}"
if [ "$KSU_PARAM_ENABLE_FASTBOOT" = "true" ]; then
  sh "$MODPATH/fastboot.sh" || exit 1
fi
echo "Done"
```

## Kompatibilitas dengan manager lama

Manager yang tidak mendukung skrip sekali jalan akan memasang ZIP sebagai modul biasa yang tombol Action-nya menjalankan `action.sh`. Pastikan `action.sh` tetap berfungsi dalam kasus tersebut: di sana `KSU_ONESHOT`, `MODPATH` dan `KSU_PARAM_*` tidak diatur dan direktori kerjanya adalah direktori modul. Gunakan nilai default sebagai cadangan, seperti pada contoh di atas:

```sh
MODPATH=${MODPATH:-${0%/*}}
MODE=${KSU_PARAM_MODE:-a}
```

## Publikasi

Untuk memublikasikan skrip sekali jalan di [KernelSU Modules](https://modules.kernelsu.org), atur `kind: script` di `module.yaml`. Lihat [Script entries](https://github.com/KernelSU-Modules/tools/blob/main/docs/developers/module-yaml.md#script-entries-kind-script) untuk aturan registri.
