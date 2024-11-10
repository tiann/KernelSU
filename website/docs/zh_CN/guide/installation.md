# 安装 {#title}

## 检查您的设备是否被支持 {#check-if-supported}

从 [GitHub Releases](https://github.com/tiann/KernelSU/releases) 下载 KernelSU 管理器应用，然后将应用程序安装到设备并打开：

- 如果应用程序显示 “不支持”，则表示您的设备不支持 KernelSU，你需要自己编译设备的内核才能使用，KernelSU 官方不会也永远不会为你提供一个可以刷写的 boot 镜像。
- 如果应用程序显示 “未安装”，那么 KernelSU 支持您的设备；可以进行下一步操作。

:::info
对于显示“不支持”的设备，这里有一个[非官方支持设备列表](unofficially-support-devices.md)，你可以用这个列表里面的内核自行编译。
:::

## 备份你的 boot.img {#backup-boot-image}

在进行刷机操作之前，你必须先备份好自己的原厂 boot.img。如果你后续刷机出现了任何问题，你都可以通过使用 fastboot 刷回原厂 boot 来恢复系统。

::: warning
任何刷机操作都是有风险的，请务必做好这一步再进行下一步操作！！必要时你还可以备份你手机的所有数据。
:::

## 必备知识 {#acknowage}

### ADB 和 fastboot {#adb-and-fastboot}

此教程默认你会使用 ADB 和 fastboot 工具，如果你没有了解过，建议使用搜索引擎先学习相关知识。

### KMI

KMI 全称 Kernel Module Interface，相同 KMI 的内核版本是**兼容的** 这也是 GKI 中“通用”的含义所在；反之，如果 KMI 不同，那么这些内核之间无法互相兼容，刷入与你设备 KMI 不同的内核镜像可能会导致死机。

具体来说，对 GKI 的设备，其内核版本格式应该如下：

```txt
KernelRelease :=
Version.PatchLevel.SubLevel-AndroidRelease-KmiGeneration-suffix
w      .x         .y       -zzz           -k            -something
```

其中，`w.x-zzz-k` 为 KMI 版本。例如，一个设备内核版本为`5.10.101-android12-9-g30979850fc20`，那么它的 KMI 为 `5.10-android12-9`；理论上刷入其他这个 KMI 的内核也能正常开机。

::: tip
请注意，内核版本中的 SubLevel 不属于 KMI 的范畴！也就是说 `5.10.101-android12-9-g30979850fc20` 与 `5.10.137-android12-9-g30979850fc20` 的 KMI 相同！
:::

### 安全补丁级别 {#security-patch-level}

新的 Android 设备上可能采取了防回滚机制，它不允许刷入一个安全补丁更旧的内核。比如，如果你的设备内核是 `5.10.101-android12-9-g30979850fc20`，它的安全补丁为 `2023-11`；即使你刷入与内核 KMI 一致的内核，如果安全补丁级别比 `2023-11`要老（例如`2023-06`），那么很可能会无法开机。

因此，在保持 KMI 一致的情况下，优先采用安全补丁级别更新的内核。

### 内核版本与 Android 版本 {#kernel-version-vs-android-version}

请注意：**内核版本与 Android 版本并不一定相同！**

如果您发现您的内核版本是 `android12-5.10.101`，然而你 Android 系统的版本为 Android 13 或者其他；请不要觉得奇怪，因为 Android 系统的版本与 Linux 内核的版本号不一定是一致的；Linux 内核的版本号一般与**设备出厂的时候自带的 Android 系统的版本一致**，如果后续 Android 系统升级，内核版本一般不会发生变化。如果你需要刷机，**请以内核版本为准！！**

## 安装介绍 {#installationintroduction}

自 `0.9.0` 版本以后，在 GKI 设备中，KernelSU 支持两种运行模式：

1. `GKI`：使用**通用内核镜像**（GKI）替换掉设备原有的内核。
2. `LKM`：使用**可加载内核模块**（LKM）的方式加载到设备内核中，不会替换掉设备原有的内核。

这两种方式适用于不同的场景，你可以根据自己的需求选择。

### GKI 模式 {#gki-mode}

GKI 模式会替换掉设备原有的内核，使用 KernelSU 提供的通用内核镜像。GKI 模式的优点是：

1. 通用型强，适用于大多数设备；比如三星开启了 KNOX 的设备，LKM 模式无法运作。还有一些冷门的魔改设备，也只能使用 GKI 模式；
2. 不依赖官方固件即可使用；不需要等待官方固件更新，只要 KMI 一致，就可以使用；

### LKM 模式 {#lkm-mode}

LKM 模式不会替换掉设备原有的内核，而是使用可加载内核模块的方式加载到设备内核中。LKM 模式的优点是：

1. 不会替换掉设备原有的内核；如果你对设备原有的内核有特殊需求，或者你希望在使用第三方内核的同时使用 KernelSU，可以使用 LKM 模式；
2. 升级和 OTA 较为方便；升级 KernelSU 时，可以直接在管理器里面安装，无需再手动刷写；系统 OTA 后，可以直接安装到第二个槽位，也无需再手动刷写；
3. 适用于一些特殊场景；比如使用临时 ROOT 权限也可以加载 LKM，由于不需要替换 boot 分区，因此不会触发 avb，不会使设备意外变砖；
4. LKM 可以被临时卸载；如果你临时想取消 root，可以卸载 LKM，这个过程不需要刷写分区，甚至也不用重启设备；如果你想再次 root，只需要重启设备即可；

:::tip 两种模式共存
打开管理器后，你可以在首页看到设备当前运行的模式；注意 GKI 模式的优先级高于 LKM，如你你既使用 GKI 内核替换掉了原有的内核，又使用 LKM 的方式修补了 GKI 内核，那么 LKM 会被忽略，设备将永远以 GKI 的模式运行。
:::

### 选哪个？ {#which-one}

如果你的设备是手机，我们建议您优先考虑 LKM 模式；如果你的设备是模拟器、WSA 或者 Waydroid 等，我们建议您优先考虑 GKI 模式。

## LKM 安装

### 获取官方固件

使用 LKM 的模式，需要获取官方固件，然后在官方固件的基础上修补；如果你使用的是第三方内核，可以把第三方内核的 boot.img 作为官方固件。

获取官方固件的方法有很多，如果你的设备支持 `fastboot boot`，那么我们最推荐以及最简单的方法是使用 `fastboot boot` 临时启动 KernelSU 提供的 GKI 内核，然后安装管理器，最后在管理器中直接安装；这种方法不需要你手动下载官方固件，也不需要你手动提取 boot。

如果你的设备不支持 `fastboot boot`，那么你可能需要手动去下载官方固件包，然后从中提取 boot。

与 GKI 模式不同，LKM 模式会修改 `ramdisk`，因此在出厂 Android 13 的设备上，它需要修补的是 `init_boot` 分区而非 `boot` 分区；而 GKI 模式则永远是操作 `boot` 分区。

### 使用管理器

打开管理器，点击右上角的安装图标，会出现若干个选项：

1. 选择并修补一个文件；如果你手机目前没有 root 权限，你可以选择这个选项，然后选择你的官方固件，管理器会自动修补它；你只需要刷入这个修补后的文件，即可永久获取 root 权限；
2. 直接安装；如果你手机已经 root，你可以选择这个选项，管理器会自动获取你的设备信息，然后自动修补官方固件，然后刷入；你可以考虑使用 `fastboot boot` KernelSU 的 GKI 内核来获取临时 root 安装管理器，然后再使用这个选项；这种方式也是 KernelSU 升级最主要的方式；
3. 安装到另一个分区；如果你的设备支持 A/B 分区，你可以选择这个选项，管理器会自动修补官方固件，然后安装到另一个分区；这种方式适用于 OTA 后的设备，你可以在 OTA 后直接安装到另一个分区，然后重启设备即可；

### 使用命令行

如果你不想使用管理器，你也可以使用命令行来安装 LKM；KernelSU 提供的 `ksud` 工具可以帮助你快速修补官方固件，然后刷入。

这个工具支持 macOS、Linux 和 Windows，你可以在 [GitHub Release](https://github.com/tiann/KernelSU/releases) 下载对应的版本。

使用方法：`ksud boot-patch` 具体的使用方法你可以查看命令行帮助。

```sh
oriole:/ # ksud boot-patch -h
Patch boot or init_boot images to apply KernelSU

Usage: ksud boot-patch [OPTIONS]

Options:
  -b, --boot <BOOT>              boot image path, if not specified, will try to find the boot image automatically
  -k, --kernel <KERNEL>          kernel image path to replace
  -m, --module <MODULE>          LKM module path to replace, if not specified, will use the builtin one
  -i, --init <INIT>              init to be replaced
  -u, --ota                      will use another slot when boot image is not specified
  -f, --flash                    Flash it to boot partition after patch
  -o, --out <OUT>                output path, if not specified, will use current directory
      --magiskboot <MAGISKBOOT>  magiskboot path, if not specified, will use builtin one
      --kmi <KMI>                KMI version, if specified, will use the specified KMI
  -h, --help                     Print help
```

需要说明的几个选项：

1. `--magiskboot` 选项可以指定 magiskboot 的路径，如果不指定，ksud 会在环境变量中查找；如果你不知道如何获取 magiskboot，可以查阅[这里](#patch-boot-image)；
2. `--kmi` 选项可以指定 `KMI` 版本，如果你的设备内核名字没有遵循 KMI 规范，你可以通过这个选项来指定；

最常见的使用方法为：

```sh
ksud boot-patch -b <boot.img> --kmi android13-5.10
```

## GKI 安装

GKI 的安装方法有如下几种，各自适用于不同的场景，请按需选择：

1. 使用 KernelSU 提供的**通用内核镜像**使用 fastboot 安装
2. 使用内核刷写 App（如 KernelFlasher）安装
3. 手动修补 boot.img 然后安装
4. 使用自定义 Recovery（如 TWRP）安装

## 使用 KernelSU 提供的 boot.img 安装 {#install-by-kernelsu-boot-image}

如果你设备的 `boot.img` 采用常用的压缩格式，那么可以采用 KernelSU 提供的的通用内核镜像直接刷入，它不需要 TWRP 或者自行修补镜像。

### 找到合适的 boot.img {#found-propery-image}

KernelSU 为 GKI 设备提供了通用的 boot.img，您应该将 boot.img 刷写到设备的 boot 分区。

您可以从 [GitHub Release](https://github.com/tiann/KernelSU/releases) 下载 boot.img, 请注意您应该使用正确版本的 boot.img。如果您不知道应该下载哪一个文件，请仔细阅读本文档中关于 [KMI](#kmi) 和 [安全补丁级别](#security-patch-level)的描述。

通常情况下，同一个 KMI 和 安全补丁级别下会有三个不同格式的 boot 文件，它们除了内核压缩格式不同之外都一样。请检查您原有 boot.img 的内核压缩格式，您应该使用正确的格式，例如 `lz4`、`gz`；如果是用不正确的压缩格式，刷入 boot 后可能无法开机。

::: info
1. 您可以通过 magiskboot 来获取你原来 boot 的压缩格式；当然您也可以询问与您机型相同的其他更有经验的童鞋。另外，内核的压缩格式通常不会发生变化，如果您使用某个压缩格式成功开机，后续可优先尝试这个格式。
2. 小米设备通常使用 `gz` 或者 **不压缩**。
3. Pixel 设备有些特殊，请查看下面的教程。
:::

### 将 boot.img 刷入设备 {#flash-boot-image}

使用 `adb` 连接您的设备，然后执行 `adb reboot bootloader` 进入 fastboot 模式，然后使用此命令刷入 KernelSU：

```sh
fastboot flash boot boot.img
```

::: info
如果你的设备支持 `fastboot boot`，可以先使用 `fastboot boot boot.img` 来先尝试使用 boot.img 引导系统，如果出现意外，再重启一次即可开机。
:::

### 重启 {#reboot}

刷入完成后，您应该重新启动您的设备：

```sh
fastboot reboot
```

## 使用内核刷写 App 安装 {#install-by-kernel-flasher}

步骤：

1. 下载 AnyKernel3 的刷机包，如果你不知道下载哪一个，请仔细查阅上述文档中关于 [KMI](#kmi) 和 [安全补丁级别](#security-patch-level)的描述；下载错误的刷机包很可能导致无法开机，请注意备份。
2. 打开内核刷写 App（授予必要的 root 权限），使用提供的 AnyKernel3 刷机包刷入。

这种方法需要内核刷写 App 拥有 root 权限，你可以用如下几种方法实现：

1. 你的设备已经获取了 root 权限，比如你已经安装好了 KernelSU 想升级到最新的版本，又或者你通过其他方法（如 Magisk）获取了 root。
2. 如果你的手机没有 root，但手机支持 `fastboot boot boot.img` 这种临时启动的方法，你可以用 KernelSU 提供的 GKI 镜像临时启动你的设备，获取临时的 root 权限，然后使用内核刷写器刷入获取永久 root 权限。


如果您以前没有使用过内核刷写 App，建议使用以下应用：

1. [Kernel Flasher](https://github.com/capntrips/KernelFlasher/releases)
2. [Franco Kernel Manager](https://play.google.com/store/apps/details?id=com.franco.kernel)
3. [Ex Kernel Manager](https://play.google.com/store/apps/details?id=flar2.exkernelmanager)

## 手动修补 boot.img {#patch-boot-image}

对于某些设备来说，其 boot.img 格式不那么常见，比如不是 `lz4`, `gz` 和未压缩；最典型的就是 Pixel，它 boot.img 的格式是 `lz4_legacy` 压缩，ramdisk 可能是 `gz` 也可能是 `lz4_legacy` 压缩；此时如果你直接刷入 KernelSU 提供的 boot.img，手机可能无法开机；这时候，你可以通过手动修补 boot.img 来实现。

任何情况下都推荐使用 `magiskboot` 来修补 boot 镜像，有两个方法：

1. [magiskboot](https://github.com/topjohnwu/Magisk/releases)
2. [magiskboot_build](https://github.com/ookiineko/magiskboot_build/releases/tag/last-ci)

Magisk 官方提供的 `magiskboot` 只能运行在 Android/Linux 设备上，如果你想在 macOS/Windows 上使用 `magiskboot` 可以使用第二个方法。

::: tip
不再推荐使用 Android-Image-Kitchen，因为它可能没有合理地处理 boot 元数据（比如安全补丁级别），从而导致某些设备上会无法启动。
:::

### 准备 {#patch-preparation}

1. 获取你手机的原厂 boot.img；你可以通过你手机的线刷包解压后之间获取，如果你是卡刷包，那你也许需要[payload-dumper-go](https://github.com/ssut/payload-dumper-go)
2. 下载 KernelSU 提供的与你设备 KMI 版本一致的 AnyKernel3 刷机包；如果您不知道应该下载哪一个文件，请仔细阅读本文档中关于 [KMI](#kmi) 和 [安全补丁级别](#security-patch-level)的描述。
3. 解压缩 AnyKernel3 刷机包，获取其中的 `Image` 文件，此文件为 KernelSU 的内核文件。

### 在 Android 设备上使用 magiskboot {#using-magiskboot-on-Android-devices}

1. 在 Magisk 的 [Release 页面](https://github.com/topjohnwu/Magisk/releases) 下载最新的 Magisk 安装包。
2. 将 `Magisk-*(version).apk` 重命名为 `Magisk-*.zip` 然后解压缩。
3. 将解压后的 `Magisk-*/lib/arm64-v8a/libmagiskboot.so` 文件，使用 adb push 到手机：`adb push Magisk-*/lib/arm64-v8a/libmagiskboot.so /data/local/tmp/magiskboot`
4. 使用 adb 将原厂 boot.img 和 AnyKernel3 中的 Image 推送到手机
5. adb shell 进入 /data/local/tmp/ 目录，然后赋予刚 push 文件的可执行权限 `chmod +x magiskboot`
6. adb shell 进入 /data/local/tmp/ 目录，执行 `./magiskboot unpack boot.img` 此时会解包 `boot.img` 得到一个叫做 `kernel` 的文件，这个文件为你原厂的 kernel
7. 使用 `Image` 替换 `kernel`: `mv -f Image kernel`
8. 执行 `./magiskboot repack boot.img` 打包 img，此时你会得到一个 `new-boot.img` 的文件，使用这个文件 fastboot 刷入设备即可。

### 在 macOS/Windows/Linux 上使用 magiskboot {#using-magiskboot-on-PC}

1. 在 [magiskboot_build](https://github.com/ookiineko/magiskboot_build/releases/tag/last-ci) 下载适合你操作系统的 `magiskboot` 二进制文件。
2. 在你的 PC 上准备好设备原厂的 boot.img 和 KernelSU 的 Image。
3. `chmod +x magiskboot`
4. 在你 PC 上合适的目录执行 `./magiskboot unpack boot.img` 来解包 `boot.img`, 你会得到一个 `kernel` 文件，这个文件是你设备原厂的 kernel。
5. 使用 `Image` 替换 `kernel`: `mv -f Image kernel`
6. 执行 `./magiskboot repack boot.img` 打包 img，此时你会得到一个 `new-boot.img` 的文件，使用这个文件 fastboot 刷入设备即可。

:::info
Magisk 官方的 `magiskboot` 可以在 Linux 设备上执行，如果你是 Linux 用户，可以直接用官方版本。
:::

## 使用自定义 Recovery 安装 {#install-by-recovery}

前提：你的设备必须有自定义的 Recovery，如 TWRP；如果没有或者只有官方 Recovery，请使用其他方法。

步骤：

1. 在 KernelSU 的 [Release 页面](https://github.com/tiann/KernelSU/releases) 下载与你手机版本匹配的以 AnyKernel3 开头的 zip 刷机包；如果你不知道下载哪一个，请仔细查阅上述文档中关于**KMI**和**安全补丁级别**的描述；下载错误的刷机包很可能导致无法开机，请注意备份。
2. 重启手机进入 TWRP。
3. 使用 adb 将 AnyKernel3-*.zip 放到手机 /sdcard 然后在 TWRP 图形界面选择安装；或者你也可以直接 `adb sideload AnyKernel-*.zip` 安装。

PS. 这种方法适用于任何情况下的安装（不限于初次安装或者后续升级），只要你用 TWRP 就可以操作。

## 其他变通方法 {#other-methods}

其实所有这些安装方法的主旨只有一个，那就是**替换原厂的内核为 KernelSU 提供的内核**；只要能实现这个目的，就可以安装；比如以下是其他可行的方法：

1. 首先安装 Magisk，通过 Magisk 获取 root 权限后使用内核刷写器刷入 KernelSU 的 AnyKernel 包。
2. 使用某些 PC 上的刷机工具箱刷入 KernelSU 提供的内核。

如果这些方法导致无法开机，请优先尝试用 `magiskboot` 的方法。
