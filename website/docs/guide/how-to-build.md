# How to build KernelSU?

First, you should read the Android Official docs for kernel build:

1. [Building Kernels](https://source.android.com/docs/setup/build/building-kernels)
2. [GKI Release Builds](https://source.android.com/docs/core/architecture/kernel/gki-release-builds)

::: warning
This page is for GKI devices, if you use an old kernel, please refer [How to integrate KernelSU for non-GKI kernels](how-to-integrate-for-non-gki).
:::

## Build Kernel

### Sync the kernel source code

```sh
repo init -u https://android.googlesource.com/kernel/manifest
mv <kernel_manifest.xml> .repo/manifests
repo init -m manifest.xml
repo sync
```

The `<kernel_manifest.xml>` is a manifest file which can determine a build uniquely, you can use the manifest to do a re-preducable build. You should download the manifest file from [Google GKI release builds](https://source.android.com/docs/core/architecture/kernel/gki-release-builds).

### Build

Please check the [official docs](https://source.android.com/docs/setup/build/building-kernels) first.

For example, to build an aarch64 kernel image:

```sh
LTO=thin BUILD_CONFIG=common/build.config.gki.aarch64 build/build.sh
```

Don't forget to add the `LTO=thin` flag, otherwise the build may fail if your computer's memory is less then 24 GB.

Starting from Android 13, the kernel is built by `bazel`:

```sh
tools/bazel build --config=fast //common:kernel_aarch64_dist
```

:::info
For some of the Android 14 kernels, to make Wi-Fi/Bluetooth work, it might be necessary to remove all the GKI protected exports:

```sh
rm common/android/abi_gki_protected_exports_*
```
:::

## Build Kernel with KernelSU

If you're able to build the kernel successfully, then adding KernelSU support to it is relatively easy. In the root of kernel source directory, run any of the command options listed below:

::: code-group

```sh[Latest tag(stable)]
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -
```

```sh[ main branch(dev)]
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s main
```

```sh[Select tag(Such as v0.5.2)]
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s v0.5.2
```

:::

And then rebuild the kernel and you will get a kernel image with KernelSU!
