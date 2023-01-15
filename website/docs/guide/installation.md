# Installation

## Check if your device is supported

Download the KernelSU manager app from [github releases](https://github.com/tiann/KernelSU/releases) or [github actions](https://github.com/tiann/KernelSU/actions/workflows/build-manager.yml), and then install the app to device and open the app:

- If the app shows `Unsupported`, it means KernelSU is not supported for your device.
- If the app shows `Not installed`, then your devices is supported by KernelSU.

## Find proper boot.img

KernelSU provides a general boot.img for GKI devices, you should flash the boot.img to the boot partition of your device.

You can download the boot.img from [github actions for kernel](https://github.com/tiann/KernelSU/actions/workflows/build-kernel.yml), Please be aware that your should use the right version of boot.img. For example, if your device show that the kernel is `5.10.101`, then you need to download the `5.10.101-xxxx.boot.xxx`.

And also, please check your stock boot.img's format, you should use the right format, such as `lz4`、`gz`.

## Flash the boot.img to device

Connect your device with `adb` and then execute `adb reboot bootloader` to enter fastboot mode, and then use this command to flash KernelSU:

```sh
fastboot flash boot boot.img
```

## Reboot

When flashed, you should reboot your device:

```sh
fastboot reboot
```
