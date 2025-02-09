# Installation

## Check if your device is supported

Download KernelSU manager from [GitHub Releases](https://github.com/tiann/KernelSU/releases) and install it on your device:

- If the app shows `Unsupported`, it means that **you should compile the kernel yourself**, KernelSU won't and never provide a boot.img file for you to flash.
- If the app shows `Not installed`, then your device is officially supported by KernelSU.

::: info
For devices showing `Unsupported`, you can check the list of [Unofficially supported devices](unofficially-support-devices.md). You can compile the kernel yourself.
:::

## Backup stock boot.img

Before flashing, it's essential that you back up your stock boot.img. If you encounter any bootloop, you can always restore the system by flashing back to the stock factory boot using fastboot.

::: warning
Flashing may cause data loss. Make sure to do this step well before proceeding to the next step! You can also back up all the data on your device if necessary.
:::

## Necessary knowledge

### ADB and fastboot

By default, you will use ADB and fastboot tools in this tutorial, so if you don't know them, we recommend using a search engine to learn about them first.

### KMI

Kernel Module Interface (KMI), kernel versions with the same KMI are **compatible**, this is what "general" means in GKI; conversely, if the KMI is different, then these kernels aren't compatible with each other, and flashing a kernel image with a different KMI than your device may cause a bootloop.

Specifically, for GKI devices, the kernel version format should be as follows:

```txt
KernelRelease :=
Version.PatchLevel.SubLevel-AndroidRelease-KmiGeneration-suffix
w      .x         .y       -zzz           -k            -something
```

`w.x-zzz-k` is the KMI version. For example, if a device kernel version is `5.10.101-android12-9-g30979850fc20`, then its KMI is `5.10-android12-9`. Theoretically, it can boot up normally with other KMI kernels.

::: tip
Note that the SubLevel in the kernel version isn't part of the KMI! This means that `5.10.101-android12-9-g30979850fc20` has the same KMI as `5.10.137-android12-9-g30979850fc20`!
:::

### Security patch level {#security-patch-level}

Newer Android devices may have anti-rollback mechanisms that prevent flashing a boot image with an old security patch level. For example, if your device kernel is `5.10.101-android12-9-g30979850fc20`, the security patch level is `2023-11`; even if you flash the kernel corresponding to the KMI, if the security patch level is older than `2023-11` (such as `2023-06`), it may cause a bootloop.

Therefore, kernels with latest security patch levels are preferred to maintain compatibility with the KMI.

### Kernel version vs Android version

Please note: **Kernel version and Android version aren't necessarily the same!**

If you find that your kernel version is `android12-5.10.101`, but your Android system version is Android 13 or other, don't be surprised, because the version number of the Android system isn't necessarily the same as the version number of the Linux kernel. The version number of the Linux kernel is generally correspondent to the version of the Android system that comes with the **device when it is shipped**. If the Android system is upgraded later, the kernel version will generally not change. So, before flashing anything, **always refer to the kernel version!**

## Introduction

Since version [0.9.0](https://github.com/tiann/KernelSU/releases/tag/v0.9.0), KernelSU supports two running modes on GKI devices:

1. `GKI`: Replace the original kernel of the device with the **Generic Kernel Image** (GKI) provided by KernelSU.
2. `LKM`: Load the **Loadable Kernel Module** (LKM) into the device kernel without replacing the original kernel.

These two modes are suitable for different scenarios, and you can choose the one according to your needs.

### GKI mode {#gki-mode}

In GKI mode, the original kernel of the device will be replaced with the generic kernel image provided by KernelSU. The advantages of GKI mode are:

1. Strong universality, suitable for most devices. For example, Samsung has enabled KNOX devices, and LKM mode cannot work. There are also some niche modified devices that can only use GKI mode.
2. Can be used without relying on official firmware, and there is no need to wait for official firmware updates, as long as the KMI is consistent, it can be used.

### LKM mode {#lkm-mode}

In LKM mode, the original kernel of the device won't be replaced, but the loadable kernel module will be loaded into the device kernel. The advantages of LKM mode are:

1. Won't replace the original kernel of the device. If you have special requirements for the original kernel of the device, or you want to use KernelSU while using a third-party kernel, you can use LKM mode.
2. It's more convenient to upgrade and OTA. When upgrading KernelSU, you can directly install it in the manager without flashing manually. After the system OTA, you can directly install it to the second slot without manual flashing.
3. Suitable for some special scenarios. For example, LKM can also be loaded with temporary root permissions. Since it doesn't need to replace the boot partition, it won't trigger AVB and won't cause the device to be bricked.
4. LKM can be temporarily uninstalled. If you want to temporarily disable root access, you can uninstall LKM. This process doesn't require flashing partitions, or even rebooting the device. If you want to enable root again, just reboot the device.

::: tip COEXISTENCE OF TWO MODES
After opening the manager, you can see the current mode of the device on the homepage. Note that the priority of GKI mode is higher than that of LKM. For example, if you use GKI kernel to replace the original kernel, and use LKM to patch the GKI kernel, the LKM will be ignored, and the device will always run in GKI mode.
:::

### Which one to choose? {#which-one}

If your device is a mobile phone, we recommend that you prioritize LKM mode. If your device is an emulator, WSA, or Waydroid, we recommend that you prioritize GKI mode.

## LKM installation

### Get the official firmware

To use LKM mode, you need to get the official firmware and patch it based on the official firmware. If you use a third-party kernel, you can use the `boot.img` of the third-party kernel as the official firmware.

There are many ways to get the official firmware. If your device supports `fastboot boot`, we recommend **the most recommended and simplest** method is to use `fastboot boot` to temporarily boot the GKI kernel provided by KernelSU, then install the manager, and finally install it directly in the manager. This method doesn't require manually downloading the official firmware or manually extracting the boot.

If your device doesn't support `fastboot boot`, you may need to manually download the official firmware package and extract the boot from it.

Unlike GKI mode, LKM mode modifies the `ramdisk`. Therefore, on devices with Android 13, it needs to patch the `init_boot` partition instead of the `boot` partition, while GKI mode always operates on the `boot` partition.

### Use the manager

Open the manager, click the installation icon in the upper right corner, and several options will appear:

1. Select a file. If your device doesn't have root privileges, you can choose this option and then select your official firmware. The manager will automatically patch it. After that, just flash this patched file to obtain root privileges permanently.
2. Direct install. If your device is already rooted, you can choose this option. The manager will automatically get your device information, and then automatically patch the official firmware, and flash it automatically. You can consider using `fastboot boot` KernelSU's GKI kernel to get temporary root and install the manager, and then use this option. This is also the main way to upgrade KernelSU.
3. Install to inactive slot. If your device supports A/B partition, you can choose this option. The manager will automatically patch the official firmware and install it to another partition. This method is suitable for devices after OTA, you can directly install it to another partition after OTA, and then restart the device.

### Use the command line

If you don't want to use the manager, you can also use the command line to install LKM. The `ksud` tool provided by KernelSU can help you quickly patch the official firmware and then flash it.

This tool supports macOS, Linux, and Windows. You can download the corresponding version from [GitHub Release](https://github.com/tiann/KernelSU/releases).

Usage: `ksud boot-patch` you can check the command line help for specific options.

```sh
oriole:/ # ksud boot-patch -h
Patch boot or init_boot images to apply KernelSU

Usage: ksud boot-patch [OPTIONS]

Options:
  -b, --boot <BOOT>              Boot image path. If not specified, it will try to find the boot image automatically
  -k, --kernel <KERNEL>          Kernel image path to be replaced
  -m, --module <MODULE>          LKM module path to be replaced. If not specified, the built-in module will be used
  -i, --init <INIT>              init to be replaced
  -u, --ota                      Will use another slot if the boot image is not specified
  -f, --flash                    Flash it to boot partition after patch
  -o, --out <OUT>                Output path. If not specified, the current directory will be used
      --magiskboot <MAGISKBOOT>  magiskboot path. If not specified, the built-in version will be used
      --kmi <KMI>                KMI version. If specified, the indicated KMI will be used
  -h, --help                     Print help
```

A few options that need to be explained:

1. The `--magiskboot` option can specify the path of magiskboot. If not specified, ksud will look for it in the environment variables. If you don’t know how to get magiskboot, you can check [here](#patch-boot-image).
2. The `--kmi` option can specify the `KMI` version. If the kernel name of your device doesn't follow the KMI specification, you can specify it using this option.

The most common usage is:

```sh
ksud boot-patch -b <boot.img> --kmi android13-5.10
```

## GKI mode installation

There are several installation methods for GKI mode, each suitable for a different scenario, so please choose accordingly:

1. Install with fastboot using the boot.img provided by KernelSU.
2. Install with a kernel flash app, such as [Kernel Flasher](https://github.com/capntrips/KernelFlasher/releases).
3. Repair the boot.img manually and install it.
4. Install with custom Recovery (e.g., TWRP).

## Install with boot.img provided by KernelSU

If your device's `boot.img` uses a commonly used compression format, you can use the GKI images provided by KernelSU to flash it directly. This doesn't require TWRP or self-patching the image.

### Find proper boot.img

KernelSU provides a generic boot.img for GKI devices, and you should flash the boot.img to the device's boot partition.

You can download boot.img from [GitHub Release](https://github.com/tiann/KernelSU/releases). Please note that you should use the correct version of boot.img. If you don't know which file to download, carefully read the description of [KMI](#kmi) and [Security patch level](#security-patch-level) in this document.

Normally, there are three boot files in different formats for the same KMI and security patch level. They're identical except for the kernel compression format. Please check the kernel compression format of your original boot.img. You should use the correct format, such as `lz4`, `gz`. If you use an incorrect compression format, you may encounter bootloop after flashing boot.img.

::: info COMPRESSION FORMAT OF BOOT.IMG
1. You can use magiskboot to get the compression format of your original boot.img. Alternatively, you can also ask members or developers in the community who have the same device model. Also, the compression format of the kernel usually doesn't change, so if you boot successfully with a certain compression format, you can try that format later as well.
2. Xiaomi devices usually use `gz` or `uncompressed`.
3. For Pixel devices, follow the instructions below:
:::

### Flash boot.img to device

Use `adb` to connect your device, then execute `adb reboot bootloader` to enter fastboot mode, and use this command to flash KernelSU:

```sh
fastboot flash boot boot.img
```

::: info
If your device supports `fastboot boot`, you can first use `fastboot boot boot.img` to try to use boot.img to boot the system first. If something unexpected happens, restart it again to boot.
:::

### Reboot

After the flash is completed, you should reboot your device:

```sh
fastboot reboot
```

## Install with Kernel Flasher

Steps:

1. Download the AnyKernel3 ZIP. If you don't know which file to download, carefully read the description of [KMI](#kmi) and [Security patch level](#security-patch-level) in this document.
2. Open the Kernel Flasher app, grant necessary root permissions, and use the provided AnyKernel3 ZIP to flash.

This way requires the Kernel Flasher app to have root permissions. You can use the following methods to achieve this:

1. Your device is rooted. For example, you have installed KernelSU and want to upgrade to the latest version or you have rooted through other methods (such as Magisk).
2. If your device isn't rooted, but the device supports the temporary boot method of `fastboot boot boot.img`, you can use the GKI image provided by KernelSU to temporarily boot your device, obtain temporary root permissions, and then use the Kernel Flash app to obtain permanent root privileges.

Some of kernel flashing apps that can be used for this:

1. [Kernel Flasher](https://github.com/capntrips/KernelFlasher/releases)
2. [Franco Kernel Manager](https://play.google.com/store/apps/details?id=com.franco.kernel)
3. [Ex Kernel Manager](https://play.google.com/store/apps/details?id=flar2.exkernelmanager)

Note: This method is more convenient when upgrading KernelSU and can be done without a computer (make a backup first).

## Patch boot.img manually {#patch-boot-image}

For some devices, the boot.img format isn't as common as `lz4`, `gz`, and `uncompressed`. A typical example is the Pixel, where the boot.img is compressed in the `lz4_legacy` format, while the, ramdisk may be in `gz` or also compressed in `lz4_legacy`. Currently, if you directly flash the boot.img provided by KernelSU, the device may not be able to boot. In this case, you can manually patch the boot.img to achieve this.

It's always recommended to use `magiskboot` to patch images, there are two ways:

1. [magiskboot](https://github.com/topjohnwu/Magisk/releases)
2. [magiskboot_build](https://github.com/ookiineko/magiskboot_build/releases/tag/last-ci)

The official build of `magiskboot` can only run on Android devices, if you want to run it on PC, you can try the second option.

::: tip
Android-Image-Kitchen isn't recommended for now because it doesn't handle the boot metadata (such as security patch level) correctly. Therefore, it may not work on some devices.
:::

### Preparation

1. Get your device's stock boot.img. You can get it from your device manufacturers. You may need [payload-dumper-go](https://github.com/ssut/payload-dumper-go).
2. Download the AnyKernel3 ZIP file provided by KernelSU that matches the KMI version of your device. You can refer to [Install with custom Recovery](#install-with-custom-recovery).
3. Unpack the AnyKernel3 package and get the `Image` file, which is the kernel file of KernelSU.

### Using magiskboot on Android devices {#using-magiskboot-on-Android-devices}

1. Download latest Magisk from [GitHub Releases](https://github.com/topjohnwu/Magisk/releases).
2. Rename `Magisk-*(version).apk` to `Magisk-*.zip` and unzip it.
3. Push `Magisk-*/lib/arm64-v8a/libmagiskboot.so` to your device by ADB: `adb push Magisk-*/lib/arm64-v8a/libmagiskboot.so /data/local/tmp/magiskboot`
4. Push stock boot.img and Image in AnyKernel3 to your device.
5. Enter ADB shell and run `cd /data/local/tmp/` directory, then `chmod +x magiskboot`
6. Enter ADB shell and run `cd /data/local/tmp/` directory, execute `./magiskboot unpack boot.img` to unpack `boot.img`, you will get a `kernel` file, this is your stock kernel.
7. Replace `kernel` with `Image` by running the command: `mv -f Image kernel`.
8. Execute `./magiskboot repack boot.img` to repack boot image, and you will get a `new-boot.img` file, flash this file to device by fastboot.

### Using magiskboot on Windows/macOS/Linux PC {#using-magiskboot-on-PC}

1. Download the corresponding `magiskboot` binary for your OS from [magiskboot_build](https://github.com/ookiineko/magiskboot_build/releases/tag/last-ci).
2. Prepare stock `boot.img` and `Image` in your PC.
3. Run `chmod +x magiskboot`.
4. Enter the corresponding directory, execute `./magiskboot unpack boot.img` to unpack `boot.img`, you will get a `kernel` file, this is your stock kernel.
5. Replace `kernel` with `Image` by running the command: `mv -f Image kernel`.
6. Execute `./magiskboot repack boot.img` to repack the boot image, and you will get a `new-boot.img` file, flash this file to device by fastboot.

::: info
Official `magiskboot` can run in `Linux` environments normally, if you're a Linux user, you can use the official build.
:::

## Install with custom Recovery {#install-with-custom-recovery}

Prerequisite: Your device must have a custom Recovery, such as TWRP. If there is no custom Recovery available for your device, use another method.

Steps:

1. On [GitHub Releases](https://github.com/tiann/KernelSU/releases), download the ZIP package starting with `AnyKernel3` that matches your device's version. For example, if the device's kernel version is `android12-5.10.66`, then you should download the `AnyKernel3-android12-5.10.66_yyyy-MM.zip` file (where `yyyy` is the year and `MM` is the month).
2. Reboot the device into TWRP.
3. Use ADB to place AnyKernel3-*.zip into the device's `/sdcard` location and choose to install it in the TWRP GUI, or you can directly run `adb sideload AnyKernel-*.zip` to install.

Note: This method is suitable for any installation (not limited to initial installation or subsequent upgrades), as long as you're using TWRP.

## Other methods

In fact, all of these installation methods have only one main idea, which is to **replace the original kernel for the one provided by KernelSU**, as long as this can be achieved, it can be installed. The following are other possible methods:

1. First, install Magisk, get root privileges through Magisk, and then use the Kernel Flasher to flash the AnyKernel3 ZIP from KernelSU.
2. Use any flashing toolkit on PC to flash the kernel provided by KernelSU.

However, if it doesn't work, please try `magiskboot` approach.
