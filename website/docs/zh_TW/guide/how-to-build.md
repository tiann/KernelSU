# 如何建置 KernelSU? {#how-to-build-kernelsu}

首先，您需要閱讀核心建置的 Android 官方文件：

1. [建置核心](https://source.android.com/docs/setup/build/building-kernels)
2. [標準核心映像 (GKI) 發行組建](https://source.android.com/docs/core/architecture/kernel/gki-release-builds)

::: warning 警告
此文件適用於 GKI 裝置，如果您是舊版核心，請參閱[如何為非 GKI 裝置整合 KernelSU](how-to-integrate-for-non-gki)
:::

## 建置核心 {#build-kernel}

### 同步核心原始碼 {#sync-the-kernel-source-code}

```sh
repo init -u https://android.googlesource.com/kernel/manifest
mv <kernel_manifest.xml> .repo/manifests
repo init -m manifest.xml
repo sync
```

`<kernel_manifest.xml>` 是一個可以唯一確定組建的資訊清單，您可以使用這個資訊清單進行可重新預測的組建。您需要從[標準核心映像 (GKI) 發行組建](https://source.android.com/docs/core/architecture/kernel/gki-release-builds)下載資訊清單。

### 建置 {#build}

請先查看[官方文件](https://source.android.com/docs/setup/build/building-kernels)。

例如，我們需要建置 aarch64 核心映像：

```sh
LTO=thin BUILD_CONFIG=common/build.config.gki.aarch64 build/build.sh
```

不要忘記新增 `LTO=thin`，否則，如果您的電腦記憶體小於 24GB，建置可能會失敗。

從 Android 13 開始，核心使用 `bazel` 建置：

```sh
tools/bazel build --config=fast //common:kernel_aarch64_dist
```

:::info 你可能需要知道...
對於某些 Android 14 核心，要使 Wi-Fi/藍牙正常工作，可能需要刪除所有受 GKI 保護的匯出：

```sh
rm common/android/abi_gki_protected_exports_*
```
:::

## 與 KernelSU 一起建置核心 {#build-kernel-with-kernelsu}

如果您可以成功建置核心，那麼建置 KernelSU 就會非常輕鬆，依自己的需求在核心原始碼根目錄中執行以下任一命令：

::: code-group

```sh[最新 tag (穩定版本)]
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -
```

```sh[main 分支 (開發版本)]
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s main
```

```sh[選取 tag (例如 v0.5.2)]
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s v0.5.2
```

:::

然後重新建置核心，您將會得到一個帶有 KernelSU 的核心映像！
