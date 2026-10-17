# 一次性脚本

一次性脚本是一个由 KernelSU 管理器**运行一次**、而不是安装的 ZIP。它的打包方式与模块相同，但不会在设备上留下任何东西：管理器把 ZIP 解压到私有目录，询问它声明的参数，展示 `action.sh` 的源码，然后在 root shell 中运行它。它适合解锁隐藏设置、导出日志之类的任务；需要在开机时运行或修改系统分区的功能应当做成[模块](module.md)。

## 包结构

```txt
<id>-<version>.zip
├── module.prop      # 常规 module.prop 字段 + oneshot=true
├── action.sh        # 必需：管理器运行的脚本
├── params.json      # 可选：运行前询问的参数
├── customize.sh     # 可选：仅在 ZIP 被当作模块安装时使用
└── other files      # 脚本通过 $MODPATH 读取的数据
```

`module.prop` 的字段与[模块](module.md#module-prop)相同，另外增加 `oneshot`：

```txt
id=example_script
name=Example Script
version=v1
versionCode=1
author=Alice
description=Does a thing once
oneshot=true
```

`oneshot` 的解析方式与 `metamodule` 相同：去掉首尾空白后为 `1` 或 `true`（不区分大小写）即生效；其他值或没有 `oneshot` 行都表示普通模块。

不要包含 `post-fs-data.sh`、`service.sh`、`system.prop`、`sepolicy.rule` 或 `system/` 目录等模块专用文件：一次性运行永远不会用到它们。一次性脚本不能是元模块。

## 运行脚本

有两种方式运行一次性脚本：

- **从模块仓库运行**：[KernelSU Modules](https://modules.kernelsu.org) 仓库中 `kind: script` 的条目会显示「脚本」标签，并用 **运行** 按钮代替 **安装**。下载的文件会按仓库记录的大小和 SHA-256 校验，且 `module.prop` 中的 `id` 必须与登记的一致。
- **从本地 ZIP 运行**：从存储中选择单个 ZIP（或从其他应用打开 ZIP）时，如果它的 `module.prop` 含有 `oneshot=true`，将打开运行页面而不是安装器。页面上还提供 **仍作为模块安装**。

运行前，管理器会检查 `module.prop` 声明了 `oneshot` 且 `id` 合法、ZIP 根目录存在 `action.sh`，然后解压 ZIP。绝对路径、`..`、符号链接和特殊文件都会被拒绝，解压内容限制为 64 MiB、4096 个条目。

运行页面展示参数表单、`action.sh` 源码和本设备上的上一次运行记录。确认后，管理器在全局挂载命名空间中新开一个 root shell 运行脚本，与模块的 Action 按钮相同：

```sh
cd <dir> && <env> /data/adb/ksu/bin/busybox sh ./action.sh </dev/null
```

输出的展示方式与 Action 输出相同，并且可以保存。运行结束后解压目录会被删除。管理器只记录上一次运行（版本和退出码）；没有已安装状态，也没有更新检查。

### 环境变量

| 变量 | 值 |
| --- | --- |
| `ASH_STANDALONE` | `1` |
| `KSU` | `true` |
| `KSU_VER`、`KSU_VER_CODE`、`KSU_KERNEL_VER_CODE` | KernelSU 版本，与模块相同 |
| `KSU_LATE_LOAD` | 仅在 [late-load 模式](module.md#late-load-mode)下为 `1` |
| `KSU_MODULE` | 脚本 ID |
| `KSU_ONESHOT` | `1`，用于区分一次性运行和已安装模块的 Action 按钮 |
| `MODPATH` | 解压目录；从这里读取你的数据文件 |
| `PATH` | `$PATH:/data/adb/ksu/bin` |
| `KSU_PARAM_<KEY>` | 每个参数一个，见 [`params.json`](#params-json) |

标准输入是 `/dev/null`，脚本无法交互式读取输入；请改用参数。

### 退出码

| 退出码 | 含义 |
| --- | --- |
| `0` | 成功。 |
| `2` | 不适用于此设备；以提示而非失败的形式展示。 |
| 其他 | 失败。 |

## `params.json` {#params-json}

`params.json` 是可选的，位于 ZIP 根目录，为 UTF-8 JSON，最大 64 KiB。管理器据此生成表单，并把每个值以 `KSU_PARAM_<KEY>` 传给 `action.sh`。

```json
{
  "schemaVersion": 1,
  "params": [
    {
      "key": "ENABLE_FASTBOOT",
      "type": "bool",
      "label": "Also enable fastboot",
      "description": "Optional longer help text",
      "default": true
    },
    { "key": "MODE", "type": "enum", "label": "Mode", "default": "a",
      "options": [ { "value": "a", "label": "A" }, { "value": "b" } ] },
    { "key": "NAME", "type": "string", "label": "Name", "default": "", "maxLength": 64,
      "required": false },
    { "key": "COUNT", "type": "int", "label": "Count", "default": 1, "min": 0, "max": 10 }
  ]
}
```

| 字段 | 规则 |
| --- | --- |
| `schemaVersion` | 必须为 `1`。 |
| `params` | 最多 16 个参数。 |
| `key` | `^[A-Z][A-Z0-9_]{0,31}$`，不可重复。值以 `KSU_PARAM_<key>` 传给脚本。 |
| `type` | `bool`、`enum`、`string` 或 `int`。 |
| `label` | 必需，1–200 个字符。按原样显示（单语言）。 |
| `description` | 可选，规则同 `label`。 |

各类型：

| 类型 | 字段 | 传入形式 |
| --- | --- | --- |
| `bool` | `default`：`true` / `false`（默认 `false`）。 | `true` / `false` |
| `enum` | `options`：1–32 项 `{value, label?}`；`value` 匹配 `^[A-Za-z0-9._-]{1,64}$` 且不可重复。`default` 必须是其中一个值（默认为第一项）。 | 选项的 value |
| `string` | `default`（默认 `""`）、`maxLength` 1–1024（默认 256）、`required`（默认 `false`；非必填时允许空值）。值不能包含 NUL 或换行。 | 原样 |
| `int` | 可选的 `min` / `max` 和 `default`（默认为 `min`，没有 `min` 时为 `0`）；32 位有符号整数。 | 十进制 |

每个参数的 `default` 都必须满足自身的约束。`params.json` 无效时，管理器会拒绝运行该脚本。

::: warning 注意
`string` 类型的值没有格式校验。每个值都以单引号传入，所以启动 `action.sh` 的 shell 不会展开它，但 `action.sh` 仍需自行校验，并在使用时加引号。
:::

## 示例

```sh
#!/system/bin/sh
# action.sh
MODPATH=${MODPATH:-${0%/*}}

if [ "$(getprop ro.miui.ui.version.name)" = "" ]; then
  echo "Not a MIUI device"
  exit 2
fi

echo "Mode: ${KSU_PARAM_MODE:-a}"
if [ "$KSU_PARAM_ENABLE_FASTBOOT" = "true" ]; then
  sh "$MODPATH/fastboot.sh" || exit 1
fi
echo "Done"
```

## 兼容旧版管理器

不支持一次性脚本的管理器会把 ZIP 当作普通模块安装，其 Action 按钮会运行 `action.sh`。请保证 `action.sh` 在这种情况下也能工作：此时 `KSU_ONESHOT`、`MODPATH` 和 `KSU_PARAM_*` 都未设置，工作目录为模块目录。像上面的示例一样回退到默认值：

```sh
MODPATH=${MODPATH:-${0%/*}}
MODE=${KSU_PARAM_MODE:-a}
```

## 发布

要在 [KernelSU Modules](https://modules.kernelsu.org) 上发布一次性脚本，请在 `module.yaml` 中设置 `kind: script`。仓库规则见 [Script entries](https://github.com/KernelSU-Modules/tools/blob/main/docs/developers/module-yaml.md#script-entries-kind-script)。
