# 如何构建 KernelSU?

首先，您应该阅读内核构建的 Android 官方文档：

1. [构建内核](https://source.android.com/docs/setup/build/building-kernels)
2. [通用内核映像 (GKI) 发布构建](https://source.android.com/docs/core/architecture/kernel/gki-release-builds)

::: warning
本文档适用于 GKI 设备，如果你是旧内核，请参考[如何为非GKI设备集成 KernelSU](how-to-integrate-for-non-gki)
:::

## 构建内核

### 同步内核源码

```sh
repo init -u https://android.googlesource.com/kernel/manifest
mv <kernel_manifest.xml> .repo/manifests
repo init -m manifest.xml
repo sync
```

`<kernel_manifest.xml>` 是一个可以唯一确定构建的清单文件，您可以使用该清单进行可重新预测的构建。 您应该从 [通用内核映像 (GKI) 发布构建](https://source.android.com/docs/core/architecture/kernel/gki-release-builds) 下载清单文件 

### 构建

请先查看 [官方文档](https://source.android.com/docs/setup/build/building-kernels)。

例如，我们需要构建 aarch64 内核镜像：

```sh
LTO=thin BUILD_CONFIG=common/build.config.gki.aarch64 build/build.sh
```

不要忘记添加 `LTO=thin`, 否则，如果您的计算机内存小于 24GB，构建可能会失败.

从 Android 13 开始，内核由 `bazel` 构建:

```sh
tools/bazel build --config=fast //common:kernel_aarch64_dist
```

## 使用 KernelSU 构建内核

如果您可以成功构建内核，那么构建 KernelSU 就很容易，根据自己的需求在内核源代码根目录中运行以下任一命令：

- 最新tag(稳定版本)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -
```

- main分支(开发版本)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s main
```

- 指定tag(比如v0.5.2)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s v0.5.2
```

然后重建内核，您将获得带有 KernelSU 的内核映像！
