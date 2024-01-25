# Installation

## Check if your device is supported

Download KernelSU manager APP from [GitHub Releases](https://github.com/tiann/KernelSU/releases) and install it to your device:

- If the app shows `Unsupported`, it means **You should compile the kernel yourself**, KernelSU won't and never provide a boot image for you to flash.
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

Newer Android devices may have anti-rollback mechanisms in place that do not allow flashing a boot image with an old security patch level. For example, if your device kernel is `5.10.101-android12-9-g30979850fc20`, its security patch is `2023-11`; even if you flash the kernel consistent with the kernel KMI, if the security patch level is older than `2023- 11`(such as `2023-06`), then it may cause bootloop.

Therefore, kernels with latest security patch levels are preferred while maintaining KMI consistency.

### Kernel version vs. Android version

Please note: **Kernel version and Android version are not necessarily the same!**

If you find that your kernel version is `android12-5.10.101`, but your Android system version is Android 13 or other; please don't be surprised, because the version number of the Android system is not necessarily the same as the version number of the Linux kernel; The version number of the Linux kernel is generally consistent with the version of the Android system that comes with the **device when it is shipped**. If the Android system is upgraded later, the kernel version will generally not change. If you need to flash, **please always refer to the kernel version!!**

## Introduction

There are several installation methods for KernelSU, each suitable for a different scenario, so please choose as needed.

1. Install with fastboot using the boot.img provided by KernelSU
2. Install with a kernel flash app, such as KernelFlasher
3. Repair the boot.img manually and install it
4. Install with custom Recovery (e.g. TWRP)

## Install with boot.img provided by KernelSU

If your device's `boot.img` uses a commonly used compression format, you can use the GKI images provided by KernelSU to flash it directly. It does not require TWRP or self-patching the image.

### Find proper boot.img

KernelSU provides a generic boot.img for GKI devices and you should flush the boot.img to the boot partition of the device.

You can download boot.img from [GitHub Release](https://github.com/tiann/KernelSU/releases), please note that you should use the correct version of boot.img. If you don't know which file to download, please carefully read the description of [KMI](#kmi) and [Security Patch Level](#security-patch-level) in this document.

Normally, there are three boot files in different formats under the same KMI and security patch level. They are all the same except for the kernel compression format. Please check the kernel compression format of your original boot.img. You should use the correct format, such as `lz4`, `gz`; if you use an incorrect compression format, you may encounter bootloop after flashing boot.

::: info
1. You can use magiskboot to get the compression format of your original boot; of course you can also ask other, more experienced kids with the same model as your device. Also, the compression format of the kernel usually does not change, so if you boot successfully with a certain compression format, you can try that format later.
2. Xiaomi devices usually use `gz` or **uncompressed**.
3. For Pixel devices, follow below instructions.
:::

### flash boot.img to device

Use `adb` to connect your device, then execute `adb reboot bootloader` to enter fastboot mode, then use this command to flash KernelSU:

```sh
fastboot flash boot boot.img
```

::: info
If your device supports `fastboot boot`, you can first use `fastboot boot boot.img` to try to use boot.img to boot the system first. If something unexpected happens, restart it again to boot.
:::

### reboot

After flashing is complete, you should reboot your device:

```sh
fastboot reboot
```

## Install with Kernel Flasher

Step:

1. Download the AnyKernel3 zip. If you don't know which file to download, please carefully read the description of [KMI](#kmi) and [Security Patch Level](#security-patch-level) in this document.
2. Open the Kernel Flash App (grant necessary root permissions) and use the provided AnyKernel3 zip to flash.

This way requires the kernel flash App to have root permissions. You can use the following methods to achieve this:

1. Your device has rooted. For example, you have installed KernelSU and want to upgrade to the latest version, or you have rooted through other methods (such as Magisk).
2. If your phone is not rooted, but the phone supports the temporary boot method of `fastboot boot boot.img`, you can use the GKI image provided by KernelSU to temporarily boot your device, obtain temporary root permissions, and then use the kernel flash Flash the writer to obtain permanent root privileges.

1. [Kernel Flasher](https://github.com/capntrips/KernelFlasher/releases)
2. [Franco Kernel Manager](https://play.google.com/store/apps/details?id=com.franco.kernel)
3. [Ex Kernel Manager](https://play.google.com/store/apps/details?id=flar2.exkernelmanager)

PS. This method is more convenient when upgrading KernelSU and can be done without a computer (backup first!). .

## Patch boot.img manually

For some devices, the boot.img format is not so common, such as not `lz4`, `gz` and uncompressed; the most typical is Pixel, its boot.img format is `lz4_legacy` compressed, ramdisk may be `gz` may also be `lz4_legacy` compression; at this time, if you directly flash the boot.img provided by KernelSU, the phone may not be able to boot; at this time, you can manually patch the boot.img to achieve.

It's always recommended to use `magiskboot` to patch images, there are two ways:

1. [magiskboot](https://github.com/topjohnwu/Magisk/releases)
2. [magiskboot_build](https://github.com/ookiineko/magiskboot_build/releases/tag/last-ci)

The official build of `magiskboot` can only run on Android devices, if you want to run it on PC, you can try the second one.

::: tip
Android-Image-Kitchen is not recommended now, because it doesn't handle the boot metadata(such as security patch level) correctly, thus it may not work on some devices.
:::

### Preparation

1. Get your phone's stock boot.img; you can get it from your device manufacturers, you may need [payload-dumper-go](https://github.com/ssut/payload-dumper-go)
2. Download the AnyKernel3 zip file provided by KernelSU that matches the KMI version of your device (you can refer to the *Install with custom Recovery*).
3. Unpack the AnyKernel3 package and get the `Image` file, which is the kernel file of KernelSU.

### Using magiskboot on Android devices {#using-magiskboot-on-Android-devices}

1. Download latest Magisk from [Release Page](https://github.com/topjohnwu/Magisk/releases)
2. Rename `Magisk-*(version).apk` to `Magisk-*.zip` and unzip it.
3. Push `Magisk-*/lib/arm64-v8a/libmagiskboot.so` to your device by adb: `adb push Magisk-*/lib/arm64-v8a/libmagiskboot.so /data/local/tmp/magiskboot`
4. Push stock boot.img and Image in AnyKernel3 to your device.
5. Enter adb shell and cd `/data/local/tmp/` directory, then `chmod +x magiskboot`
6. Enter adb shell and cd `/data/local/tmp/` directory, execute `./magiskboot unpack boot.img` to unpack `boot.img`, you will get a `kernel` file, this is your stock kernel.
7. Replace `kernel` with `Image`: `mv -f Image kernel`
8. Execute `./magiskboot repack boot.img` to repack boot img, and you will get a `new-boot.img` file, flash this file to device by fastboot.

### Using magiskboot on Windows/macOS/Linux PC{#using-magiskboot-on-PC}

1. Download proper `magiskboot` for your OS from [magiskboot_build](https://github.com/ookiineko/magiskboot_build/releases/tag/last-ci)
2. Prepare stock boot.img and Image in your PC.
3. `chmod +x magiskboot`
4. Enter the proper directory, execute `./magiskboot unpack boot.img` to unpack `boot.img`, you will get a `kernel` file, this is your stock kernel.
5. Replace `kernel` with `Image`: `mv -f Image kernel`
6. Execute `./magiskboot repack boot.img` to repack boot img, and you will get a `new-boot.img` file, flash this file to device by fastboot.

::: info
Official `magiskboot` can run `Linux` device normally, if you are a Linux user, you can use official build.
:::

## Install with custom Recovery

Prerequisite: Your device must have a custom Recovery, such as TWRP; if not or only official Recovery is available, use another method.

Step:

1. From the [Release page](https://github.com/tiann/KernelSU/releases) of KernelSU, download the zip package starting with AnyKernel3 that matches your phone version; for example, the phone kernel version is `android12-5.10. 66`, then you should download the file `AnyKernel3-android12-5.10.66_yyyy-MM.zip` (where `yyyy` is the year and `MM` is the month).
2. Reboot the phone into TWRP.
3. Use adb to put AnyKernel3-*.zip into the phone /sdcard and choose to install it in the TWRP GUI; or you can directly `adb sideload AnyKernel-*.zip` to install.

PS. This method is suitable for any installation (not limited to initial installation or subsequent upgrades), as long as you use TWRP.

## Other methods

In fact, all these installation methods have only one main idea, which is to **replace the original kernel for the one provided by KernelSU**; as long as this can be achieved, it can be installed; for example, the following are other possible methods.

1. First install Magisk, get root privileges through Magisk and then use the kernel flasher to flash in the AnyKernel zip from KernelSU.
2. Use some flashing toolkit on PCs to flash in the kernel provided KernelSU.

But if it doesn't work, please try `magiskboot` way.
