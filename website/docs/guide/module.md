# Module guide

KernelSU provides a module mechanism that achieves the effect of modifying the system directory while maintaining the integrity of the system partition. This mechanism is commonly known as "systemless".

The module mechanism of KernelSU is almost the same as that of Magisk. If you're familiar with Magisk module development, developing KernelSU modules is very similar. You can skip the introduction of modules below and just read [Difference with Magisk](difference-with-magisk.md).

::: warning METAMODULE ONLY NEEDED FOR SYSTEM FILE MODIFICATION
KernelSU uses a [metamodule](metamodule.md) architecture for mounting the `system` directory. **Only if your module needs to modify `/system` files** (via the `system` directory) do you need to install a metamodule (such as [meta-overlayfs](https://github.com/tiann/KernelSU/releases)). Other module features like scripts, sepolicy rules, and system.prop work without a metamodule.
:::

## WebUI

KernelSU's modules support displaying interfaces and interacting with users. For more details, refer to the [WebUI documentation](module-webui.md).

## Module Configuration

KernelSU provides a built-in configuration system that allows modules to store persistent or temporary key-value settings. Configurations are stored in a binary format at `/data/adb/ksu/module_configs/<module_id>/` with the following characteristics:

### Configuration Types

- **Persist Config** (`persist.config`): Survives reboots and persists until explicitly deleted or the module is uninstalled
- **Temp Config** (`tmp.config`): Automatically cleared during the post-fs-data stage on every boot

When reading configurations, temporary values take priority over persistent values for the same key.

### Using Configuration in Module Scripts

All module scripts (`post-fs-data.sh`, `service.sh`, `boot-completed.sh`, etc.) run with the `KSU_MODULE` environment variable set to the module ID. You can use the `ksud module config` commands to manage your module's configuration:

```bash
# Get a configuration value
value=$(ksud module config get my_setting)

# Set a persistent configuration value
ksud module config set my_setting "some value"

# Set a temporary configuration value (cleared on reboot)
ksud module config set --temp runtime_state "active"

# Set value from stdin (useful for multiline or complex data)
ksud module config set my_key <<EOF
multiline
text value
EOF

# Or pipe from command
echo "value" | ksud module config set my_key

# Explicit stdin flag
cat file.json | ksud module config set json_data --stdin

# List all configuration entries (merged persist + temp)
ksud module config list

# Delete a configuration entry
ksud module config delete my_setting

# Delete a temporary configuration entry
ksud module config delete --temp runtime_state

# Clear all persistent configurations
ksud module config clear

# Clear all temporary configurations
ksud module config clear --temp
```

### Validation Limits

The configuration system enforces the following limits:

- **Maximum key length**: 256 bytes
- **Maximum value length**: 1MB (1048576 bytes)
- **Maximum config entries**: 32 per module
- **Key format**: Must match `^[a-zA-Z][a-zA-Z0-9._-]+$` (same as module ID)
  - Must start with a letter (a-zA-Z)
  - Can contain letters, numbers, dots (`.`), underscores (`_`), or hyphens (`-`)
  - Minimum length: 2 characters
- **Value format**: No restrictions - can contain any UTF-8 characters including newlines, control characters, etc.
  - Stored in binary format with length prefix, ensuring safe handling of all data

### Lifecycle

- **On boot**: All temporary configurations are cleared during the post-fs-data stage
- **On module uninstall**: All configurations (both persist and temp) are removed automatically
- Configurations are stored in a binary format with magic number `0x4b53554d` ("KSUM") and version validation

### Use Cases

The configuration system is ideal for:

- **User preferences**: Store module settings that users configure through WebUI or action scripts
- **Feature flags**: Enable/disable module features without reinstalling
- **Runtime state**: Track temporary state that should reset on reboot (use temp config)
- **Installation settings**: Remember choices made during module installation
- **Complex data**: Store JSON, multiline text, Base64 encoded data, or any structured content (up to 1MB)

::: tip BEST PRACTICES
- Use persistent configs for user preferences that should survive reboots
- Use temporary configs for runtime state or feature toggles that should reset on boot
- Validate configuration values in your scripts before using them
- Use the `ksud module config list` command to debug configuration issues
:::

### Advanced Features

The module configuration system provides special configuration keys for advanced use cases:

#### Overriding Module Description

You can dynamically override the `description` field from `module.prop` by setting the `override.description` configuration key:

```bash
# Override module description
ksud module config set override.description "Custom description shown in the manager"
```

When the module list is retrieved, if the `override.description` config exists, it will replace the original description from `module.prop`. This is useful for:
- Displaying dynamic status information in the module description
- Showing runtime configuration details to users
- Updating description based on module state without reinstalling

#### Declaring Managed Features

Modules can declare which KernelSU features they manage using the `manage.<feature>` configuration pattern. The supported features correspond to KernelSU's internal `FeatureId` enum:

**Supported Features:**
- `su_compat` - SU compatibility mode
- `kernel_umount` - Kernel automatic unmount
- `enhanced_security` - Enhanced security mode

```bash
# Declare that this module manages SU compatibility and enables it
ksud module config set manage.su_compat true

# Declare that this module manages kernel unmount and disables it
ksud module config set manage.kernel_umount false

# Remove feature management (module no longer controls this feature)
ksud module config delete manage.su_compat
```

**How it works:**
- The presence of a `manage.<feature>` key indicates the module is managing that feature
- The value indicates the desired state: `true`/`1` for enabled, `false`/`0` (or any other value) for disabled
- To stop managing a feature, delete the configuration key entirely

Managed features are exposed through the module list API as a `managedFeatures` field (comma-separated string). This allows:
- KernelSU manager to detect which modules manage which KernelSU features
- Prevention of conflicts when multiple modules try to manage the same feature
- Better coordination between modules and core KernelSU functionality

::: warning SUPPORTED FEATURES ONLY
Only use the predefined feature names listed above (`su_compat`, `kernel_umount`, `enhanced_security`). These correspond to actual KernelSU internal features. Using other feature names will not cause errors but serves no functional purpose.
:::

## BusyBox

KernelSU ships with a feature-complete BusyBox binary (including full SELinux support). The executable is located at `/data/adb/ksu/bin/busybox`. KernelSU's BusyBox supports runtime toggle-able "ASH Standalone Shell Mode". What this Standalone Mode means is that when running in the `ash` shell of BusyBox, every single command will directly use the applet within BusyBox, regardless of what is set as `PATH`. For example, commands like `ls`, `rm`, `chmod` will **NOT** use what is in `PATH` (in the case of Android by default it will be `/system/bin/ls`, `/system/bin/rm`, and `/system/bin/chmod` respectively), but will instead directly call internal BusyBox applets. This makes sure that scripts always run in a predictable environment and always have the full suite of commands no matter which Android version it is running on. To force a command _not_ to use BusyBox, you have to call the executable with full paths.

Every single shell script running in the context of KernelSU will be executed in BusyBox's `ash` shell with Standalone Mode enabled. For what is relevant to 3rd party developers, this includes all boot scripts and module installation scripts.

For those who want to use this Standalone Mode feature outside of KernelSU, there are 2 ways to enable it:

1. Set environment variable `ASH_STANDALONE` to `1` <br>Example: `ASH_STANDALONE=1 /data/adb/ksu/bin/busybox sh <script>`
2. Toggle with command-line options:<br>`/data/adb/ksu/bin/busybox sh -o standalone <script>`

To make sure all subsequent `sh` shell executed also runs in Standalone Mode, option 1 is the preferred method (and this is what KernelSU and the KernelSU manager use internally) as environment variables are inherited down to child processes.

::: tip DIFFERENCE WITH MAGISK
KernelSU's BusyBox is now using the binary file compiled directly from the Magisk project. **Thanks to Magisk!** Therefore, you don't need to worry about compatibility issues between BusyBox scripts in Magisk and KernelSU, as they're exactly the same!
:::

## KernelSU modules

A KernelSU module is a folder placed in `/data/adb/modules` with the structure below:

```txt
/data/adb/modules
├── .
├── .
|
├── $MODID                  <--- The folder is named with the ID of the module
│   │
│   │      *** Module Identity ***
│   │
│   ├── module.prop         <--- This file stores the metadata of the module
│   │
│   │      *** Main Contents ***
│   │
│   ├── system              <--- This folder will be mounted if skip_mount does not exist
│   │   ├── ...
│   │   ├── ...
│   │   └── ...
│   │
│   │      *** Status Flags ***
│   │
│   ├── skip_mount          <--- If exists, KernelSU will NOT mount your system folder
│   ├── disable             <--- If exists, the module will be disabled
│   ├── remove              <--- If exists, the module will be removed next reboot
│   │
│   │      *** Optional Files ***
│   │
│   ├── post-fs-data.sh     <--- This script will be executed in post-fs-data
│   ├── post-mount.sh       <--- This script will be executed in post-mount
│   ├── service.sh          <--- This script will be executed in late_start service
│   ├── boot-completed.sh   <--- This script will be executed on boot completed
|   ├── uninstall.sh        <--- This script will be executed when KernelSU removes your module
|   ├── action.sh           <--- This script will be executed when user click the Action button in KernelSU app
│   ├── system.prop         <--- Properties in this file will be loaded as system properties by resetprop
│   ├── sepolicy.rule       <--- Additional custom sepolicy rules
│   │
│   │      *** Auto Generated, DO NOT MANUALLY CREATE OR MODIFY ***
│   │
│   ├── vendor              <--- A symlink to $MODID/system/vendor
│   ├── product             <--- A symlink to $MODID/system/product
│   ├── system_ext          <--- A symlink to $MODID/system/system_ext
│   │
│   │      *** Any additional files / folders are allowed ***
│   │
│   ├── ...
│   └── ...
|
├── another_module
│   ├── .
│   └── .
├── .
├── .
```

::: tip DIFFERENCE WITH MAGISK
KernelSU doesn't have built-in support for Zygisk, so there is no content related to Zygisk in the module. However, you can use [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext) to support Zygisk modules. In this case, the content of the Zygisk module is identical to that supported by Magisk.
:::

### module.prop

`module.prop` is a configuration file for a module. In KernelSU, if a module doesn't contain this file, it won't be recognized as a module. The format of this file is as follows:

```txt
id=<string>
name=<string>
version=<string>
versionCode=<int>
author=<string>
description=<string>
```

- `id` has to match this regular expression: `^[a-zA-Z][a-zA-Z0-9._-]+$`<br>
  Example: ✓ `a_module`, ✓ `a.module`, ✓ `module-101`, ✗ `a module`, ✗ `1_module`, ✗ `-a-module`<br>
  This is the **unique identifier** of your module. You should not change it once published.
- `versionCode` has to be an **integer**. This is used to compare versions.
- Others that were not mentioned above can be any **single line** string.
- Make sure to use the `UNIX (LF)` line break type and not the `Windows (CR+LF)` or `Macintosh (CR)`.

### Shell scripts

Please read the [Boot scripts](#boot-scripts) section to understand the difference between `post-fs-data.sh` and `service.sh`. For most module developers, `service.sh` should be good enough if you just need to run a boot script, if you need to run the script after boot completed, please use `boot-completed.sh`. If you want to do something after mounting OverlayFS, please use `post-mount.sh`.

In all scripts of your module, please use `MODDIR=${0%/*}` to get your module's base directory path; do **NOT** hardcode your module path in scripts.

::: tip DIFFERENCE WITH MAGISK
You can use the environment variable `KSU` to determine if a script is running in KernelSU or Magisk. If running in KernelSU, this value will be set to `true`.
:::

### `system` directory

The contents of this directory will be overlaid on top of the system's `/system` partition after the system is booted. This means that:

::: tip METAMODULE REQUIREMENT
The `system` directory is only mounted if you have a metamodule installed that provides mounting functionality (such as `meta-overlayfs`). The metamodule handles how modules are mounted. See the [Metamodule Guide](metamodule.md) for more information.
:::

1. Files with the same name as those in the corresponding directory in the system will be overwritten by the files in this directory.
2. Folders with the same name as those in the corresponding directory in the system will be merged with the folders in this directory.

If you want to delete a file or folder in the original system directory, you need to create a file with the same name as the file/folder in the module directory using `mknod filename c 0 0`. This way, the OverlayFS system will automatically "whiteout" this file as if it has been deleted (the /system partition isn't actually changed).

You can also declare a variable named `REMOVE` containing a list of directories in `customize.sh` to execute removal operations, and KernelSU will automatically execute `mknod <TARGET> c 0 0` in the corresponding directories of the module. For example:

```sh
REMOVE="
/system/app/YouTube
/system/app/Bloatware
"
```

The above list will execute `mknod $MODPATH/system/app/YouTube c 0 0` and `mknod $MODPATH/system/app/Bloatware c 0 0`, `/system/app/YouTube` and `/system/app/Bloatware` will be removed after the module takes effect.

If you want to replace a directory in the system, you need to create a directory with the same path in your module directory, and then set the attribute `setfattr -n trusted.overlay.opaque -v y <TARGET>` for this directory. This way, the OverlayFS system will automatically replace the corresponding directory in the system (without changing the /system partition).

You can declare a variable named `REPLACE` in your `customize.sh` file, which includes a list of directories to be replaced, and KernelSU will automatically perform the corresponding operations in your module directory. For example:

```sh
REPLACE="
/system/app/YouTube
/system/app/Bloatware
"
```

This list will automatically create the directories `$MODPATH/system/app/YouTube` and `$MODPATH/system/app/Bloatware`, and then execute `setfattr -n trusted.overlay.opaque -v y $MODPATH/system/app/YouTube` and `setfattr -n trusted.overlay.opaque -v y $MODPATH/system/app/Bloatware`. After the module takes effect, `/system/app/YouTube` and `/system/app/Bloatware` will be replaced with empty directories.

::: tip DIFFERENCE WITH MAGISK
KernelSU uses a [metamodule architecture](metamodule.md) where mounting is delegated to pluggable metamodules. The official `meta-overlayfs` metamodule uses the kernel's OverlayFS for systemless modifications, while Magisk uses magic mount (bind mount) built directly into its core. Both achieve the same goal: modifying `/system` files without physically modifying the `/system` partition. KernelSU's approach provides more flexibility and reduces detection surface.
:::

If you're interested in OverlayFS, it's recommended to read the Linux Kernel's [documentation on OverlayFS](https://docs.kernel.org/filesystems/overlayfs.html). For details on KernelSU's metamodule system, see the [Metamodule Guide](metamodule.md).

### system.prop

This file follows the same format as `build.prop`. Each line comprises of `[key]=[value]`.

### sepolicy.rule

If your module requires some additional sepolicy patches, please add those rules into this file. Each line in this file will be treated as a policy statement.

## Module installer

A KernelSU module installer is a KernelSU module packaged in a ZIP file that can be flashed in the KernelSU manager. The simplest KernelSU module installer is just a KernelSU module packed as a ZIP file.

```txt
module.zip
│
├── customize.sh                       <--- (Optional, more details later)
│                                           This script will be sourced by update-binary
├── ...
├── ...  /* The rest of module's files */
│
```

::: warning
KernelSU module is **NOT** compatible for installation in a custom Recovery!
:::

### Customization

If you need to customize the module installation process, optionally you can create a script in the installer named `customize.sh`. This script will be **sourced** (not executed) by the module installer script after all files are extracted and default permissions and secontext are applied. This is very useful if your module requires additional setup based on the device ABI, or you need to set special permissions/secontext for some of your module files.

If you would like to fully control and customize the installation process, declare `SKIPUNZIP=1` in `customize.sh` to skip all default installation steps. By doing so, your `customize.sh` will be responsible to install everything by itself.

The `customize.sh` script runs in KernelSU's BusyBox `ash` shell with Standalone Mode enabled. The following variables and functions are available:

#### Variables

- `KSU` (bool): a variable to mark that the script is running in the KernelSU environment, and the value of this variable will always be true. You can use it to distinguish between KernelSU and Magisk.
- `KSU_VER` (string): the version string of currently installed KernelSU (e.g. `v0.4.0`).
- `KSU_VER_CODE` (int): the version code of currently installed KernelSU in userspace (e.g. `10672`).
- `KSU_KERNEL_VER_CODE` (int): the version code of currently installed KernelSU in kernel space (e.g. `10672`).
- `BOOTMODE` (bool): always be `true` in KernelSU.
- `MODPATH` (path): the path where your module files should be installed.
- `TMPDIR` (path): a place where you can temporarily store files.
- `ZIPFILE` (path): your module's installation ZIP.
- `ARCH` (string): the CPU architecture of the device. Value is either `arm`, `arm64`, `x86`, or `x64`.
- `IS64BIT` (bool): `true` if `$ARCH` is either `arm64` or `x64`.
- `API` (int): the API level (Android version) of the device (e.g., `23` for Android 6.0).

::: warning
In KernelSU, `MAGISK_VER_CODE` is always `25200`, and `MAGISK_VER` is always `v25.2`. Please don't use these two variables to determine whether KernelSU is running or not.
:::

#### Functions

```txt
ui_print <msg>
    print <msg> to console
    Avoid using 'echo' as it will not display in custom recovery's console

abort <msg>
    print error message <msg> to console and terminate the installation
    Avoid using 'exit' as it will skip the termination cleanup steps

set_perm <target> <owner> <group> <permission> [context]
    if [context] is not set, the default is "u:object_r:system_file:s0"
    this function is a shorthand for the following commands:
       chown owner.group target
       chmod permission target
       chcon context target

set_perm_recursive <directory> <owner> <group> <dirpermission> <filepermission> [context]
    if [context] is not set, the default is "u:object_r:system_file:s0"
    for all files in <directory>, it will call:
       set_perm file owner group filepermission context
    for all directories in <directory> (including itself), it will call:
       set_perm dir owner group dirpermission context
```

## Boot scripts

In KernelSU, scripts are divided into two types based on their running mode: post-fs-data mode and late_start service mode.

- post-fs-data mode
  - This stage is BLOCKING. The boot process is paused before execution is done or after 10 seconds.
  - Scripts run before any modules are mounted. This allows a module developer to dynamically adjust their modules before it gets mounted.
  - This stage happens before Zygote is started, which pretty much means everything in Android.
  - **WARNING:** Using `setprop` will deadlock the boot process! Please use `resetprop -n <prop_name> <prop_value>` instead.
  - **Only run scripts in this mode if necessary**.
- late_start service mode
  - This stage is NON-BLOCKING. Your script runs in parallel with the rest of the booting process.
  - **This is the recommended stage to run most scripts**.

In KernelSU, startup scripts are divided into two types based on their storage location: general scripts and module scripts.

- General scripts
  - Placed in `/data/adb/post-fs-data.d`, `/data/adb/service.d`, `/data/adb/post-mount.d` or `/data/adb/boot-completed.d`.
  - Only executed if the script is set as executable (`chmod +x script.sh`).
  - Scripts in `post-fs-data.d` runs in post-fs-data mode, and scripts in `service.d` runs in late_start service mode.
  - Modules should **NOT** add general scripts during installation.
- Module scripts
  - Placed in the module's own folder.
  - Only executed if the module is enabled.
  - `post-fs-data.sh` runs in post-fs-data mode, `service.sh` runs in late_start service mode, `boot-completed.sh` runs on boot completed, `post-mount.sh` runs on OverlayFS mounted.

All boot scripts will run in KernelSU's BusyBox `ash` shell with Standalone Mode enabled.

### Boot scripts process explanation

The following is the relevant boot process for Android (some parts are omitted), which includes the operation of KernelSU (with leading asterisks), and can help you better understand the purpose of these module scripts:

```txt
0. Bootloader (nothing on screen)
load patched boot.img
load kernel:
    - GKI mode: GKI kernel with KernelSU integrated
    - LKM mode: stock kernel
...

1. kernel exec init (OEM logo on screen):
    - GKI mode: stock init
    - LKM mode: exec ksuinit, insmod kernelsu.ko, exec stock init
mount /dev, /dev/pts, /proc, /sys, etc.
property-init -> read default props
read init.rc
...
early-init -> init -> late_init
early-fs
   start vold
fs
  mount /vendor, /system, /persist, etc.
post-fs-data
  *safe mode check
  *execute general scripts in post-fs-data.d/
  *load sepolicy.rule
  *execute metamodule's post-fs-data.sh (if exists)
  *execute module scripts post-fs-data.sh
    **(Zygisk)./bin/zygisk-ptrace64 monitor
  *(pre)load system.prop (same as resetprop -n)
  *execute metamodule's metamount.sh (mounts all modules)
  *execute general scripts in post-mount.d/
  *execute metamodule's post-mount.sh (if exists)
  *execute module scripts post-mount.sh
zygote-start
load_all_props_action
  *execute resetprop (actual set props for resetprop with -n option)
... -> boot
  class_start core
    start-service logd, console, vold, etc.
  class_start main
    start-service adb, netd (iptables), zygote, etc.

2. kernel2user init (ROM animation on screen, start by service bootanim)
*execute general scripts in service.d/
*execute metamodule's service.sh (if exists)
*execute module scripts service.sh
*set props for resetprop without -p option
  **(Zygisk) hook zygote (start zygiskd)
  **(Zygisk) mount zygisksu/module.prop
start system apps (autostart)
...
boot complete (broadcast ACTION_BOOT_COMPLETED event)
*execute general scripts in boot-completed.d/
*execute metamodule's boot-completed.sh (if exists)
*execute module scripts boot-completed.sh

3. User operable (lock screen)
input password to decrypt /data/data
*actual set props for resetprop with -p option
start user apps (autostart)
```

If you're interested in Android Init Language, it's recommended to read its [documentation](https://android.googlesource.com/platform/system/core/+/master/init/README.md).
