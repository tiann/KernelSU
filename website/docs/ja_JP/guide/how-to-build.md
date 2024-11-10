# KernelSU のビルド方法は？

まず、Android の公式ドキュメントを読むべきです：

1. [カーネルをビルドする](https://source.android.com/docs/setup/build/building-kernels)
2. [GKI リリースビルド](https://source.android.com/docs/core/architecture/kernel/gki-release-builds)

::: 警告
このページは GKI デバイス用です。もし古いカーネルを使用している場合は、[古いカーネルへの KernelSU の統合方法](how-to-integrate-for-non-gki)を参照してください。
:::

## カーネルビルド

### カーネルソースコードの同期

```sh
repo init -u https://android.googlesource.com/kernel/manifest
mv <kernel_manifest.xml> .repo/manifests
repo init -m manifest.xml
repo sync
```

`<kernel_manifest.xml>` は、ビルドを一意に決定するマニフェストファイルです。マニフェストを使用して再現可能なビルドを行えます。マニフェストファイルは [Google GKI リリースビルド](https://source.android.com/docs/core/architecture/kernel/gki-release-builds) からダウンロードしてください。

### ビルド

まずは [公式ドキュメント](https://source.android.com/docs/setup/build/building-kernels)を確認してください。

たとえば、aarch64 カーネルイメージをビルドする必要があります：

```sh
LTO=thin BUILD_CONFIG=common/build.config.gki.aarch64 build/build.sh
```
`LTO=thin` フラグを追加するのを忘れないでください。それをしないと、コンピュータのメモリが 24Gb 未満の場合にビルドに失敗する可能性があります。

Android 13 からは、カーネルは `bazel` によってビルドされます：

```sh
tools/bazel build --config=fast //common:kernel_aarch64_dist
```
## KernelSU を使ったカーネルビルド

もしカーネルを正常にビルドできた場合、KernelSU をビルドするのは簡単です。カーネルソースのルートディレクトリで任意のものを選択して実行します：

::: code-group
```sh[最新タグ(安定版)]
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -
```
```sh[ main ブランチ (開発用)]
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s main
```
```sh[タグを選択 (例：v0.5.2)]
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s v0.5.2
```

:::

その後でカーネルを再ビルドすると、KernelSU が組み込まれたカーネルイメージが得られます！
