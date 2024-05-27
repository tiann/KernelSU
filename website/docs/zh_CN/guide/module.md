# 模块开发指南 {#introduction}

KernelSU 提供了一个模块机制，它可以在保持系统分区完整性的同时达到修改系统分区的效果；这种机制通常被称之为 systemless。

KernelSU 的模块运作机制与 Magisk 几乎是一样的，如果你熟悉 Magisk 模块的开发，那么开发 KernelSU 的模块大同小异，你可以跳过下面有关模块的介绍，只需要了解 [KernelSU 模块与 Magisk 模块的异同](difference-with-magisk.md)。

## 模块界面

KernelSU 的模块支持显示界面并与用户交互，请参阅 [WebUI 文档](module-webui.md)。

## Busybox

KernelSU 提供了一个功能完备的 BusyBox 二进制文件（包括完整的SELinux支持）。可执行文件位于 `/data/adb/ksu/bin/busybox`。
KernelSU 的 BusyBox 支持运行时可切换的 "ASH Standalone Shell Mode"。
这种独立模式意味着在运行 BusyBox 的 ash shell 时，每个命令都会直接使用 BusyBox 中内置的应用程序，而不管 PATH 设置为什么。
例如，`ls`、`rm`、`chmod` 等命令将不会使用 PATH 中设置的命令（在Android的情况下，默认情况下分别为 `/system/bin/ls`、`/system/bin/rm` 和 `/system/bin/chmod`），而是直接调用 BusyBox 内置的应用程序。
这确保了脚本始终在可预测的环境中运行，并始终具有完整的命令套件，无论它运行在哪个Android版本上。
要强制一个命令不使用BusyBox，你必须使用完整路径调用可执行文件。

在 KernelSU 上下文中运行的每个 shell 脚本都将在 BusyBox 的 ash shell 中以独立模式运行。对于第三方开发者相关的内容，包括所有启动脚本和模块安装脚本。

对于想要在 KernelSU 之外使用这个“独立模式”功能的用户，有两种启用方法:

1. 设置环境变量 `ASH_STANDALONE` 为 `1`。例如：`ASH_STANDALONE=1 /data/adb/ksu/bin/busybox sh <script>`
2. 使用命令行选项切换：`/data/adb/ksu/bin/busybox sh -o standalone <script>`

为了确保所有后续的 `sh` shell 都在独立模式下执行，第一种是首选方法（这也是 KernelSU 和 KernelSU 管理器内部使用的方法），因为环境变量会被继承到子进程中。

::: tip 与 Magisk 的差异

KernelSU 的 BusyBox 现在是直接使用 Magisk 项目编译的二进制文件，**感谢 Magisk！**
因此，你完全不用担心 BusyBox 脚本与在 Magisk 和 KernelSU 之间的兼容问题，因为他们是完全一样的！
:::

## KernelSU 模块 {#kernelsu-modules}

KernelSU 模块就是一个放置在 `/data/adb/modules` 内且满足如下结构的文件夹：

```txt
/data/adb/modules
├── .
├── .
|
├── $MODID                  <--- 模块的文件夹名称与模块 ID 相同
│   │
│   │      *** 模块配置文件 ***
│   │
│   ├── module.prop         <--- 此文件保存模块相关的一些配置，如模块 ID、版本等
│   │
│   │      *** 模块内容 ***
│   │
│   ├── system              <--- 这个文件夹通常会被挂载到系统
│   │   ├── ...
│   │   ├── ...
│   │   └── ...
│   │
│   │      *** 标记文件 ***
│   │
│   ├── skip_mount          <--- 如果这个文件存在，那么模块的 `/system` 将不会被挂载
│   ├── disable             <--- 如果这个文件存在，那么模块会被禁用
│   ├── remove              <--- 如果这个文件存在，下次重启的时候模块会被移除
│   │
│   │      *** 可选文件 ***
│   │
│   ├── post-fs-data.sh     <--- 这个脚本将会在 post-fs-data 模式下运行
│   ├── post-mount.sh       <--- 这个脚本将会在 post-mount 模式下运行
│   ├── service.sh          <--- 这个脚本将会在 late_start 服务模式下运行
│   ├── boot-completed.sh   <--- 这个脚本将会在 Android 系统启动完毕后以服务模式运行
|   ├── uninstall.sh        <--- 这个脚本将会在模块被卸载时运行
│   ├── system.prop         <--- 这个文件中指定的属性将会在系统启动时通过 resetprop 更改
│   ├── sepolicy.rule       <--- 这个文件中的 SELinux 策略将会在系统启动时加载
│   │
│   │      *** 自动生成的目录，不要手动创建或者修改！ ***
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

::: tip 与 Magisk 的差异
KernelSU 没有内置的针对 Zygisk 的支持，因此模块中没有 Zygisk 相关的内容，但你可以通过 [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext) 来支持 Zygisk 模块，此时 Zygisk 模块的内容与 Magisk 所支持的 Zygisk 是完全相同的。
:::

### module.prop

module.prop 是一个模块的配置文件，在 KernelSU 中如果模块中不包含此文件，那么它将不被认为是一个模块；此文件的格式如下：

```txt
id=<string>
name=<string>
version=<string>
versionCode=<int>
author=<string>
description=<string>
```

- id 必须与这个正则表达式匹配：`^[a-zA-Z][a-zA-Z0-9._-]+$` 例如：✓ `a_module`，✓ `a.module`，✓ `module-101`，✗ `a  module`，✗ `1_module`，✗ `-a-module`。这是您的模块的唯一标识符，发布后不应更改。
- versionCode 必须是一个整数，用于比较版本。
- 其他未在上面提到的内容可以是任何单行字符串。
- 请确保使用 UNIX（LF）换行类型，而不是Windows（CR + LF）或 Macintosh（CR）。

### Shell 脚本 {#shell-scripts}

请阅读 [启动脚本](#boot-scripts) 一节，以了解 `post-fs-data.sh`, `post-mount.sh`, `service.sh` 和 `boot-completed.sh` 之间的区别。对于大多数模块开发者来说，如果您只需要运行一个启动脚本，`service.sh` 应该已经足够了。

在您的模块的所有脚本中，请使用 `MODDIR=${0%/*}`来获取您的模块的基本目录路径；请勿在脚本中硬编码您的模块路径。

:::tip 与 Magisk 的差异
你可以通过环境变量 `KSU` 来判断脚本是运行在 KernelSU 还是 Magisk 中，如果运行在 KernelSU，这个值会被设置为 `true`。
:::

### `system` 目录 {#system-directories}

这个目录的内容会在系统启动后，以 `overlayfs` 的方式叠加在系统的 `/system` 分区之上，这意味着：

1. 系统中对应目录的同名文件会被此目录的文件覆盖。
2. 系统中对应目录的同名文件夹会与此目录的文件夹合并。

如果你想删掉系统原来目录某个文件或者文件夹，你需要在模块目录通过 `mknod filename c 0 0` 来创建一个 `filename` 的同名文件；这样 overlayfs 系统会自动 whiteout 等效删除此文件（`/system` 分区并没有被更改）。

你也可以在 `customize.sh` 中声明一个名为 `REMOVE` 并且包含一系列目录的变量来执行删除操作，KernelSU 会自动为你在模块对应目录执行 `mknod <TARGET> c 0 0`。例如：

```sh
REMOVE="
/system/app/YouTube
/system/app/Bloatware
"
```

上面的这个列表将会执行： `mknod $MODPATH/system/app/YouTuBe c 0 0` 和 `mknod $MODPATH/system/app/Bloatware c 0 0`；并且 `/system/app/YouTube` 和 `/system/app/Bloatware` 将会在模块生效后被删除。

如果你想替换掉系统的某个目录，你需要在模块目录创建一个相同路径的目录，然后为此目录设置此属性：`setfattr -n trusted.overlay.opaque -v y <TARGET>`；这样 overlayfs 系统会自动将系统内相应目录替换（`/system` 分区并没有被更改）。

你可以在 `customize.sh` 中声明一个名为 `REPLACE` 并且包含一系列目录的变量来执行替换操作，KernelSU 会自动为你在模块对应目录执行相关操作。例如：

```sh
REPLACE="
/system/app/YouTube
/system/app/Bloatware
"
```

上面这个列表将会：自动创建目录 `$MODPATH/system/app/YouTube` 和 `$MODPATH//system/app/Bloatware`，然后执行 `setfattr -n trusted.overlay.opaque -v y $$MODPATH/system/app/YouTube` 和 `setfattr -n trusted.overlay.opaque -v y $$MODPATH/system/app/Bloatware`；并且 `/system/app/YouTube` 和 `/system/app/Bloatware` 将会在模块生效后替换为空目录。

::: tip 与 Magisk 的差异

KernelSU 的 systemless 机制是通过内核的 overlayfs 实现的，而 Magisk 当前则是通过 magic mount (bind mount)，二者实现方式有着巨大的差异，但最终的目标实际上是一致的：不修改物理的 `/system` 分区但实现修改 `/system` 文件。
:::

如果你对 overlayfs 感兴趣，建议阅读 Linux Kernel 关于 [overlayfs 的文档](https://docs.kernel.org/filesystems/overlayfs.html)

### system.prop

这个文件的格式与 `build.prop` 完全相同：每一行都是 `[key]=[value]` 的形式。

### sepolicy.rule

如果您的模块需要一些额外的 SELinux 策略补丁，请将这些规则添加到此文件中。这个文件中的每一行都将被视为一个策略语句。

## 模块安装包 {#module-installer}

KernelSU 的模块安装包就是一个可以通过 KernelSU 管理器 APP 刷入的 zip 文件，此 zip 文件的格式如下：

```txt
module.zip
│
├── customize.sh                       <--- (Optional, more details later)
│                                           This script will be sourced by update-binary
├── ...
├── ...  /* 其他模块文件 */
│
```

:::warning
KernelSU 模块不支持在 Recovery 中安装！！
:::

### 定制安装过程 {#customizing-installation}

如果你想控制模块的安装过程，可以在模块的目录下创建一个名为 `customize.sh` 的文件，这个脚本将会在模块被解压后**导入**到当前 shell 中，如果你的模块需要根据设备的 API 版本或者设备构架做一些额外的操作，那这个脚本将非常有用。

如果你想完全控制脚本的安装过程，你可以在 `customize.sh` 中声明 `SKIPUNZIP=1` 来跳过所有的默认安装步骤；此时，你需要自行处理所有安装过程（如解压模块，设置权限等）

`customize.sh` 脚本以“独立模式”运行在 KernelSU 的 BusyBox `ash` shell 中。你可以使用如下变量和函数：

#### 变量 {#variables}

- `KSU` (bool): 标记此脚本运行在 KernelSU 环境下，此变量的值将永远为 `true`，你可以通过它区分 Magisk。
- `KSU_VER` (string): KernelSU 当前的版本名字 (如： `v0.4.0`)
- `KSU_VER_CODE` (int): KernelSU 用户空间当前的版本号 (如. `10672`)
- `KSU_KERNEL_VER_CODE` (int): KernelSU 内核空间当前的版本号 (如. `10672`)
- `BOOTMODE` (bool): 此变量在 KernelSU 中永远为 `true`
- `MODPATH` (path): 当前模块的安装目录
- `TMPDIR` (path): 可以存放临时文件的目录
- `ZIPFILE` (path): 当前模块的安装包文件
- `ARCH` (string): 设备的 CPU 构架，有如下几种： `arm`, `arm64`, `x86`, or `x64`
- `IS64BIT` (bool): 是否是 64 位设备
- `API` (int): 当前设备的 Android API 版本 (如：Android 6.0 上为 `23`)

::: warning
`MAGISK_VER_CODE` 在 KernelSU 中永远为 `25200`，`MAGISK_VER` 则为 `v25.2`，请不要通过这两个变量来判断是否是 KernelSU！
:::

#### 函数 {#functions}

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

## 启动脚本 {#boot-scripts}

在 KernelSU 中，根据脚本运行模式的不同分为两种：post-fs-data 模式和 late_start 服务模式。

- post-fs-data 模式
  - 这个阶段是阻塞的。在执行完成之前或者 10 秒钟之后，启动过程会暂停。
  - 脚本在任何模块被挂载之前运行。这使得模块开发者可以在模块被挂载之前动态地调整它们的模块。
  - 这个阶段发生在 Zygote 启动之前。
  - 使用 setprop 会导致启动过程死锁！请使用 `resetprop -n <prop_name> <prop_value>` 代替。
  - **只有在必要时才在此模式下运行脚本**。

- late_start 服务模式
  - 这个阶段是非阻塞的。你的脚本会与其余的启动过程**并行**运行。
  - **大多数脚本都建议在这种模式下运行**。

在 KernelSU 中，启动脚本根据存放位置的不同还分为两种：通用脚本和模块脚本。

- 通用脚本
  - 放置在 `/data/adb/post-fs-data.d`, `/data/adb/post-mount.d`, `/data/adb/service.d` 或 `/data/adb/boot-completed.d` 中。
  - 只有在脚本被设置为可执行（`chmod +x script.sh`）时才会被执行。
  - 在 `post-fs-data.d` 中的脚本以 post-fs-data 模式运行，在 `service.d` 中的脚本以 late_start 服务模式运行。
  - 模块**不应**在安装过程中添加通用脚本。

- 模块脚本
  - 放置在模块自己的文件夹中。
  - 只有当模块被启用时才会执行。
  - `post-fs-data.sh` 以 post-fs-data 模式运行，`post-mount.sh` 以 post-mount 模式运行，而 `service.sh` 则以 late_start 服务模式运行，`boot-completed` 在 Android 系统启动完毕后以服务模式运行。

所有启动脚本都将在 KernelSU 的 BusyBox ash shell 中运行，并启用“独立模式”。

### 启动脚本的流程解疑 {#Boot-scripts-process-explanation}

以下是 Android 的相关启动流程（部分省略），其中包括了 KernelSU 的操作（带前导星号），应该能帮助你更好地理解这些启动脚本的用途：

```txt
0. Bootloader (nothing on screen)
load patched boot.img
load kernel:
    - GKI mode: GKI kernel with KernelSU integrated
    - LKM mode: stock kernel
...

1. kernel exec init (oem logo on screen):
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
  *mount tmpfs
  *execute module scripts post-fs-data.sh
    **(Zygisk)./bin/zygisk-ptrace64 monitor
  *(pre)load system.prop (same as resetprop -n)
  *remount modules /system
  *execute general scripts in post-mount.d/
  *execute module scripts post-mount.sh
zygote-start
load_all_props_action
  *execute resetprop (actual set props for resetprop with -n option)
... -> boot
  class_start core
    start-service logd, console, vold, etc.
  class_start main
    start-service adb, netd (iptables), zygote, etc.

2. kernel2user init (rom animation on screen, start by service bootanim)
*execute general scripts in service.d/
*execute module scripts service.sh
*set props for resetprop without -p option
  **(Zygisk) hook zygote (start zygiskd)
  **(Zygisk) mount zygisksu/module.prop
start system apps (autostart)
...
boot complete (broadcast ACTION_BOOT_COMPLETED event)
*execute general scripts in boot-completed.d/
*execute module scripts boot-completed.sh

3. User operable (lock screen)
input password to decrypt /data/data
*actual set props for resetprop with -p option
start user apps (autostart)
```

如果你对 Android 的 init 语言感兴趣，推荐阅读[文档](https://android.googlesource.com/platform/system/core/+/master/init/README.md)。
