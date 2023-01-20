# 安装

## 检查您的设备是否被支持

从 [GitHub Releases](https://github.com/tiann/KernelSU/releases) 或 [GitHub Actions](https://github.com/tiann/KernelSU/actions/workflows/build-manager.yml) 下载 KernelSU 管理器应用，然后将应用程序安装到设备并打开：

- 如果应用程序显示 “不支持”，则表示您的设备不支持 KernelSU，你需要自己编译设备的内核才能使用，KernelSU 官方不会也永远不会为你提供一个可以刷写的 boot 镜像。
- 如果应用程序显示 “未安装”，那么 KernelSU 支持您的设备。

## 找到合适的 boot.img

KernelSU 为 GKI 设备提供了通用的 boot.img，您应该将 boot.img 刷写到设备的引导分区。

您可以从 [GitHub Actions for Kernel](https://github.com/tiann/KernelSU/actions/workflows/build-kernel.yml) 下载 boot.img, 请注意您应该使用正确版本的 boot.img. 例如，如果您的设备显示内核是 `5.10.101`, 需要下载 `5.10.101-xxxx.boot.xxx`.

另外，请检查您原有 boot.img 的内核压缩格式，您应该使用正确的格式，例如 `lz4`、`gz`。

## 将 boot.img 刷入设备

使用 `adb` 连接您的设备，然后执行 `adb reboot bootloader` 进入 fastboot 模式，然后使用此命令刷入 KernelSU：

```sh
fastboot flash boot boot.img
```

## 重启

刷入完成后，您应该重新启动您的设备：

```sh
fastboot reboot
```
