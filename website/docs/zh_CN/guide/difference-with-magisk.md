# KernelSU 模块与 Magisk 的差异 {#title}

虽然 KernelSU 模块与 Magisk 模块有很多相似之处，但由于它们的实现机制完全不同，因此不可避免地会有一些差异；如果你希望你的模块能同时在 Magisk 与 KernelSU 中运行，那么你必须了解这些差异。

## 相同之处 {#similarities}

- 模块文件格式: 都以 zip 的方式组织模块，并且模块的格式几乎相同
- 模块安装目录: 都在 `/data/adb/modules`
- systemless: 都支持通过模块的形式以 systemless 修改 /system
- `post-fs-data.sh`: 执行时机完全一致，语义也完全一致
- `service.sh`: 执行时机完全一致，语义也完全一致
- `system.prop`: 完全相同
- `sepolicy.rule`: 完全相同
- BusyBox：脚本都在 BusyBox 中以“独立模式”运行

## 不同之处 {#differences}

在了解不同之处之前，你需要知道如何区分你的模块是运行在 KernelSU 还是运行在 Magisk 之中；在所有你可以运行模块脚本的地方（`customize.sh`, `post-fs-data.sh`, `service.sh`)，你都可以通过环境变量`KSU` 来区分，在 KernelSU 中，这个环境变量将被设置为 `true`。

以下是一些不同之处：

1. KernelSU 的模块不支持在 Recovery 中安装。
2. KernelSU 的模块没有内置的 Zygisk 支持（但你可以通过 [ZygiskOnKernelSU](https://github.com/Dr-TSNG/ZygiskOnKernelSU) 来使用 Zygisk 模块）。
3. KernelSU 模块替换或者删除文件与 Magisk 完全不同。KernelSU 不支持 `.replace` 方式，相反，你需要通过 `mknod filename c 0 0` 创建同名文件夹来删除对应文件。
4. BusyBox 的目录不同；KernelSU 内置的 BusyBox 在 `/data/adb/ksu/bin/busybox` 而 Magisk 在 `/data/adb/magisk/busybox`；**注意此为 KernelSU 内部行为，未来可能会更改！**
5. KernelSU 不支持 `.replace` 文件；但 KernelSU 支持 `REPLACE` 和 `REMOVE` 变量。
