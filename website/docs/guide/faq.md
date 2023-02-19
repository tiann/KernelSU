# FAQ

## Does KernelSU support my device?

First, your devices should be able to unlock the bootloader. If it can't, then it is unsupported.

Then install KernelSU manager App to your device and open it, if it shows `Unsupported` then your device is unsupported and won't be supported in the future.

## Does KernelSU need to unlock Bootloader?

Certainly, yes.

## Does KernelSU support modules?

Yes, But it is in early version, it may be buggy. Please wait for it to be stable :)

## Does KernelSU support Xposed?

Yes, [Dreamland](https://github.com/canyie/Dreamland) and [TaiChi](https::/taichi.cool) partially works now. For LSPosed, you can follow [Zygisk on KernelSU](https://github.com/Dr-TSNG/ZygiskOnKernelSU)

## Is KernelSU compatible with Magisk?

KernelSU's module system is conflict with Magisk's magic mount, if there is any module enabled in KernelSU, then the whole Magisk would not work.

But if you only use the `su` of KernelSU, then it will work well with Magisk: KernelSU modify the `kernel` and Magisk modify the `ramdisk`, they can work together.

## Will KernelSU substitute Magisk?

We don't think so and it's not our goal. Magisk is good enough for userspace root solution and it will live long. KernelSU's goal is to provide a kernel interface to users, not substituting Magisk.

## Can KernelSU support non GKI devices?

It is possible. But you should download the kernel source and intergrate KernelSU to the source tree and compile the kernel yourself.

## Can KernelSU support devices below Android 12?

It is device's kernel that affect KernelSU's compatability and it has nothing to do with Android version.The only restriction is that devices launched with Android 12 must be kernel 5.10+(GKI devices). So:

1. Devices launched with Android 12 must be supported.
2. Devices with has an old kernel (Some Android 12 devices is also old kernel) is compatable (You should build kernel yourself)

## Can KernelSU support old kernel?

It is possible, KernelSU is backported to kernel 4.14 now, for older kernel, you need to backport it manully and PRs welcome!

## How to integrate KernelSU for old kernel?

Please refer [guide](how-to-integrate-for-non-gki)

## Why my Android version is 13, and the kernel shows "android12-5.10"?

The Kernel version has nothing to do with Android version, if you need to flash kernel, always use the kernel version, Android version is not so important.

## Is there any --mount-master/global mount namespace in KernelSU?

There isn't now(maybe in the future), But you can use `nsenter -t 1 -m sh` to enter global mount namespace instead.

## Does KernelSU support Zygisk?

KernelSU has no builtin Zygisk support, but you can use [Zygisk on KernelSU](https://github.com/Dr-TSNG/ZygiskOnKernelSU) instead.

## I am GKI1.0, can i use this?

GKI1 is completely different from GKI2, you must compile kernel by yourself.
