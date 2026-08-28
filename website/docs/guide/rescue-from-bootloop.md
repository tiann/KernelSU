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

#### Rescue by pressing Volume down button {#volume-down}

You can try using **Safe Mode** to rescue your device. After entering Safe Mode, all modules are disabled.

There are two ways to enter Safe Mode:

1. The built-in Safe Mode of some systems: Some systems have a built-in Safe Mode that can be accessed by long-pressing the Volume down button. In other systems (such as MIUI/HyperOS), Safe Mode can be activated from the Recovery. When entering the system's Safe Mode, KernelSU will also enter this mode and automatically disable the modules.
2. The built-in Safe Mode of KernelSU: In this case, the method is to **press the Volume down key continuously more than three times** after the first boot screen. Note that it is press-release, press-release, press-release, not hold.

After entering Safe Mode, all modules on the Module page in the KernelSU manager will be disabled. However, you can still perform the "uninstall" operation to remove any modules that may be causing issues.

The built-in Safe Mode is implemented in the kernel, so there is no possibility of missing important events due to interception. However, for non-GKI kernels, manual code integration may be required. For this, refer to the official documentation for guidance.

::: warning
KernelSU registers volume key listener during kernel module initialization (loaded when the kernel executes the init process in LKM mode), and unregisters it at the `on_post_fs_data` stage (before the boot animation). You need to grasp the timing and quickly press the volume down key three times after the first boot screen. If the device boots fast or the operation is not timely, the safe mode may not be triggered.

If the module writes unreasonable code in initrc causing the device to fail to boot, this code will still be executed even in safe mode.
:::

#### Manual Rescue {#manual-rescue}

When safe mode cannot solve the problem, you can try manual rescue. Choose the following methods according to the device status.

**Method 1: Use ksud to manage modules via ADB**

If the device can get root shell via ADB, you can use the `ksud` command line directly to disable or uninstall the problematic module:

::: tip
After mounting `metadata` and `data` partitions, you can run `/data/adb/ksud` command under Recovery mode to manage modules.

Since GKI devices share `init`, KernelSU kernel module will still be loaded under Recovery mode, you should be able to use most features of `ksud` (like setting features) normally.
:::

```
adb shell
su
ksud module list          # List all modules
ksud module disable <id>  # Disable problematic module
ksud module uninstall <id> # Or uninstall directly
reboot
```

**Method 2: Manually clean up through Recovery**

If you cannot enter the system (even ADB cannot be connected), you need a third-party Recovery (such as TWRP) on the device.

The module loading of KernelSU depends on the init.rc injection file on the kernel side and the ksud process in user space. After deleting these files and rebooting, KernelSU will not load any modules.

**Operating steps:**

1. Enter Recovery (such as TWRP).
2. Mount the data partition:
   ```
   mount /data
   ```
   (You may need to decrypt the data partition first. The specific operation depends on the device and decryption method.)
3. Delete ksud to prevent module loading:
   ```
   rm -f /data/adb/ksud
   ```
4. (Optional) Mount the metadata partition and delete the init.rc injection file generated by the module:
   ```
   mount /metadata
   rm -f /metadata/ksu/modules.rc
   rm -f /metadata/watchdog/ksu/modules.rc
   ```
5. Reboot the device:
   ```
   reboot
   ```

KernelSU will skip the loading of all modules after rebooting. After entering the system, you can reopen the KernelSU manager to deal with the module problems.

### Format data or other malicious modules

If the above methods cannot rescue your device, it's highly likely that the module you installed has malicious operations or has damaged your device in some other way. In this case, there are only two suggestions:

1. Wipe the data and flash the official system completely.
2. Consult the after-sales service.
