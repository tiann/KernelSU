# 模块配置

KernelSU 提供了一个内置的配置系统,允许模块存储持久化或临时的键值设置。配置以二进制格式存储在 `/data/adb/ksu/module_configs/<module_id>/`,具有以下特性:

## 配置类型

- **持久配置** (`persist.config`):重启后保留,直到明确删除或卸载模块
- **临时配置** (`tmp.config`):在每次启动时的 post-fs-data 阶段自动清除

读取配置时,对于同一个键,临时值优先于持久值。

## 在模块脚本中使用配置

所有模块脚本(`post-fs-data.sh`、`service.sh`、`boot-completed.sh` 等)运行时都会设置 `KSU_MODULE` 环境变量为模块 ID。您可以使用 `ksud module config` 命令来管理模块的配置:

```bash
# 获取配置值
value=$(ksud module config get my_setting)

# 设置持久配置值
ksud module config set my_setting "some value"

# 设置临时配置值(重启后清除)
ksud module config set --temp runtime_state "active"

# 从 stdin 设置值(适用于多行或复杂数据)
ksud module config set my_key <<EOF
多行
文本值
EOF

# 或从命令管道输入
echo "value" | ksud module config set my_key

# 显式使用 stdin 标志
cat file.json | ksud module config set json_data --stdin

# 列出所有配置项(合并持久和临时配置)
ksud module config list

# 删除配置项
ksud module config delete my_setting

# 删除临时配置项
ksud module config delete --temp runtime_state

# 清除所有持久配置
ksud module config clear

# 清除所有临时配置
ksud module config clear --temp
```

## 验证限制

配置系统强制执行以下限制:

- **最大键长度**:256 字节
- **最大值长度**:1MB (1048576 字节)
- **最大配置项数**:每个模块 32 个
- **键格式**:必须匹配 `^[a-zA-Z][a-zA-Z0-9._-]+$`(与模块 ID 相同)
  - 必须以字母(a-zA-Z)开头
  - 可包含字母、数字、点(`.`)、下划线(`_`)或连字符(`-`)
  - 最小长度:2 个字符
- **值格式**:无限制 - 可包含任何 UTF-8 字符,包括换行符、控制字符等
  - 以二进制格式存储,带长度前缀,确保安全处理所有数据

## 生命周期

- **启动时**:所有临时配置在 post-fs-data 阶段清除
- **模块卸载时**:所有配置(持久和临时)自动删除
- 配置以二进制格式存储,使用魔数 `0x4b53554d`("KSUM")和版本验证

## 使用场景

配置系统适用于:

- **用户偏好**:存储用户通过 WebUI 或 action 脚本配置的模块设置
- **功能开关**:在不重新安装的情况下启用/禁用模块功能
- **运行时状态**:跟踪应在重启时重置的临时状态(使用临时配置)
- **安装设置**:记住模块安装时做出的选择
- **复杂数据**:存储 JSON、多行文本、Base64 编码数据或任何结构化内容(最多 1MB)

::: tip 最佳实践
- 对于应在重启后保留的用户偏好,使用持久配置
- 对于应在启动时重置的运行时状态或功能开关,使用临时配置
- 在脚本中使用配置值之前验证它们
- 使用 `ksud module config list` 命令调试配置问题
:::

## 高级功能

模块配置系统提供了用于高级用例的特殊配置键:

### 覆盖模块描述 {#overriding-module-description}

您可以通过设置 `override.description` 配置键来动态覆盖 `module.prop` 中的 `description` 字段:

```bash
# 覆盖模块描述
ksud module config set override.description "在管理器中显示的自定义描述"
```

当获取模块列表时,如果存在 `override.description` 配置,它将替换 `module.prop` 中的原始描述。这对于以下场景很有用:
- 在模块描述中显示动态状态信息
- 向用户显示运行时配置详情
- 基于模块状态更新描述而无需重新安装

### 声明管理的功能

模块可以使用 `manage.<feature>` 配置模式声明它们管理的 KernelSU 功能。支持的功能对应于 KernelSU 内部的 `FeatureId` 枚举:

**支持的功能:**
- `su_compat` - SU 兼容模式
- `kernel_umount` - 内核自动卸载

```bash
# 声明此模块管理 SU 兼容性并将其启用
ksud module config set manage.su_compat true

# 声明此模块管理内核卸载并将其禁用
ksud module config set manage.kernel_umount false

# 移除功能管理(模块不再控制此功能)
ksud module config delete manage.su_compat
```

**工作原理:**
- `manage.<feature>` 键的存在表示模块正在管理该功能
- 值表示期望的状态:`true`/`1` 代表启用,`false`/`0`(或任何其他值)代表禁用
- 要停止管理某个功能,请完全删除该配置键

管理的功能通过模块列表 API 以 `managedFeatures` 字段(逗号分隔的字符串)公开。这允许:
- KernelSU 管理器检测哪些模块管理哪些 KernelSU 功能
- 防止多个模块尝试管理同一功能时发生冲突
- 更好地协调模块与核心 KernelSU 功能之间的关系

::: warning 仅支持预定义功能
仅使用上面列出的预定义功能名称(`su_compat`、`kernel_umount`)。这些对应于实际的 KernelSU 内部功能。使用其他功能名称不会导致错误,但没有任何功能作用。
:::
