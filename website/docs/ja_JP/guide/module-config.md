# モジュール設定

KernelSU は、モジュールが永続的または一時的なキー値設定を保存できる組み込みの設定システムを提供します。設定は `/data/adb/ksu/module_configs/<module_id>/` にバイナリ形式で保存され、以下の特性があります:

## 設定タイプ

- **永続設定** (`persist.config`):再起動後も保持され、明示的に削除またはモジュールをアンインストールするまで残ります
- **一時設定** (`tmp.config`):起動時の post-fs-data ステージで自動的にクリアされます

設定を読み取るとき、同じキーに対して一時値が永続値より優先されます。

## モジュールスクリプトでの設定の使用

すべてのモジュールスクリプト(`post-fs-data.sh`、`service.sh`、`boot-completed.sh` など)は、`KSU_MODULE` 環境変数がモジュール ID に設定された状態で実行されます。`ksud module config` コマンドを使用してモジュールの設定を管理できます:

```bash
# 設定値を取得
value=$(ksud module config get my_setting)

# 永続設定値を設定
ksud module config set my_setting "some value"

# 一時設定値を設定(再起動後にクリア)
ksud module config set --temp runtime_state "active"

# stdinから値を設定(複数行や複雑なデータに便利)
ksud module config set my_key <<EOF
複数行
テキスト値
EOF

# またはコマンドからパイプ
echo "value" | ksud module config set my_key

# 明示的なstdinフラグ
cat file.json | ksud module config set json_data --stdin

# すべての設定エントリを一覧表示(永続と一時をマージ)
ksud module config list

# 設定エントリを削除
ksud module config delete my_setting

# 一時設定エントリを削除
ksud module config delete --temp runtime_state

# すべての永続設定をクリア
ksud module config clear

# すべての一時設定をクリア
ksud module config clear --temp
```

## 検証制限

設定システムは以下の制限を強制します:

- **最大キー長**:256 バイト
- **最大値長**:1MB (1048576 バイト)
- **最大設定エントリ数**:モジュールあたり 32 個
- **キー形式**:`^[a-zA-Z][a-zA-Z0-9._-]+$` にマッチする必要があります(モジュールIDと同じ)
  - 文字(a-zA-Z)で開始する必要があります
  - 文字、数字、ドット(`.`)、アンダースコア(`_`)、ハイフン(`-`)を含めることができます
  - 最小長:2文字
- **値形式**:制限なし - 改行、制御文字などを含むあらゆるUTF-8文字を含めることができます
  - 長さプレフィックス付きのバイナリ形式で保存され、すべてのデータの安全な処理を保証

## ライフサイクル

- **起動時**:すべての一時設定が post-fs-data ステージでクリアされます
- **モジュールアンインストール時**:すべての設定(永続と一時)が自動的に削除されます
- 設定はバイナリ形式で保存され、マジックナンバー `0x4b53554d`("KSUM")とバージョン検証を使用します

## ユースケース

設定システムは以下に最適です:

- **ユーザー設定**:WebUI または action スクリプトを通じてユーザーが設定したモジュール設定を保存
- **機能フラグ**:再インストールせずにモジュール機能を有効/無効にする
- **ランタイム状態**:再起動時にリセットすべき一時的な状態を追跡(一時設定を使用)
- **インストール設定**:モジュールインストール時に行った選択を記憶
- **複雑なデータ**:JSON、複数行テキスト、Base64エンコードデータ、または任意の構造化コンテンツを保存(最大1MB)

::: tip ベストプラクティス
- 再起動後も保持すべきユーザー設定には永続設定を使用
- 起動時にリセットすべきランタイム状態や機能フラグには一時設定を使用
- スクリプトで設定値を使用する前に検証する
- 設定の問題をデバッグするには `ksud module config list` コマンドを使用
:::

## 高級機能

モジュール設定システムは、高度なユースケースのための特別な設定キーを提供します:

### モジュール説明のオーバーライド

`override.description` 設定キーを設定することで、`module.prop` の `description` フィールドを動的にオーバーライドできます:

```bash
# モジュール説明をオーバーライド
ksud module config set override.description "マネージャーに表示されるカスタム説明"
```

モジュールリストを取得する際、`override.description` 設定が存在する場合、`module.prop` の元の説明が置き換えられます。これは以下の場合に便利です:
- モジュール説明に動的なステータス情報を表示
- ユーザーにランタイム設定の詳細を表示
- 再インストールせずにモジュールの状態に基づいて説明を更新

### 管理対象機能の宣言

モジュールは `manage.<feature>` 設定パターンを使用して、管理する KernelSU 機能を宣言できます。サポートされている機能は、KernelSU 内部の `FeatureId` 列挙型に対応しています:

**サポートされている機能:**
- `su_compat` - SU 互換モード
- `kernel_umount` - カーネル自動アンマウント
- `enhanced_security` - 強化されたセキュリティモード

```bash
# このモジュールが SU 互換性を管理し、有効にすることを宣言
ksud module config set manage.su_compat true

# このモジュールがカーネルアンマウントを管理し、無効にすることを宣言
ksud module config set manage.kernel_umount false

# 機能管理を削除(モジュールはこの機能を制御しなくなります)
ksud module config delete manage.su_compat
```

**動作原理:**
- `manage.<feature>` キーの存在は、モジュールがその機能を管理していることを示します
- 値は希望する状態を示します:`true`/`1` は有効、`false`/`0`(またはその他の値)は無効
- 機能の管理を停止するには、設定キーを完全に削除します

管理対象機能は、モジュールリスト API を通じて `managedFeatures` フィールド(カンマ区切りの文字列)として公開されます。これにより以下が可能になります:
- KernelSU マネージャーがどのモジュールがどの KernelSU 機能を管理しているかを検出
- 複数のモジュールが同じ機能を管理しようとした際の競合を防止
- モジュールとコア KernelSU 機能間のより良い調整

::: warning サポートされている機能のみ
上記にリストされた事前定義された機能名(`su_compat`、`kernel_umount`、`enhanced_security`)のみを使用してください。これらは実際の KernelSU 内部機能に対応しています。他の機能名を使用してもエラーにはなりませんが、機能的な目的はありません。
:::
