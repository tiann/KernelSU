# One-shot scripts

A one-shot script is a ZIP that the KernelSU manager **runs once** instead of installing. It is packaged like a module, but nothing stays on the device: the manager extracts the ZIP to a private directory, asks for the parameters it declares, shows the `action.sh` source and runs it in a root shell. Use it for tasks such as unlocking a hidden setting or exporting a log; anything that must run at boot or modify the system partitions should be a [module](module.md).

## Package layout

```txt
<id>-<version>.zip
├── module.prop      # the usual module.prop fields + oneshot=true
├── action.sh        # required: the script the manager runs
├── params.json      # optional: parameters asked before running
├── customize.sh     # optional: only used if the ZIP is installed as a module
└── other files      # data the script reads via $MODPATH
```

`module.prop` uses the same fields as a [module](module.md#module-prop) and adds `oneshot`:

```txt
id=example_script
name=Example Script
version=v1
versionCode=1
author=Alice
description=Does a thing once
oneshot=true
```

`oneshot` is read like `metamodule`: the value, ignoring surrounding whitespace, is `1` or `true` (case-insensitive). Anything else, or no `oneshot` line, means a regular module.

Do not ship module-only files such as `post-fs-data.sh`, `service.sh`, `system.prop`, `sepolicy.rule` or a `system/` directory: they are never used by a one-shot run. A one-shot script cannot be a metamodule.

## Running a script

There are two ways to run a one-shot script:

- **From the module store**: entries of the [KernelSU Modules](https://modules.kernelsu.org) registry with `kind: script` show a "Script" badge and a **Run** button instead of **Install**. The download is checked against the registry's size and SHA-256, and `module.prop` must carry the registered `id`.
- **From a local ZIP**: if you pick a single ZIP from storage (or open one from another app) and its `module.prop` has `oneshot=true`, the run page opens instead of the installer. The page also offers **Install as module anyway**.

Before running, the manager checks that `module.prop` declares `oneshot` with a valid `id` and that `action.sh` exists at the ZIP root, then extracts the ZIP. Absolute paths, `..`, symlinks and special files are rejected, and the extracted contents are limited to 64 MiB and 4096 entries.

The run page shows the parameter form, the `action.sh` source and the last run on this device. After a confirmation, the manager runs the script in a new root shell in the global mount namespace, like a module's Action button:

```sh
cd <dir> && <env> /data/adb/ksu/bin/busybox sh ./action.sh </dev/null
```

The output is shown like the Action output and can be saved. The extracted directory is removed afterwards. The manager only remembers the last run (version and exit code); there is no installed state and no update check.

### Environment variables

| Variable | Value |
| --- | --- |
| `ASH_STANDALONE` | `1` |
| `KSU` | `true` |
| `KSU_VER`, `KSU_VER_CODE`, `KSU_KERNEL_VER_CODE` | KernelSU versions, as for modules |
| `KSU_LATE_LOAD` | `1` in [late-load mode](module.md#late-load-mode) only |
| `KSU_MODULE` | the script ID |
| `KSU_ONESHOT` | `1`, to tell a one-shot run from the Action button of an installed module |
| `MODPATH` | the extraction directory; read your data files from here |
| `PATH` | `$PATH:/data/adb/ksu/bin` |
| `KSU_PARAM_<KEY>` | one per parameter, see [`params.json`](#params-json) |

Stdin is `/dev/null`, so the script cannot read input interactively; declare parameters instead.

### Exit codes

| Exit code | Meaning |
| --- | --- |
| `0` | Success. |
| `2` | Not applicable to this device; shown as a notice, not as a failure. |
| anything else | Failure. |

## `params.json` {#params-json}

`params.json` is optional. It lives at the ZIP root, is UTF-8 JSON and is at most 64 KiB. The manager builds a form from it and passes every value to `action.sh` as `KSU_PARAM_<KEY>`.

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

| Field | Rule |
| --- | --- |
| `schemaVersion` | Must be `1`. |
| `params` | Up to 16 parameters. |
| `key` | `^[A-Z][A-Z0-9_]{0,31}$`, unique. The value reaches the script as `KSU_PARAM_<key>`. |
| `type` | `bool`, `enum`, `string` or `int`. |
| `label` | Required, 1–200 characters. Labels are shown as written (single-language). |
| `description` | Optional, same rules as `label`. |

Per type:

| Type | Fields | Passed as |
| --- | --- | --- |
| `bool` | `default`: `true` / `false` (default `false`). | `true` / `false` |
| `enum` | `options`: 1–32 entries `{value, label?}`; `value` matches `^[A-Za-z0-9._-]{1,64}$` and is unique. `default` must be one of the values (default: the first option). | the option value |
| `string` | `default` (default `""`), `maxLength` 1–1024 (default 256), `required` (default `false`; an empty value is allowed unless required). Values cannot contain NUL or line breaks. | as entered |
| `int` | Optional `min` / `max` and `default` (default `min`, or `0` without `min`); 32-bit signed integers. | decimal |

The `default` of every parameter must satisfy its own constraints. If `params.json` is invalid, the manager refuses to run the script.

::: warning
There is no format check for `string` values. Every value is passed single-quoted, so it is never expanded by the shell that starts `action.sh`, but `action.sh` must still validate it and quote it when using it.
:::

## Example

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

## Compatibility with older managers

Managers without one-shot support install the ZIP as a regular module whose Action button runs `action.sh`. Keep `action.sh` working in that case too: there `KSU_ONESHOT`, `MODPATH` and `KSU_PARAM_*` are not set and the working directory is the module directory. Fall back to defaults, as in the example above:

```sh
MODPATH=${MODPATH:-${0%/*}}
MODE=${KSU_PARAM_MODE:-a}
```

## Publishing

To publish a one-shot script on [KernelSU Modules](https://modules.kernelsu.org), set `kind: script` in `module.yaml`. See [Script entries](https://github.com/KernelSU-Modules/tools/blob/main/docs/developers/module-yaml.md#script-entries-kind-script) for the registry rules.
