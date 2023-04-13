# How to build KernelSU?

First, you should read the Android Official docs for kernel build:

1. [Building Kernels](https://source.android.com/docs/setup/build/building-kernels)
2. [GKI Release Builds](https://source.android.com/docs/core/architecture/kernel/gki-release-builds)

::: warning
This page is for GKI devices, if you use an old kernel, please refer [how to integrate KernelSU for old kernel](how-to-integrate-for-non-gki)
:::

## Build Kernel

### Sync the kernel source code

```sh
repo init -u https://android.googlesource.com/kernel/manifest
mv <kernel_manifest.xml> .repo/manifests
repo init -m manifest.xml
repo sync
```

The `<kernel_manifest.xml>` is a manifest file which can determine a build uniquely, you can use the manifest to do a re-preducable build. You should download the manifest file from [Google GKI release builds](https://source.android.com/docs/core/architecture/kernel/gki-release-builds)

### Build

Please check the [official docs](https://source.android.com/docs/setup/build/building-kernels) first.

For example, we need to build aarch64 kernel image:

```sh
LTO=thin BUILD_CONFIG=common/build.config.gki.aarch64 build/build.sh
```

Don't forget to add the `LTO=thin` flag, otherwise the build may fail if your computer's memory is less then 24Gb.

Starting from Android 13, the kernel is built by `bazel`:

```sh
tools/bazel build --config=fast //common:kernel_aarch64_dist
```

## Build Kernel with KernelSU

If you can build the kernel successfully, then build KernelSU is so easy, Select any one run in Kernel source root dir:

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

And then rebuild the kernel and you will get a kernel image with KernelSU!
