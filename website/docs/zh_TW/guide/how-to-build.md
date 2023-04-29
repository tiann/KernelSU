# 如何建置 KernelSU?

首先，您需要閱讀核心建置的 Android 官方文件：

1. [建置核心](https://source.android.com/docs/setup/build/building-kernels)
2. [標準核心映像 (GKI) 發行組建](https://source.android.com/docs/core/architecture/kernel/gki-release-builds)

::: warning
此文件適用於 GKI 裝置，如果您是舊版核心，請參閱[如何為非 GKI 裝置整合 KernelSU](how-to-integrate-for-non-gki)
:::

## 建置核心

### 同步核心原始碼

```sh
repo init -u https://android.googlesource.com/kernel/manifest
mv <kernel_manifest.xml> .repo/manifests
repo init -m manifest.xml
repo sync
```

`<kernel_manifest.xml>` 是一個可以唯一確定組建的資訊清單檔案，您可以使用這個資訊清單進行可重新預測的組建。您需要從[標準核心映像 (GKI) 發行組建](https://source.android.com/docs/core/architecture/kernel/gki-release-builds) 下載資訊清單檔案

### 建置

請先查看[官方文件](https://source.android.com/docs/setup/build/building-kernels)。

例如，我們需要建置 aarch64 核心映像：

```sh
LTO=thin BUILD_CONFIG=common/build.config.gki.aarch64 build/build.sh
```

不要忘記新增 `LTO=thin`，否則，如果您的電腦記憶體小於 24GB，建置可能會失敗。

從 Android 13 開始，核心由 `bazel` 建置：

```sh
tools/bazel build --config=fast //common:kernel_aarch64_dist
```

## 使用 KernelSU 建置核心

如果您可以成功建置核心，那麼建置 KernelSU 就會非常輕鬆，依自己的需求在核心原始碼根目錄中執行以下任一命令：

- 最新 tag (穩定版本)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -
```

- main 分支 (開發版本)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s main
```

- 選取 tag (例如 v0.5.2)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s v0.5.2
```

然後重新建置核心，您將會得到一個帶有 KernelSU 的核心映像！
