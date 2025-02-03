# Rescue from bootloop

When flashing a device, we may encounter situations when the device becomes "bricked". In theory, if you only use fastboot to flash the boot partition or install incompatible modules that cause the device to fail to boot, then this can be restored by appropriate operations. This document aims to provide some emergency methods to help you recover from a "bricked" state.

## Brick by flashing boot partition

In KernelSU, the following situations may cause boot brick when flashing the boot partition:

1. You flashed a boot image in the wrong format. For example, if your device's boot format is `gz`, but you flashed an `lz4` format image, then the device will not be able to boot.
2. Your device needs to disable AVB verification in order to boot properly (usually requiring wiping all data on the device).
3. Your kernel has some bugs or is not suitable for your device to flash.

No matter what the situation is, you can recover by **flashing the stock boot image**. Therefore, at the beginning of the installation tutorial, we strongly recommend that you back up your stock boot before flashing. If you have not backed up, you can obtain the original factory boot from other users with the same device as you or from the official firmware.

## Brick by modules

Installing modules can be a more common cause of bricking your device, but we must seriously warn you: **Do not install modules from unknown sources**! Since modules have root privileges, they can potentially cause irreversible damage to your device!

### Normal modules

If you have flashed a module that has been proven to be safe but causes your device to fail to boot, then this situation is easily recoverable in KernelSU without any worries. KernelSU has built-in mechanisms to rescue your device, including the following:

1. AB update
2. Rescue by pressing Volume Down

#### AB update

KernelSU's module updates draw inspiration from the Android system's AB update mechanism used in OTA updates. If you install a new module or update an existing one, it will not directly modify the currently used module file. Instead, all modules will be built into another update image. After the system is restarted, it will attempt to start using this update image. If the Android system successfully boots up, the modules will then be truly updated.

Therefore, the simplest and most commonly used method to rescue your device is to **force a reboot**. If you are unable to start your system after flashing a module, you can press and hold the power button for more than 10 seconds, and the system will automatically reboot; after rebooting, it will roll back to the state before updating the module, and the previously updated modules will be automatically disabled.

#### Rescue by pressing Volume Down

If AB updates still cannot solve the problem, you can try using **Safe Mode**. In Safe Mode, all modules are disabled.

There are two ways to enter Safe Mode:

1. The built-in Safe Mode of some systems; some systems have a built-in Safe Mode that can be accessed by long-pressing the volume down button, while others (such as MIUI) can enable Safe Mode in Recovery. When entering the system's Safe Mode, KernelSU will also enter Safe Mode and automatically disable modules.
2. The built-in Safe Mode of KernelSU; the operation method is to **press the volume down key continuously for more than three times** after the first boot screen. Note that it is press-release, press-release, press-release, not press and hold.

After entering safe mode, all modules on the module page of the KernelSU Manager are disabled, but you can perform "uninstall" operations to uninstall any modules that may be causing issues.

The built-in safe mode is implemented in the kernel, so there is no possibility of missing key events due to interception. However, for non-GKI kernels, manual integration of the code may be required, and you can refer to the official documentation for guidance.

### Malicious modules

If the above methods cannot rescue your device, it is very likely that the module you installed has malicious operations or has damaged your device through other means. In this case, there are only two suggestions:

1. Wipe the data and flash the official system.
2. Consult the after-sales service.
