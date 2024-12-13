# FAQ

## Does KernelSU support my device?

First, your devices should be able to unlock the bootloader. If it can't, then it is unsupported.

Then install KernelSU manager App to your device and open it, if it shows `Unsupported` then your device cannot be supported out of box, but you can build kernel source and integrate KernelSU to make it work or using [Unofficially supported devices](unofficially-support-devices).

## Does KernelSU need to unlock Bootloader?

Certainly, yes.

## Does KernelSU support modules?

Yes, check [Module guide](module.md) please.

## Does KernelSU support Xposed?

Yes, you can use LSPosed on [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext).

## Does KernelSU support Zygisk?

KernelSU has no builtin Zygisk support, but you can use [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext) instead.

## Is KernelSU compatible with Magisk?

KernelSU's module system is conflict with Magisk's magic mount, if there is any module enabled in KernelSU, then the whole Magisk would not work.

But if you only use the `su` of KernelSU, then it will work well with Magisk: with KernelSU modifying the `kernel` and Magisk modifying the `ramdisk`, both of them can work together simultaneously.

## Will KernelSU substitute Magisk?

We don't think so and it's not our goal. Magisk is good enough for userspace root solution and it will live long. KernelSU's goal is to provide a kernel interface to users, not substituting Magisk.

## Can KernelSU support non GKI devices?

It is possible. But you should download the kernel source and intergrate KernelSU to the source tree and compile the kernel yourself.

## Can KernelSU support devices below Android 12?

It is device's kernel that affect KernelSU's compatability and it has nothing to do with Android version. The only restriction is that devices launched with Android 12 must be kernel of a 5.10+ version (GKI devices). So:

1. Devices launched with Android 12 must be supported.
2. Devices which have an old kernel (Some Android 12 devices is also old kernel) are compatible (You should build kernel yourself).

## Can KernelSU support old kernel?

It is possible, KernelSU is backported to kernel 4.14 now; for older kernel, you need to backport it manually and PRs welcome!

## How to integrate KernelSU for an older kernel?

Please refer to the following [How to integrate KernelSU for non-GKI kernels](how-to-integrate-for-non-gki) guide.

## Why my Android version is 13, and the kernel shows "android12-5.10"?

The Kernel version has nothing to do with Android version, if you need to flash kernel, always use the kernel version, Android version is not so important.

## I am GKI1.0, can i use this?

GKI1 is completely different from GKI2, you must compile kernel by yourself.

## How can I make `/system` RW?

We do not recommend you to modify the system partition directly. You should use the [module](module.md) to modify it systemlessly. If you insist on doing this, check [magisk_overlayfs](https://github.com/HuskyDG/magic_overlayfs).

## Can KernelSU modify hosts? How can I use AdAway？

Of course. But KernelSU doesn't have built-in hosts support, you can install [systemless-hosts](https://github.com/symbuzzer/systemless-hosts-KernelSU-module) to do it.

## Why is there a huge 1 TB file?

The 1 TB size `modules.img` is a disk image file, **don't worry about its size**, it's a special type of file known as a [sparse file](https://en.wikipedia.org/wiki/Sparse_file), it's actual size is only the size of the module you use, and it will dynamically shrink after you delete the module; it does not actually occupy 1 TB of disk space (your mobile phone may not actually have that much space).

If you're really unhappy with the size of this file, you can use the `resize2fs -M` command to make it the actual size; but the module may not work properly at this time, and we won't provide any support for this.

## Why does my device show wrong storage size?

Certain devices use non-standard methods for calculating device's storage size, potentially leading to inaccurate storage calculations in system apps and menus, especially when dealing with 1 TB sparse files. While this problem seems to be specific to Samsung devices, affecting only Samsung apps and services, it's essential to note that the discrepancy is primarily in the total storage size, and the free space calculation remains accurate.
