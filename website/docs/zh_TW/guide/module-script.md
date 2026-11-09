# 一次性腳本

一次性腳本是一個由 KernelSU 管理器**執行一次**、而不是安裝的 ZIP。它的打包方式與模組相同，但不會在裝置上留下任何東西：管理器將 ZIP 解壓縮到私有目錄，詢問它宣告的參數，顯示 `action.sh` 的原始碼，然後在 root shell 中執行它。它適合解鎖隱藏設定、匯出日誌之類的工作；需要在開機時執行或修改系統分割區的功能應當做成[模組](module.md)。

## 套件結構

```txt
<id>-<version>.zip
├── module.prop      # 常規 module.prop 欄位 + oneshot=true
├── action.sh        # 必要：管理器執行的腳本
├── params.json      # 選用：執行前詢問的參數
├── customize.sh     # 選用：僅在 ZIP 被當作模組安裝時使用
└── other files      # 腳本透過 $MODPATH 讀取的資料
```

`module.prop` 的欄位與[模組](module.md#module-prop)相同，另外增加 `oneshot`：

```txt
id=example_script
name=Example Script
version=v1
versionCode=1
author=Alice
description=Does a thing once
oneshot=true
```

`oneshot` 的解析方式與 `metamodule` 相同：去除首尾空白後為 `1` 或 `true`（不區分大小寫）即生效；其他值或沒有 `oneshot` 行都表示一般模組。

不要包含 `post-fs-data.sh`、`service.sh`、`system.prop`、`sepolicy.rule` 或 `system/` 目錄等模組專用檔案：一次性執行永遠不會用到它們。一次性腳本不能是 metamodule。

## 執行腳本

有兩種方式執行一次性腳本：

- **從模組倉庫執行**：[KernelSU Modules](https://modules.kernelsu.org) 倉庫中 `kind: script` 的項目會顯示「腳本」標籤，並以 **執行** 按鈕取代 **安裝**。下載的檔案會依倉庫記錄的大小與 SHA-256 校驗，且 `module.prop` 中的 `id` 必須與登記的一致。
- **從本機 ZIP 執行**：從儲存空間選擇單個 ZIP（或從其他應用程式開啟 ZIP）時，如果它的 `module.prop` 含有 `oneshot=true`，將開啟執行頁面而不是安裝程式。頁面上還提供 **仍作為模組安裝**。

執行前，管理器會檢查 `module.prop` 宣告了 `oneshot` 且 `id` 合法、ZIP 根目錄存在 `action.sh`，然後解壓縮 ZIP。絕對路徑、`..`、符號連結和特殊檔案都會被拒絕，解壓縮內容限制為 64 MiB、4096 個項目。

執行頁面顯示參數表單、`action.sh` 原始碼和本裝置上的上一次執行紀錄。確認後，管理器會在全域掛載命名空間中開啟一個新的 root shell 執行腳本，與模組的 Action 按鈕相同：

```sh
cd <dir> && <env> /data/adb/ksu/bin/busybox sh ./action.sh </dev/null
```

輸出的顯示方式與 Action 輸出相同，並且可以儲存。執行結束後解壓縮目錄會被刪除。管理器只記錄上一次執行（版本與結束代碼）；沒有已安裝狀態，也沒有更新檢查。

### 環境變數

| 變數 | 值 |
| --- | --- |
| `ASH_STANDALONE` | `1` |
| `KSU` | `true` |
| `KSU_VER`、`KSU_VER_CODE`、`KSU_KERNEL_VER_CODE` | KernelSU 版本，與模組相同 |
| `KSU_LATE_LOAD` | 僅在 [late-load 模式](module.md#late-load-mode)下為 `1` |
| `KSU_MODULE` | 腳本 ID |
| `KSU_ONESHOT` | `1`，用於區分一次性執行與已安裝模組的 Action 按鈕 |
| `MODPATH` | 解壓縮目錄；從這裡讀取您的資料檔案 |
| `PATH` | `$PATH:/data/adb/ksu/bin` |
| `KSU_PARAM_<KEY>` | 每個參數一個，見 [`params.json`](#params-json) |

標準輸入是 `/dev/null`，腳本無法互動式讀取輸入；請改用參數。

### 結束代碼

| 結束代碼 | 意義 |
| --- | --- |
| `0` | 成功。 |
| `2` | 不適用於此裝置；以提示而非失敗的形式顯示。 |
| 其他 | 失敗。 |

## `params.json` {#params-json}

`params.json` 為選用，位於 ZIP 根目錄，為 UTF-8 JSON，最大 64 KiB。管理器據此產生表單，並將每個值以 `KSU_PARAM_<KEY>` 傳給 `action.sh`。

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

| 欄位 | 規則 |
| --- | --- |
| `schemaVersion` | 必須為 `1`。 |
| `params` | 最多 16 個參數。 |
| `key` | `^[A-Z][A-Z0-9_]{0,31}$`，不可重複。值以 `KSU_PARAM_<key>` 傳給腳本。 |
| `type` | `bool`、`enum`、`string` 或 `int`。 |
| `label` | 必要，1–200 個字元。依原樣顯示（單一語言）。 |
| `description` | 選用，規則同 `label`。 |

各類型：

| 類型 | 欄位 | 傳入形式 |
| --- | --- | --- |
| `bool` | `default`：`true` / `false`（預設 `false`）。 | `true` / `false` |
| `enum` | `options`：1–32 項 `{value, label?}`；`value` 符合 `^[A-Za-z0-9._-]{1,64}$` 且不可重複。`default` 必須是其中一個值（預設為第一項）。 | 選項的 value |
| `string` | `default`（預設 `""`）、`maxLength` 1–1024（預設 256）、`required`（預設 `false`；非必填時允許空值）。值不能包含 NUL 或換行。 | 原樣 |
| `int` | 選用的 `min` / `max` 與 `default`（預設為 `min`，沒有 `min` 時為 `0`）；32 位元有號整數。 | 十進位 |

每個參數的 `default` 都必須滿足自身的限制。`params.json` 無效時，管理器會拒絕執行該腳本。

::: warning 注意
`string` 類型的值沒有格式檢查。每個值都以單引號傳入，因此啟動 `action.sh` 的 shell 不會展開它，但 `action.sh` 仍需自行校驗，並在使用時加上引號。
:::

## 範例

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

## 相容舊版管理器

不支援一次性腳本的管理器會將 ZIP 當作一般模組安裝，其 Action 按鈕會執行 `action.sh`。請確保 `action.sh` 在這種情況下也能運作：此時 `KSU_ONESHOT`、`MODPATH` 與 `KSU_PARAM_*` 都未設定，工作目錄為模組目錄。像上面的範例一樣退回預設值：

```sh
MODPATH=${MODPATH:-${0%/*}}
MODE=${KSU_PARAM_MODE:-a}
```

## 發佈

要在 [KernelSU Modules](https://modules.kernelsu.org) 上發佈一次性腳本，請在 `module.yaml` 中設定 `kind: script`。倉庫規則請參閱 [Script entries](https://github.com/KernelSU-Modules/tools/blob/main/docs/developers/module-yaml.md#script-entries-kind-script)。
