# Installation

## Check if your device is supported

Download KernelSU manager APP from [GitHub Releases](https://github.com/tiann/KernelSU/releases) and install it to your device:

- If the app shows `Unsupported`, it means **you should compile the kernel yourself**, KernelSU won't and never provide a boot image for you to flash.
- If the app shows `Not installed`, then your devices is officially supported by KernelSU.

:::info
For devices showing `Unsupported`, here is an [Unofficially-support-devices](unofficially-support-devices.md), you can compile the kernel yourself.
:::

## Backup stock boot.img

Before flashing, you must first backup your stock boot.img. If you encounter any bootloop, you can always restore the system by flashing back to the stock factory boot using fastboot.

::: warning
Flashing may cause data loss, be sure to do this step well before proceeding to the next step!! You can also back up all the data on your phone if necessary.
:::

## Necessary knowledge

### ADB and fastboot

By default, you will use ADB and fastboot tools in this tutorial, so if you don't know them, we recommend using a search engine to learn about them first.

### KMI

Kernel Module Interface (KMI), kernel versions with the same KMI are **compatible** This is what "general" means in GKI; conversely, if the KMI is different, then these kernels are not compatible with each other, and flashing a kernel image with a different KMI than your device may cause a bootloop.

Specifically, for GKI devices, the kernel version format should be as follows:

```txt
KernelRelease :=
Version.PatchLevel.SubLevel-AndroidRelease-KmiGeneration-suffix
w      .x         .y       -zzz           -k            -something
```

`w.x-zzz-k` is the KMI version. For example, if a device kernel version is `5.10.101-android12-9-g30979850fc20`, then its KMI is `5.10-android12-9`; theoretically, it can boot up normally with other KMI kernels.

::: tip
Note that the SubLevel in the kernel version is not part of the KMI! That means that `5.10.101-android12-9-g30979850fc20` has the same KMI as `5.10.137-android12-9-g30979850fc20`!
:::

### Security patch level {#security-patch-level}

Newer Android devices may have anti-rollback mechanisms in place that do not allow flashing a boot image with an old security patch level. For example, if your device kernel is `5.10.101-android12-9-g30979850fc20`, it's security patch level is `2023-11`; even if you flash the kernel corresponding to the KMI, if the security patch level is older than `2023- 11` (such as `2023-06`), then it may cause bootloop.

Therefore, kernels with latest security patch levels are preferred for maintaining the correspondence with KMI.

### Kernel version vs Android version

Please note: **Kernel version and Android version are not necessarily the same!**

If you find that your kernel version is `android12-5.10.101`, but your Android system version is Android 13 or other, please don't be surprised, because the version number of the Android system is not necessarily the same as the version number of the Linux kernel. The version number of the Linux kernel is generally correspondent to the version of the Android system that comes with the **device when it is shipped**. If the Android system is upgraded later, the kernel version will generally not change. So before flashing anything, **please always refer to the kernel version!**

## Introduction

Since version `0.9.0`, KernelSU supports two running modes on GKI devices:

1. `GKI`: Replace the original kernel of the device with the **Generic Kernel Image** (GKI) provided by KernelSU.
2. `LKM`: Load the **Loadable Kernel Module** (LKM) into the device kernel without replacing the original kernel.

These two modes are suitable for different scenarios, and you can choose the one according to your needs.

### GKI mode {#gki-mode}

In GKI mode, the original kernel of the device will be replaced with the generic kernel image provided by KernelSU. The advantages of GKI mode are:

1. Strong universality, suitable for most devices; for example, Samsung has enabled KNOX devices, and LKM mode cannot operate. There are also some niche modified devices that can only use GKI mode.
2. Can be used without relying on official firmware; no need to wait for official firmware updates, as long as the KMI is consistent, it can be used.

### LKM mode {#lkm-mode}

In LKM mode, the original kernel of the device will not be replaced, but the loadable kernel module will be loaded into the device kernel. The advantages of LKM mode are:

1. Will not replace the original kernel of the device; if you have special requirements for the original kernel of the device, or you want to use KernelSU while using a third-party kernel, you can use LKM mode.
2. It is more convenient to upgrade and OTA; when upgrading KernelSU, you can directly install it in the manager without flashing manually; after the system OTA, you can directly install it to the second slot without manual flashing.
3. Suitable for some special scenarios; for example, LKM can also be loaded with temporary ROOT permissions. Since it does not need to replace the boot partition, it will not trigger avb and will not cause the device to be bricked.
4. LKM can be temporarily uninstalled; if you want to temporarily disable root access, you can uninstall LKM, this process does not require flashing partitions, or even rebooting the device; if you want to enable root again, just reboot the device.

:::tip Coexistence of two modes
After opening the manager, you can see the current mode of the device on the homepage; note that the priority of GKI mode is higher than that of LKM. For example, if you use GKI kernel to replace the original kernel, and use LKM to patch the GKI kernel, then LKM will be ignored, and the device will always run in GKI mode.
:::

### Which one to choose? {#which-one}

If your device is a mobile phone, we recommend that you prioritize LKM mode; if your device is an emulator, WSA, or Waydroid, we recommend that you prioritize GKI mode.

## LKM installation

### Get the official firmware

To use LKM mode, you need to get the official firmware and then patch it on the basis of the official firmware; if you use a third-party kernel, you can use the `boot.img` of the third-party kernel as the official firmware.

There are many ways to get the official firmware. If your device supports `fastboot boot`, then we recommend **the most recommended and simplest** method is to use `fastboot boot` to temporarily boot the GKI kernel provided by KernelSU, then install the manager, and finally install it directly in the manager; this method does not require you to manually download the official firmware, nor do you need to manually extract the boot.

If your device does not support `fastboot boot`, then you may need to manually download the official firmware package and then extract the boot from it.

Unlike GKI mode, LKM mode will modify the `ramdisk`, so on devices with Android 13, it needs to patch the `init_boot` partition instead of the `boot` partition; meanwhile, GKI mode always operates the `boot` partition.

### Use the manager

Open the manager, click the installation icon in the upper right corner, and several options will appear:

1. Select and patch a file; if your phone does not have root permissions, you can choose this option, and then select your official firmware, the manager will automatically patch it; you only need to flash this patched file to permanently obtain root permissions.
2. Install directly; if your phone is already rooted, you can choose this option, the manager will automatically get your device information, and then automatically patch the official firmware, and then flash it; you can consider using `fastboot boot` KernelSU's GKI kernel to get temporary root and install the manager, and then use this option; this is also the main way to upgrade KernelSU.
3. Install to another partition; if your device supports A/B partition, you can choose this option, the manager will automatically patch the official firmware, and then install it to another partition; this method is suitable for devices after OTA, you can directly install it to another partition after OTA, and then restart the device.

### Use the command line

If you don’t want to use the manager, you can also use the command line to install LKM; the `ksud` tool provided by KernelSU can help you quickly patch the official firmware and then flash it.

This tool supports macOS, Linux, and Windows. You can download the corresponding version from [GitHub Release](https://github.com/tiann/KernelSU/releases).

Usage: `ksud boot-patch`. You can check the command line help for specific options.

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

A few options that need to be explained:

1. The `--magiskboot` option can specify the path of magiskboot. If it is not specified, ksud will look for it in the environment variables; if you don’t know how to get magiskboot, you can refer to [this](#patch-boot-image).
2. The `--kmi` option can specify the `KMI` version. If the kernel name of your device does not follow the KMI specification, you can specify it through this option.

The most common usage is:

```sh
ksud boot-patch -b <boot.img> --kmi android13-5.10
```

## GKI mode installation

There are several installation methods for GKI mode, each suitable for a different scenario, so please choose accordingly:

1. Install with fastboot using the boot.img provided by KernelSU.
2. Install with a kernel flash app, such as KernelFlasher.
3. Repair the boot.img manually and install it.
4. Install with custom Recovery (e.g., TWRP).

## Install with boot.img provided by KernelSU

If your device's `boot.img` uses a commonly used compression format, you can use the GKI images provided by KernelSU to flash it directly. It does not require TWRP or self-patching the image.

### Find proper boot.img

KernelSU provides a generic boot.img for GKI devices, and you should flash the boot.img to the boot partition of the device.

You can download boot.img from [GitHub Release](https://github.com/tiann/KernelSU/releases), please note that you should use the correct version of boot.img. If you don't know which file to download, please carefully read the description of [KMI](#kmi) and [Security patch level](#security-patch-level) in this document.

Normally, there are three boot files in different formats under the same KMI and security patch level. They are all the same except for the kernel compression format. Please check the kernel compression format of your original boot.img. You should use the correct format, such as `lz4`, `gz`; if you use an incorrect compression format, you may encounter bootloop after flashing boot.

:::info Compression format of boot.img
1. You can use magiskboot to get the compression format of your original boot; alternatively, you can also ask for it from community members/developers with the same model as your device. Also, the compression format of the kernel usually does not change, so if you boot successfully with a certain compression format, you can try that format later as well.
2. Xiaomi devices usually use `gz` or **uncompressed**.
3. For Pixel devices, follow the instructions below.
:::

### Flash boot.img to device

Use `adb` to connect your device, then execute `adb reboot bootloader` to enter fastboot mode, then use this command to flash KernelSU:

```sh
fastboot flash boot boot.img
```

::: info
If your device supports `fastboot boot`, you can first use `fastboot boot boot.img` to try to use boot.img to boot the system first. If something unexpected happens, restart it again to boot.
:::

### Reboot

After the flashing process is complete, you should reboot your device:

```sh
fastboot reboot
```

## Install with Kernel Flasher

Steps:

1. Download the AnyKernel3 zip. If you don't know which file to download, please carefully read the description of [KMI](#kmi) and [Security Patch Level](#security-patch-level) in this document.
2. Open the Kernel Flash App (grant necessary root permissions) and use the provided AnyKernel3 zip to flash.

This way requires the kernel flash App to have root permissions. You can use the following methods to achieve this:

1. Your device is rooted. For example, you have installed KernelSU and want to upgrade to the latest version, or you have rooted through other methods (such as Magisk).
2. If your device is not rooted, but the phone supports the temporary boot method of `fastboot boot boot.img`, you can use the GKI image provided by KernelSU to temporarily boot your device, obtain temporary root permissions, and then use the Kernel Flash App to obtain permanent root privileges.

Some of kernel flashing apps that can be used for this:

1. [Kernel Flasher](https://github.com/capntrips/KernelFlasher/releases)
2. [Franco Kernel Manager](https://play.google.com/store/apps/details?id=com.franco.kernel)
3. [Ex Kernel Manager](https://play.google.com/store/apps/details?id=flar2.exkernelmanager)

P.S. This method is more convenient when upgrading KernelSU and can be done without a computer (make a backup first!).

## Patch boot.img manually {#patch-boot-image}

For some devices, the boot.img format is not so common, such as not `lz4`, `gz` and uncompressed; the most typical example is a Pixel, it's boot.img format is `lz4_legacy` compressed, ramdisk may be `gz` may also be `lz4_legacy` compression; currently, if you directly flash the boot.img provided by KernelSU, the phone may not be able to boot; as an alternative, and you can manually patch the boot.img to achieve this.

It's always recommended to use `magiskboot` to patch images, there are two ways:

1. [magiskboot](https://github.com/topjohnwu/Magisk/releases)
2. [magiskboot_build](https://github.com/ookiineko/magiskboot_build/releases/tag/last-ci)

The official build of `magiskboot` can only run on Android devices, if you want to run it on PC, you can try the second option.

::: tip
Android-Image-Kitchen is not recommended for now, as it doesn't handle the boot metadata (such as security patch level) correctly, thus it may not work on some devices.
:::

### Preparation

1. Get your phone's stock boot.img; you can get it from your device manufacturers, you may need [payload-dumper-go](https://github.com/ssut/payload-dumper-go).
2. Download the AnyKernel3 zip file provided by KernelSU that matches the KMI version of your device (you can refer to the *Install with custom Recovery*).
3. Unpack the AnyKernel3 package and get the `Image` file, which is the kernel file of KernelSU.

### Using magiskboot on Android devices {#using-magiskboot-on-Android-devices}

1. Download latest Magisk from [Release Page](https://github.com/topjohnwu/Magisk/releases).
2. Rename `Magisk-*(version).apk` to `Magisk-*.zip` and unzip it.
3. Push `Magisk-*/lib/arm64-v8a/libmagiskboot.so` to your device by adb: `adb push Magisk-*/lib/arm64-v8a/libmagiskboot.so /data/local/tmp/magiskboot`
4. Push stock boot.img and Image in AnyKernel3 to your device.
5. Enter adb shell and run `cd /data/local/tmp/` directory, then `chmod +x magiskboot`
6. Enter adb shell and run `cd /data/local/tmp/` directory, execute `./magiskboot unpack boot.img` to unpack `boot.img`, you will get a `kernel` file, this is your stock kernel.
7. Replace `kernel` with `Image` by running the command: `mv -f Image kernel`.
8. Execute `./magiskboot repack boot.img` to repack boot image, and you will get a `new-boot.img` file, flash this file to device by fastboot.

### Using magiskboot on Windows/macOS/Linux PC{#using-magiskboot-on-PC}

1. Download the corresponding `magiskboot` binary for your OS from [magiskboot_build](https://github.com/ookiineko/magiskboot_build/releases/tag/last-ci).
2. Prepare stock boot.img and Image in your PC.
3. Run `chmod +x magiskboot`.
4. Enter the corresponding directory, execute `./magiskboot unpack boot.img` to unpack `boot.img`, you will get a `kernel` file, this is your stock kernel.
5. Replace `kernel` with `Image` by running the command: `mv -f Image kernel`.
6. Execute `./magiskboot repack boot.img` to repack the boot image, and you will get a `new-boot.img` file, flash this file to device by fastboot.

::: info
Official `magiskboot` can run in Linux environments normally, if you are a Linux user, you can use the official build.
:::

## Install with Custom Recovery

Prerequisite: Your device must have a Custom Recovery, such as TWRP; if there is no custom recovery available for your device, use another method.

Steps:

1. From the [Release page](https://github.com/tiann/KernelSU/releases) of KernelSU, download the zip package starting with `AnyKernel3` that matches your phone version; for example, if the device's kernel version is `android12-5.10. 66`, then you should download the `AnyKernel3-android12-5.10.66_yyyy-MM.zip` file (where `yyyy` is the year and `MM` is the month).
2. Reboot the device into TWRP.
3. Use adb to place AnyKernel3-*.zip into the device's `/sdcard` location and choose to install it in the TWRP GUI; or you can directly run `adb sideload AnyKernel-*.zip` to install.

P.S. This method is suitable for any installation (not limited to initial installation or subsequent upgrades), as long as you're using TWRP.

## Other methods

In fact, all of these installation methods have only one main idea, which is to **replace the original kernel for the one provided by KernelSU**; as long as this can be achieved, it can be installed; for example, the following are other possible methods.

1. First install Magisk, get root privileges through Magisk and then use the kernel flasher to flash in the AnyKernel3 zip from KernelSU.
2. Use any flashing toolkit on PC to flash the kernel provided by KernelSU.

However, if it doesn't work, please try `magiskboot` approach.
