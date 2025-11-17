# 元模块

元模块是 KernelSU 的一项革新性功能，它将关键的模块系统能力从核心转移到可插拔模块中。这种架构转变在保持 KernelSU 稳定性和安全性的同时，为模块生态系统释放了更大的创新潜力。

## 什么是元模块?

元模块是一种特殊类型的 KernelSU 模块，为模块系统提供核心基础设施功能。与常规模块不同，元模块控制常规模块的*安装和挂载方式*。

元模块是一种基于插件的扩展机制，允许完全自定义 KernelSU 的模块管理基础设施。通过将挂载和安装逻辑委托给元模块，KernelSU 避免成为脆弱的检测点，同时支持多样化的实现策略。

**主要特征:**

- **基础设施角色**: 元模块提供常规模块依赖的服务
- **单实例**: 只允许一个元模块处于运行状态
- **优先执行**: 元模块脚本在常规模块脚本之前运行
- **特殊钩子**: 提供三个用于安装、挂载和清理的钩子脚本

## 为什么需要元模块?

传统的 Root 解决方案将挂载逻辑内置在核心中，这使得它们更容易被检测且难以演进。KernelSU 的元模块架构通过关注点分离解决了这些问题。

**战略优势:**

- **减少检测面**: KernelSU 本身不执行挂载，减少了检测向量
- **稳定性**: 核心保持稳定，而挂载实现可以不断演进
- **创新性**: 社区可以开发替代挂载策略
- **选择性**: 用户可以选择最适合其需求的实现

**挂载灵活性:**

- **无挂载**: 对于仅使用无挂载模块的用户，完全避免挂载开销
- **OverlayFS 挂载**: 传统方法，支持读写层(通过 `meta-overlayfs`)
- **Magic Mount**: Magisk 兼容挂载，以获得更好的应用兼容性
- **自定义实现**: 基于 FUSE 的 overlayfs、自定义 VFS 挂载或全新方法

**超越挂载:**

- **可扩展性**: 添加内核模块支持等功能，无需修改核心 KernelSU
- **模块化**: 独立于 KernelSU 版本更新实现
- **定制化**: 为特定设备或用例创建专门的解决方案

::: warning 重要
如果没有安装元模块，依赖挂载的模块，其挂载功能将不会生效。新安装的 KernelSU 需要安装元模块(如 `meta-overlayfs`)才能使模块正常工作。
:::

## 对于用户

### 安装元模块

像安装常规模块一样安装元模块:

1. 下载元模块 ZIP 文件(例如 `meta-overlayfs.zip`)
2. 打开 KernelSU Manager 应用
3. 点击浮动操作按钮(➕)
4. 选择元模块 ZIP 文件
5. 重启设备

`meta-overlayfs` 元模块是官方参考实现，提供传统的基于 overlayfs 的模块挂载，支持读写系统分区。

### 检查活动的元模块

您可以在 KernelSU Manager 应用的模块页面中查看当前活动的元模块。活动的元模块将显示在模块列表中，并带有特殊标识。

### 卸载元模块

::: danger 警告
卸载元模块会影响**所有**模块。移除后，模块将不再被挂载，直到您安装另一个元模块。
:::

卸载步骤:

1. 打开 KernelSU Manager
2. 在模块列表中找到元模块
3. 点击卸载(您会看到特殊警告)
4. 确认操作
5. 重启设备

卸载后，如果您需要元模块的功能，应该安装另一个元模块。

### 单元模块约束

只允许一个元模块处于运行状态。如果您尝试安装第二个元模块，KernelSU 将阻止安装以避免冲突。

切换元模块的步骤:

1. 卸载所有常规模块
2. 卸载当前元模块
3. 重启
4. 安装新元模块
5. 重新安装常规模块
6. 再次重启

## 对于模块开发者

如果您正在开发常规 KernelSU 模块，您不需要太担心元模块。只要用户安装了兼容的元模块(如 `meta-overlayfs`)，您的模块就能正常工作。

**您需要知道的:**

- **挂载需要元模块**: 模块中的 `system` 目录只有在用户安装了提供挂载功能的元模块时才会被挂载
- **无需更改代码**: 现有模块无需修改即可继续工作

::: tip
如果您熟悉 Magisk 模块开发，您的模块在安装元模块后将在 KernelSU 中以相同方式工作，因为它提供了 Magisk 兼容的挂载。
:::

## 对于元模块开发者

创建元模块允许您自定义 KernelSU 处理模块安装、挂载和卸载的方式。

### 基本要求

元模块通过 `module.prop` 中的特殊属性来识别:

```txt
id=meta-example
name=My Custom Metamodule
version=1.0
versionCode=1
author=Your Name
description=Custom module mounting implementation
metamodule=1
```

`metamodule=1`(或 `metamodule=true`)属性将此模块标记为元模块。没有此属性，模块将被视为常规模块。

### 文件结构

元模块结构:

```txt
meta-example/
├── module.prop              (必须包含 metamodule=1)
│
│      *** 元模块特定钩子 ***
├── metamount.sh             (可选: 自定义挂载处理程序)
├── metainstall.sh           (可选: 常规模块的安装钩子)
├── metauninstall.sh         (可选: 常规模块的清理钩子)
│
│      *** 标准模块文件(全部可选) ***
├── customize.sh             (安装自定义)
├── post-fs-data.sh          (post-fs-data 阶段脚本)
├── service.sh               (late_start service 脚本)
├── boot-completed.sh        (启动完成脚本)
├── uninstall.sh             (元模块自己的卸载脚本)
└── [任何其他文件]
```

除了特殊的元模块钩子外，元模块可以使用所有标准模块功能(生命周期脚本等)。

### 钩子脚本

元模块可以提供最多三个特殊钩子脚本:

#### 1. metamount.sh - 挂载处理程序

**目的**: 控制启动期间模块的挂载方式。

**执行时机**: 参考 [执行顺序](#execution-order)

**环境变量:**

- `MODDIR`: 元模块的目录路径(例如 `/data/adb/modules/meta-example`)
- 所有标准 KernelSU 环境变量

**职责:**

- 以无系统方式挂载所有已启用的模块
- 检查 `skip_mount` 标志
- 处理特定模块的挂载要求

::: danger 关键要求
执行挂载操作时，**必须**将源/设备名称设置为 `"KSU"`。这将挂载标识为属于 KernelSU。

**示例(正确):**

```sh
mount -t overlay -o lowerdir=/lower，upperdir=/upper，workdir=/work KSU /target
```

**对于现代挂载 API**，设置源字符串:

```rust
fsconfig_set_string(fs， "source"， "KSU")?;
```

这对于 KernelSU 正确识别和管理其挂载至关重要。
:::

**示例脚本:**

```sh
#!/system/bin/sh
MODDIR="${0%/*}"

# 示例: 简单的绑定挂载实现
for module in /data/adb/modules/*; do
    if [ -f "$module/disable" ] || [ -f "$module/skip_mount" ]; then
        continue
    fi

    if [ -d "$module/system" ]; then
        # 使用 source=KSU 挂载(必需!)
        mount -o bind，dev=KSU "$module/system" /system
    fi
done
```

#### 2. metainstall.sh - 安装钩子

**目的**: 自定义常规模块的安装方式。

**执行时机**: 在模块安装期间，文件提取后但安装完成前。此脚本被内置安装程序**source**(而非执行)，类似于 `customize.sh` 的工作方式。

**环境变量和函数:**

此脚本继承内置 `install.sh` 的所有变量和函数:

- **变量**: `MODPATH`、`TMPDIR`、`ZIPFILE`、`ARCH`、`API`、`IS64BIT`、`KSU`、`KSU_VER`、`KSU_VER_CODE`、`BOOTMODE` 等
- **函数**:
  - `ui_print <msg>` - 向控制台打印消息
  - `abort <msg>` - 打印错误并终止安装
  - `set_perm <target> <owner> <group> <permission> [context]` - 设置文件权限
  - `set_perm_recursive <directory> <owner> <group> <dirpermission> <filepermission> [context]` - 递归设置权限
  - `install_module` - 调用内置模块安装过程

**用例:**

- 在内置安装之前或之后处理模块文件(准备好后调用 `install_module`)
- 移动模块文件
- 验证模块兼容性
- 设置特殊目录结构
- 初始化模块特定资源

**注意**: 安装元模块本身时**不会**调用此脚本。

#### 3. metauninstall.sh - 清理钩子

**目的**: 卸载常规模块时清理资源。

**执行时机**: 在模块卸载期间，在删除模块目录之前。

**环境变量:**

- `MODULE_ID`: 正在卸载的模块的 ID

**用例:**

- 处理文件
- 清理符号链接
- 释放分配的资源
- 更新内部跟踪

**示例脚本:**

```sh
#!/system/bin/sh
# 卸载常规模块时调用
MODULE_ID="$1"
IMG_MNT="/data/adb/metamodule/mnt"

# 从镜像中删除模块文件
if [ -d "$IMG_MNT/$MODULE_ID" ]; then
    rm -rf "$IMG_MNT/$MODULE_ID"
fi
```

### 执行顺序 {#execution-order}

了解启动执行顺序对于元模块开发至关重要:

```txt
post-fs-data 阶段:
  1. 执行通用 post-fs-data.d 脚本
  2. restorecon，加载 sepolicy.rule
  3. 执行元模块的 post-fs-data.sh(如果存在)
  4. 执行常规模块的 post-fs-data.sh
  5. 加载 system.prop
  6. 执行元模块的 metamount.sh
     └─> 以无系统方式挂载所有模块
  7. post-mount.d 阶段运行
     - 通用 post-mount.d 脚本
     - 元模块的 post-mount.sh(如果存在)
     - 常规模块的 post-mount.sh

service 阶段:
  1. 执行通用 service.d 脚本
  2. 执行元模块的 service.sh(如果存在)
  3. 执行常规模块的 service.sh

boot-completed 阶段:
  1. 执行通用 boot-completed.d 脚本
  2. 执行元模块的 boot-completed.sh(如果存在)
  3. 执行常规模块的 boot-completed.sh
```

**要点:**

- `metamount.sh` 在所有 post-fs-data 脚本(元模块和常规模块)**之后**运行
- 元模块生命周期脚本(`post-fs-data.sh`、`service.sh`、`boot-completed.sh`)始终在常规模块脚本之前运行
- `.d` 目录中的通用脚本在元模块脚本之前运行
- `post-mount` 阶段在挂载完成后运行

### 符号链接机制

当安装元模块时，KernelSU 会创建一个符号链接:

```sh
/data/adb/metamodule -> /data/adb/modules/<metamodule_id>
```

这为访问活动元模块提供了稳定的路径，无论其 ID 如何。

**好处:**

- 一致的访问路径
- 轻松检测活动元模块
- 简化配置

### 真实示例: meta-overlayfs

`meta-overlayfs` 元模块是官方参考实现。它展示了元模块开发的最佳实践。

#### 架构

`meta-overlayfs` 使用**双目录架构**:

1. **元数据目录**: `/data/adb/modules/`
   - 包含 `module.prop`、`disable`、`skip_mount` 标记
   - 启动期间快速扫描
   - 存储占用小

2. **内容目录**: `/data/adb/metamodule/mnt/`
   - 包含实际模块文件(system、vendor、product 等)
   - 存储在 ext4 镜像(`modules.img`)中
   - 使用 ext4 功能优化空间

#### metamount.sh 实现

以下是 `meta-overlayfs` 如何实现挂载处理程序:

```sh
#!/system/bin/sh
MODDIR="${0%/*}"
IMG_FILE="$MODDIR/modules.img"
MNT_DIR="$MODDIR/mnt"

# 如果尚未挂载，则挂载 ext4 镜像
if ! mountpoint -q "$MNT_DIR"; then
    mkdir -p "$MNT_DIR"
    mount -t ext4 -o loop，rw，noatime "$IMG_FILE" "$MNT_DIR"
fi

# 为双目录支持设置环境变量
export MODULE_METADATA_DIR="/data/adb/modules"
export MODULE_CONTENT_DIR="$MNT_DIR"

# 执行挂载二进制文件
# (实际挂载逻辑在 Rust 二进制文件中)
"$MODDIR/meta-overlayfs"
```

#### 主要特性

**Overlayfs 挂载:**

- 使用内核 overlayfs 进行真正的无系统修改
- 支持多个分区(system、vendor、product、system_ext、odm、oem)
- 通过 `/data/adb/modules/.rw/` 支持读写层

**源标识:**

```rust
// 来自 meta-overlayfs/src/mount.rs
fsconfig_set_string(fs， "source"， "KSU")?;  // 必需!
```

这为所有 overlay 挂载设置 `dev=KSU`，实现正确识别。

### 最佳实践

开发元模块时:

1. **始终将源设置为"KSU"**以进行挂载操作 - 内核卸载和 zygisksu 卸载需要此设置才能正确卸载
2. **优雅地处理错误** - 启动过程对时间敏感
3. **尊重标准标志** - 支持 `skip_mount` 和 `disable`
4. **记录操作** - 使用 `echo` 或日志记录进行调试
5. **彻底测试** - 挂载错误可能导致启动循环
6. **记录行为** - 清楚地解释您的元模块做什么
7. **提供迁移路径** - 帮助用户从其他解决方案切换

### 测试您的元模块

发布前:

1. 在干净的 KernelSU 设置上**测试安装**
2. **验证挂载**各种模块类型
3. **检查兼容性**与常见模块
4. **测试卸载**和清理
5. **验证启动性能**(metamount.sh 是阻塞的!)
6. **确保正确的错误处理**以避免启动循环

## 常见问题

### 我需要元模块吗?

**对于用户**: 仅当您想使用需要挂载的模块时。如果您只使用运行脚本而不修改系统文件的模块，则不需要元模块。

**对于模块开发者**: 不需要，您正常开发模块。仅当您的模块需要挂载时，用户才需要元模块。

**对于高级用户**: 仅当您想自定义挂载行为或创建替代挂载实现时。

### 我可以有多个元模块吗?

不可以。一次只能安装一个元模块。这可以防止冲突并确保可预测的行为。

### 如果我卸载了唯一的元模块会怎样?

模块将不再被挂载。您的设备将正常启动，但模块修改将不会应用，直到您安装另一个元模块。

### meta-overlayfs 是必需的吗?

不是。它提供与大多数模块兼容的标准 overlayfs 挂载。如果您需要不同的行为，可以创建自己的元模块。

## 另请参阅

- [模块指南](module.md) - 通用模块开发
- [与 Magisk 的区别](difference-with-magisk.md) - 比较 KernelSU 和 Magisk
- [如何构建](how-to-build.md) - 从源代码构建 KernelSU
