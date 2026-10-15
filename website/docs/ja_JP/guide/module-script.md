# ワンショットスクリプト

ワンショットスクリプトは、KernelSU マネージャーがインストールする代わりに**一度だけ実行する** ZIP です。モジュールと同じ形式でパッケージされますが、デバイスには何も残りません。マネージャーは ZIP をプライベートディレクトリに展開し、宣言されたパラメーターの入力を求め、`action.sh` のソースを表示してから root シェルで実行します。隠し設定の解除やログのエクスポートといった作業に使用してください。起動時に実行する必要があるものや、システムパーティションを変更するものは[モジュール](module.md)にすべきです。

## パッケージ構成

```txt
<id>-<version>.zip
├── module.prop      # the usual module.prop fields + oneshot=true
├── action.sh        # required: the script the manager runs
├── params.json      # optional: parameters asked before running
├── customize.sh     # optional: only used if the ZIP is installed as a module
└── other files      # data the script reads via $MODPATH
```

`module.prop` は[モジュール](module.md#module-prop)と同じフィールドを使用し、`oneshot` を追加します：

```txt
id=example_script
name=Example Script
version=v1
versionCode=1
author=Alice
description=Does a thing once
oneshot=true
```

`oneshot` は `metamodule` と同じ方法で読み取られます。前後の空白を除いた値が `1` または `true`（大文字小文字を区別しない）の場合に有効です。それ以外の値、または `oneshot` 行がない場合は通常のモジュールとして扱われます。

`post-fs-data.sh`、`service.sh`、`system.prop`、`sepolicy.rule`、`system/` ディレクトリなどのモジュール専用ファイルは含めないでください。これらはワンショット実行では一切使用されません。ワンショットスクリプトを metamodule にすることはできません。

## スクリプトの実行

ワンショットスクリプトを実行する方法は 2 つあります：

- **モジュールストアから**：[KernelSU Modules](https://modules.kernelsu.org) レジストリで `kind: script` が指定されたエントリには「Script」バッジが表示され、**インストール**の代わりに **Run** ボタンが表示されます。ダウンロードしたファイルはレジストリのサイズと SHA-256 で検証され、`module.prop` には登録済みの `id` が含まれている必要があります。
- **ローカルの ZIP から**：ストレージから ZIP を 1 つ選択した場合（または他のアプリから開いた場合）、その `module.prop` に `oneshot=true` があれば、インストーラーの代わりに実行ページが開きます。このページでは **Install as module anyway** も選択できます。

実行前に、マネージャーは `module.prop` が有効な `id` とともに `oneshot` を宣言していること、および ZIP のルートに `action.sh` が存在することを確認してから ZIP を展開します。絶対パス、`..`、シンボリックリンク、特殊ファイルは拒否され、展開される内容は 64 MiB および 4096 エントリまでに制限されます。

実行ページには、パラメーターフォーム、`action.sh` のソース、およびこのデバイスでの前回の実行結果が表示されます。確認後、マネージャーはモジュールのアクションボタンと同様に、グローバルマウント名前空間内の新しい root シェルでスクリプトを実行します：

```sh
cd <dir> && <env> /data/adb/ksu/bin/busybox sh ./action.sh </dev/null
```

出力はアクションの出力と同様に表示され、保存することもできます。展開されたディレクトリは実行後に削除されます。マネージャーが記憶するのは前回の実行（バージョンと終了コード）のみで、インストール状態や更新チェックはありません。

### 環境変数

| 変数 | 値 |
| --- | --- |
| `ASH_STANDALONE` | `1` |
| `KSU` | `true` |
| `KSU_VER`, `KSU_VER_CODE`, `KSU_KERNEL_VER_CODE` | KernelSU のバージョン（モジュールと同じ） |
| `KSU_LATE_LOAD` | [late-load モード](module.md#late-load-mode)の場合のみ `1` |
| `KSU_MODULE` | スクリプト ID |
| `KSU_ONESHOT` | `1`。ワンショット実行とインストール済みモジュールのアクションボタンを区別するためのもの |
| `MODPATH` | 展開先ディレクトリ。データファイルはここから読み込みます |
| `PATH` | `$PATH:/data/adb/ksu/bin` |
| `KSU_PARAM_<KEY>` | パラメーターごとに 1 つ。[`params.json`](#params-json) を参照 |

標準入力は `/dev/null` であるため、スクリプトは対話的に入力を読み取ることができません。代わりにパラメーターを宣言してください。

### 終了コード

| 終了コード | 意味 |
| --- | --- |
| `0` | 成功。 |
| `2` | このデバイスには適用されない。失敗ではなく通知として表示されます。 |
| それ以外 | 失敗。 |

## `params.json` {#params-json}

`params.json` は任意です。ZIP のルートに配置し、UTF-8 の JSON で、最大 64 KiB です。マネージャーはこれをもとにフォームを作成し、すべての値を `KSU_PARAM_<KEY>` として `action.sh` に渡します。

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

| フィールド | ルール |
| --- | --- |
| `schemaVersion` | `1` でなければなりません。 |
| `params` | 最大 16 個のパラメーター。 |
| `key` | `^[A-Z][A-Z0-9_]{0,31}$`、一意。値は `KSU_PARAM_<key>` としてスクリプトに渡されます。 |
| `type` | `bool`、`enum`、`string`、`int` のいずれか。 |
| `label` | 必須、1〜200 文字。ラベルは記述されたとおりに表示されます（単一言語）。 |
| `description` | 任意、`label` と同じルール。 |

タイプ別：

| タイプ | フィールド | 渡される値 |
| --- | --- | --- |
| `bool` | `default`：`true` / `false`（既定値 `false`）。 | `true` / `false` |
| `enum` | `options`：1〜32 個のエントリ `{value, label?}`。`value` は `^[A-Za-z0-9._-]{1,64}$` に一致し、一意である必要があります。`default` はいずれかの値でなければなりません（既定値：最初のオプション）。 | オプションの値 |
| `string` | `default`（既定値 `""`）、`maxLength` 1〜1024（既定値 256）、`required`（既定値 `false`。required でない限り空の値も許可されます）。値に NUL や改行を含めることはできません。 | 入力されたとおり |
| `int` | 任意の `min` / `max` と `default`（既定値は `min`、`min` がない場合は `0`）。32 ビット符号付き整数。 | 10 進数 |

すべてのパラメーターの `default` は、そのパラメーター自身の制約を満たす必要があります。`params.json` が無効な場合、マネージャーはスクリプトの実行を拒否します。

::: warning
`string` の値にはフォーマットチェックがありません。すべての値はシングルクォートで囲んで渡されるため、`action.sh` を起動するシェルによって展開されることはありませんが、`action.sh` 側でも値を検証し、使用する際にはクォートする必要があります。
:::

## 例

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

## 古いマネージャーとの互換性

ワンショットに対応していないマネージャーは、ZIP を通常のモジュールとしてインストールし、そのアクションボタンで `action.sh` を実行します。この場合でも `action.sh` が動作するようにしてください。この場合 `KSU_ONESHOT`、`MODPATH`、`KSU_PARAM_*` は設定されず、作業ディレクトリはモジュールディレクトリになります。上記の例のように既定値にフォールバックしてください：

```sh
MODPATH=${MODPATH:-${0%/*}}
MODE=${KSU_PARAM_MODE:-a}
```

## 公開

ワンショットスクリプトを [KernelSU Modules](https://modules.kernelsu.org) で公開するには、`module.yaml` で `kind: script` を設定します。レジストリのルールについては [Script entries](https://github.com/KernelSU-Modules/tools/blob/main/docs/developers/module-yaml.md#script-entries-kind-script) を参照してください。
