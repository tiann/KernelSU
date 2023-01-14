# FAQ

## Does KernelSU support my device?

First, your devices should be able to unlock the bootloader. If it can't, then it is unsupported.

Then install KernelSU manager App to your device and open it, if it shows `Unsupported` then your device is unsupported and won't be supported in the future.

## Does KernelSU need to unlock Bootloader?

Certainly, yes.

## Does KernelSU support modules?

Yes, But it is in early version, may be buggy. Please waiting it to be stable :)

## Does KernelSU support Xposed?

Yes, [Dreamland](https://github.com/canyie/Dreamland) and [TaiChi](https::/taichi.cool) partially works now, And we are trying to make other Xposed Framework work.

## Is KernelSU compatible with Magisk?

KernelSU's module system is conflict with Magisk's magic mount, if there is any module enabled in KernelSU, then the whole Magisk would not work.

But if you only use the `su` of KernelSU, then it will work well with Magisk: KernelSU modify the `kernel` and Magisk modify the `ramdisk`, they can work together.

## Will KernelSU substitute Magisk?

We don't think so and it's not our goal. Magisk is good enough for userspace root solution and it will live long. KernelSU's goal is to provide a kernel interface to users, not substituting Magisk.

## Can KernelSU support non GKI devices?

It is possible. But you should download the kernel source and intergrate KernelSU to the source tree and compile the kernel yourself.

## Can KernelSU support old kernel?

It is possible, but you need to backport it manully and PRs welcome!