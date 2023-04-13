# Bagaimana caranya untuk build KernelSU?

Pertama, Anda harus membaca dokumen resmi Android untuk membangun kernel:

1. [Building Kernels](https://source.android.com/docs/setup/build/building-kernels)
2. [GKI Release Builds](https://source.android.com/docs/core/architecture/kernel/gki-release-builds)

> Halaman ini untuk perangkat GKI, jika Anda menggunakan kernel lama, silakan lihat [cara mengintegrasikan KernelSU untuk kernel lama](how-to-integrate-for-non-gki)

## Build Kernel

### Menyinkronkan source code kernel

```sh
repo init -u https://android.googlesource.com/kernel/manifest
mv <kernel_manifest.xml> .repo/manifests
repo init -m manifest.xml
repo sync
```

`<kernel_manifest.xml>` adalah berkas manifes yang dapat menentukan build secara unik, Anda dapat menggunakan manifes tersebut untuk melakukan build yang dapat diprediksikan ulang. Anda harus mengunduh berkas manifes dari [Google GKI release builds](https://source.android.com/docs/core/architecture/kernel/gki-release-builds)

### Build

Silakan periksa [official docs](https://source.android.com/docs/setup/build/building-kernels) terlebih dahulu.

Sebagai contoh, kita perlu build image kernel aarch64:

```sh
LTO=thin BUILD_CONFIG=common/build.config.gki.aarch64 build/build.sh
```

Jangan lupa untuk menambahkan flag `LTO=thin`, jika tidak, maka build akan gagal jika memori komputer Anda kurang dari 24GB.

Mulai dari Android 13, kernel dibuild oleh `bazel`:

```sh
tools/bazel build --config=fast //common:kernel_aarch64_dist
```

## Build Kernel dengan KernelSU

Jika Anda dapat build kernel dengan sukses, maka build KernelSU sangatlah mudah, jalankan perintah ini di root dir kernel source:

- Latest tag(stable)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -
```

- main branch(dev)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s main
```

- Select tag(Such as v0.5.2)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s v0.5.2
```

Dan kemudian build ulang kernel dan Anda akan mendapatkan image kernel dengan KernelSU!
