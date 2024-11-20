# Difference with Magisk

Although there are many similarities between KernelSU modules and Magisk modules, there are inevitably some differences due to their completely different implementation mechanisms. If you want your module to run on both Magisk and KernelSU, you must understand these differences.

## Similarities

- Module file format: both use zip format to organize modules, and the format of modules is almost the same.
- Module installation directory: both located in `/data/adb/modules`.
- Systemless: both support modifying `/system` in a systemless way through modules.
- post-fs-data.sh: the execution time and semantics are exactly the same.
- service.sh: the execution time and semantics are exactly the same.
- system.prop: completely the same.
- sepolicy.rule: completely the same.
- BusyBox: scripts are run in BusyBox with "Standalone Mode" enabled in both cases.

## Differences

Before understanding the differences, you need to know how to differentiate whether your module is running in KernelSU or Magisk. You can use the environment variable `KSU` to differentiate it in all places where you can run module scripts (`customize.sh`, `post-fs-data.sh`, `service.sh`). In KernelSU, this environment variable will be set to `true`.

Here are some differences:

- KernelSU modules cannot be installed in Recovery mode.
- KernelSU modules do not have built-in support for Zygisk (but you can use Zygisk modules through [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext)).
- The method for replacing or deleting files in KernelSU modules is completely different from Magisk. KernelSU does not support the `.replace` method. Instead, you need to create a same-named file with `mknod filename c 0 0` to delete the corresponding file.
- The directories for BusyBox are different. The built-in BusyBox in KernelSU is located in `/data/adb/ksu/bin/busybox`, while in Magisk it is in `/data/adb/magisk/busybox`. **Note that this is an internal behavior of KernelSU and may change in the future!**
- KernelSU does not support `.replace` files; however, KernelSU supports the `REMOVE` and `REPLACE` variable to remove or replace files and folders.
- KernelSU adds `boot-completed` stage to run some scripts on boot completed.
- KernelSU adds `post-mount` stage to run some scripts after mounting OverlayFS.
