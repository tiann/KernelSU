# Rescue from bootloop

When updating a device, situations may arise where the device becomes "bricked". In theory, if you only use fastboot to flash the boot partition or install incompatible modules that cause the device to fail during boot, it can be restored through appropriate operations. This document aims to provide emergency methods to help you recover a "bricked" device.

## Brick by flashing boot partition

In KernelSU, the following situations may cause boot brick when flashing the boot partition:

1. You flashed a boot image in the wrong format. For example, if your device's boot format is `gz`, but you flashed an image in `lz4` format, the device won't boot.
2. Your device needs to disable AVB verification to boot correctly, which usually requires wiping all data from the device.
3. Your kernel contains bugs or isn't compatible with your device's flash.

No matter the situation, you can recover by **flashing the stock boot image**. Therefore, at the beginning of the installation guide, we strongly recommend that you back up your stock boot before flashing. If you didn't back it up, you can obtain the original factory boot from other users with the same device or from the official firmware.

## Brick by modules

The installation of modules can be one of the most common causes of bricking your device, but we must seriously warn you: **DO NOT INSTALL MODULES FROM UNKNOWN SOURCES!** Since modules have root privileges, they can cause irreversible damage to your device!

### Normal modules

If you have flashed a module that has been proven to be safe but causes your device to fail to boot, then this situation is easily recoverable in KernelSU without any worries. KernelSU has built-in mechanisms to rescue your device, including the following:

1. AB update
2. Rescue by pressing Volume down button

#### AB update

KernelSU's module updates are based on the Android system's AB update mechanism used in OTA updates. When you install a new module or update an existing one, it won't directly modify the currently used module file. Instead, all modules are integrated into a new update image. After the system is restarted, it will attempt to boot using this new update image. If the Android system boots successfully, the modules will be effectively updated.

Therefore, the simplest and most commonly used method to rescue your device is to **force a reboot**. If you're unable to start your system after flashing a module, you can press and hold the power button for more than 10 seconds, and the system will automatically reboot. After rebooting, it will roll back to the state before the module update, and any previously updated modules will be automatically disabled.

#### Rescue by pressing Volume down button

If AB update hasn't yet resolved the issue, you can try using **Safe Mode**. In this mode, all modules are disabled.

There are two ways to enter Safe Mode:

1. The built-in Safe Mode of some systems: Some systems have a built-in Safe Mode that can be accessed by long-pressing the Volume down button. In other systems (such as HyperOS), Safe Mode can be activated from the Recovery. When entering the system's Safe Mode, KernelSU will also enter this mode and automatically disable the modules.
2. The built-in Safe Mode of KernelSU: In this case, the method is to **press the Volume down key continuously more than three times** after the first boot screen.

After entering Safe Mode, all modules on the Module page in the KernelSU manager will be disabled. However, you can still perform the "uninstall" operation to remove any modules that may be causing issues.

The built-in Safe Mode is implemented in the kernel, so there is no possibility of missing important events due to interception. However, for non-GKI kernels, manual code integration may be required. For this, refer to the official documentation for guidance.

### Malicious modules

If the above methods cannot rescue your device, it's highly likely that the module you installed has malicious operations or has damaged your device in some other way. In this case, there are only two suggestions:

1. Wipe the data and flash the official system.
2. Consult the after-sales service.
