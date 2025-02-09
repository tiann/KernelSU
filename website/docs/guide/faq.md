# FAQ

## Does KernelSU support my device?

First, your devices should be able to unlock the bootloader. If not, then there is unsupported.

Next, install the KernelSU manager on your device and open it. If it shows `Unsupported`, then your device cannot be supported immediately. However, you can build kernel source and integrate KernelSU to make it work, or use [Unofficially supported devices](unofficially-support-devices).

## Does KernelSU need to unlock bootloader?

Certainly, yes.

## Does KernelSU support modules?

Yes, check [Module guide](module.md).

## Does KernelSU support Xposed?

Yes, you can use LSPosed with [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext).

## Does KernelSU support Zygisk?

KernelSU has no built-in Zygisk support, but you can use [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext).

## Is KernelSU compatible with Magisk?

KernelSU's module system conflicts with Magisk's magic mount. If any module is enabled in KernelSU, Magisk will stop working entirely.

However, if you only use the `su` of KernelSU, it will work well with Magisk. KernelSU modifies the `kernel`, while Magisk modifies the `ramdisk`, allowing both to work together.

## Will KernelSU substitute Magisk?

We believe that it isn't, and that isn't our goal. Magisk is good enough for userspace root solution and will have a long life. KernelSU's goal is to provide a kernel interface to users, not substituting Magisk.

## Can KernelSU support non-GKI devices?

It's possible. But you should download the kernel source and intergrate KernelSU into the source tree, and compile the kernel yourself.

## Can KernelSU support devices below Android 12?

It's the device's kernel that affects KernelSU's compatibility, and it has nothing to do with the Android version. The only restriction is that devices launched with Android 12 must have a kernel version of 5.10+ (GKI devices). So:

1. Devices launched with Android 12 must be supported.
2. Devices with an older kernel (some devices with Android 12 also have the older kernel) are compatible (you should build kernel yourself).

## Can KernelSU support old kernel?

It's possible. KernelSU is backported to kernel 4.14 now. For older kernels, you need to backport it manually, and PRs are always welcome!

## How to integrate KernelSU for an older kernel?

Please check the [Intergrate for non-GKI devices](how-to-integrate-for-non-gki) guide.

## Why my Android version is 13, and the kernel shows "android12-5.10"?

The kernel version has nothing to do with the Android version. If you need to flash kernel, always use the kernel version; the Android version isn't as important.

## I'm GKI 1.0, can I use this?

GKI 1.0 is completely different from GKI 2.0, you must compile kernel by yourself.

## How can I make `/system` RW?

We don't recommend that you modify the system partition directly. Please check [Module guide](module.md) to modify it systemlessly. If you insist on doing this, check [magisk_overlayfs](https://github.com/HuskyDG/magic_overlayfs).

## Can KernelSU modify hosts? How can I use AdAway？

Of course. But KernelSU doesn't have built-in hosts support, you can install [systemless-hosts](https://github.com/symbuzzer/systemless-hosts-KernelSU-module) to do it.

## Why is there a huge 1 TB file?

The 1 TB `modules.img` file is a disk image file. **Don't worry about its size**; it's a special type of file known as a [sparse file](https://en.wikipedia.org/wiki/Sparse_file). Its actual size is only the size of the module you use, and it will decrease dynamically after you delete the module. In fact, it doesn't occupy 1 TB of disk space (your device might not even have that much space).

If you really care about the size of this file, you can use the `resize2fs -M` command to make it the actual size. However, the module may not work correctly in this case, and we won't provide any support for this.

## Why does my device show wrong storage size?

Certain devices use non-standard methods to calculate the device's storage size, which may lead to inaccurate storage calculations in system apps and menus, especially when dealing with 1 TB sparse files. Although this problem seems to be specific to Samsung devices, affecting only Samsung apps and services, it's important to note that the discrepancy mainly concerns the total storage size, and the free space calculation remains accurate.
