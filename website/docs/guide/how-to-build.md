# How to build

First, you should read the official Android documentation for kernel build:

1. [Build kernels](https://source.android.com/docs/setup/build/building-kernels)
2. [GKI release builds](https://source.android.com/docs/core/architecture/kernel/gki-release-builds)

::: warning
This page is for GKI devices, if you use an old kernel, please refer [Intergrate for non-GKI devices](how-to-integrate-for-non-gki).
:::

## Build kernel

### Sync the kernel source code

```sh
repo init -u https://android.googlesource.com/kernel/manifest
mv <kernel_manifest.xml> .repo/manifests
repo init -m manifest.xml
repo sync
```

The `<kernel_manifest.xml>` file is a manifest that uniquely identifies a build, allowing you to make it reproducible. To do this, you should download the manifest file from [GKI release builds](https://source.android.com/docs/core/architecture/kernel/gki-release-builds).

### Build

Please check the [Building kernels](https://source.android.com/docs/setup/build/building-kernels) first.

For example, to build an `aarch64` kernel image:

```sh
LTO=thin BUILD_CONFIG=common/build.config.gki.aarch64 build/build.sh
```

Don't forget to add the `LTO=thin` flag; otherwise, the build may fail if your computer has less than 24 GB of memory.

Starting from Android 13, the kernel is built by `bazel`:

```sh
tools/bazel build --config=fast //common:kernel_aarch64_dist
```

::: info
For some Android 14 kernels, to make Wi-Fi/Bluetooth work, it might be necessary to remove all GKI protected exports:

```sh
rm common/android/abi_gki_protected_exports_*
```
:::

## Build kernel with KernelSU

If you can successfully build the kernel, adding support for KernelSU will be relatively easy. In the root of kernel source directory, run any of the options listed below:

::: code-group

```sh[Latest tag (stable)]
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -
```

```sh[ main branch (dev)]
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s main
```

```sh[Select tag (such as v0.5.2)]
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s v0.5.2
```

:::

Then, rebuild the kernel and you will get a kernel image with KernelSU!
