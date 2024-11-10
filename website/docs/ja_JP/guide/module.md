# モジュールのガイド

KernelSU はシステムパーティションの整合性を維持しながら、システムディレクトリを変更する効果を実現するモジュール機構を提供します。この機構は一般に「システムレス」と呼ばれています。

KernelSU のモジュール機構は、Magisk とほぼ同じです。Magisk のモジュール開発に慣れている方であれば、KernelSU のモジュール開発も簡単でしょう。その場合は以下のモジュールの紹介は読み飛ばして、[Magisk との違い](difference-with-magisk.md)の内容だけ読めばOKです。

## Busybox

KernelSU には、機能的に完全な Busybox バイナリ (SELinux の完全サポートを含む) が同梱されています。実行ファイルは `/data/adb/ksu/bin/busybox` に配置されています。KernelSU の Busybox はランタイムに切り替え可能な「ASH スタンドアローンシェルモード」をサポートしています。このスタンドアロンモードとは、Busybox の `ash` シェルで実行する場合 `PATH` として設定されているものに関係なく、すべてのコマンドが Busybox 内のアプレットを直接使用するというものです。たとえば、`ls`、`rm`、`chmod` などのコマンドは、`PATH` にあるもの（Android の場合、デフォルトではそれぞれ `/system/bin/ls`, `/system/bin/rm`, `/system/bin/chmod`）ではなく、直接 Busybox 内部のアプレットを呼び出すことになります。これにより、スクリプトは常に予測可能な環境で実行され、どの Android バージョンで実行されていても常にコマンドを利用できます。Busybox を使用しないコマンドを強制的に実行するには、フルパスで実行ファイルを呼び出す必要があります。

KernelSU のコンテキストで実行されるすべてのシェルスクリプトは、Busybox の `ash` シェルでスタンドアロンモードが有効な状態で実行されます。サードパーティの開発者に関係するものとしては、すべてのブートスクリプトとモジュールのインストールスクリプトが含まれます。

この「スタンドアロンモード」機能を KernelSU 以外で使用したい場合、2つの方法で有効にできます：

1. 環境変数 `ASH_STANDALONE` を `1` にする<br>例: `ASH_STANDALONE=1 /data/adb/ksu/bin/busybox sh <script>`
2. コマンドラインのオプションで変更する:<br>`/data/adb/ksu/bin/busybox sh -o standalone <script>`

環境変数が子プロセスに継承されるため、その後に実行されるすべての `sh` シェルもスタンドアロンモードで実行されるようにするにはオプション 1 が望ましい方法です（KernelSU と KernelSU Managerが内部的に使用しているのもこちらです）。

::: tip Magisk との違い

KernelSU の Busybox は、Magisk プロジェクトから直接コンパイルされたバイナリファイルを使用するようになりました。Magisk と KernelSU の Busybox スクリプトはまったく同じものなので、互換性の問題を心配する必要はありません！
:::

## KernelSU モジュール

KernelSU モジュールは、`/data/adb/modules` に配置された以下の構造を持つフォルダーです：

```txt
/data/adb/modules
├── .
├── .
|
├── $MODID                  <--- フォルダの名前はモジュールの ID で付けます
│   │
│   │      *** モジュールの ID ***
│   │
│   ├── module.prop         <--- このファイルにモジュールのメタデータを保存します
│   │
│   │      *** メインコンテンツ ***
│   │
│   ├── system              <--- skip_mount が存在しない場合、このフォルダがマウントされます
│   │   ├── ...
│   │   ├── ...
│   │   └── ...
│   │
│   │      *** ステータスフラグ ***
│   │
│   ├── skip_mount          <--- 存在する場合、KernelSU はシステムフォルダをマウントしません
│   ├── disable             <--- 存在する場合、モジュールは無効化されます
│   ├── remove              <--- 存在する場合、次の再起動時にモジュールが削除されます
│   │
│   │      *** 任意のファイル ***
│   │
│   ├── post-fs-data.sh     <--- このスクリプトは post-fs-data で実行されます
│   ├── service.sh          <--- このスクリプトは late_start サービスで実行されます
|   ├── uninstall.sh        <--- このスクリプトは KernelSU がモジュールを削除するときに実行されます
│   ├── system.prop         <--- このファイルのプロパティは resetprop によってシステムプロパティとして読み込まれます
│   ├── sepolicy.rule       <--- カスタム SEPolicy ルールを追加します
│   │
│   │      *** 自動生成されるため、手動で作成または変更しないでください ***
│   │
│   ├── vendor              <--- $MODID/system/vendor へのシンボリックリンク
│   ├── product             <--- $MODID/system/product へのシンボリックリンク
│   ├── system_ext          <--- $MODID/system/system_ext へのシンボリックリンク
│   │
│   │      *** その他のファイル/フォルダの追加も可能です ***
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

::: tip Magisk との違い
KernelSU は Zygisk をビルトインでサポートしていないため、モジュール内に Zygisk に関連するコンテンツは存在しません。 しかし、[ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext) をインストールすれば Zygisk モジュールを使えます。その場合の Zygisk モジュールのコンテンツは Magisk と同じです。
:::

### module.prop

module.prop はモジュールの設定ファイルです。KernelSU ではこのファイルを含まないモジュールは、モジュールとして認識されません。このファイルの形式は以下の通りです：

```txt
id=<string>
name=<string>
version=<string>
versionCode=<int>
author=<string>
description=<string>
```

- `id` はこの正規表現に一致していなければいけません: `^[a-zA-Z][a-zA-Z0-9._-]+$`<br>
  例: ✓ `a_module`, ✓ `a.module`, ✓ `module-101`, ✗ `a module`, ✗ `1_module`, ✗ `-a-module`<br>
  これはモジュールの**ユニークな ID** です。公開後は変更しないでください。
- `versionCode` は **integer** です。バージョンの比較に使います。
- 他のものには**単一行** の文字であれば何でも使えます。
- 改行文字は `UNIX (LF)` を使ってください。`Windows (CR+LF)` や `Macintosh (CR)` は使ってはいけません。

### シェルスクリプト

`post-fs-data.sh` と `service.sh` の違いについては、[ブートスクリプト](#boot-scripts)のセクションを読んでください。ほとんどのモジュール開発者にとって、ブートスクリプトを実行するだけなら `service.sh` で十分なはずです。

モジュールのすべてのスクリプトでは、`MODDIR=${0%/*}`を使えばモジュールのベースディレクトリのパスを取得できます。スクリプト内でモジュールのパスをハードコードしないでください。

::: tip Magisk との違い
環境変数 `KSU` を使用すると、スクリプトが KernelSU と Magisk どちらで実行されているかを判断できます。KernelSU で実行されている場合、この値は `true` に設定されます。
:::

### `system` ディレクトリ

このディレクトリの内容は、システムの起動後に OverlayFS を使用してシステムの /system パーティションの上にオーバーレイされます：

1. システム内の対応するディレクトリにあるファイルと同名のファイルは、このディレクトリにあるファイルで上書きされます。
2. システム内の対応するディレクトリにあるフォルダと同じ名前のフォルダは、このディレクトリにあるフォルダと統合されます。

元のシステムディレクトリにあるファイルやフォルダを削除したい場合は、`mknod filename c 0 0` を使ってモジュールディレクトリにそのファイル/フォルダと同じ名前のファイルを作成する必要があります。こうすることで、OverlayFS システムはこのファイルを削除したかのように自動的に「ホワイトアウト」します（/system パーティションは実際には変更されません）。

また、`customize.sh` 内で `REMOVE` という変数に削除操作を実行するディレクトリのリストを宣言すると、KernelSU は自動的にそのモジュールの対応するディレクトリで `mknod <TARGET> c 0 0` を実行します。例えば

```sh
REMOVE="
/system/app/YouTube
/system/app/Bloatware
"
```

上記の場合は、`mknod $MODPATH/system/app/YouTuBe c 0 0`と`mknod $MODPATH/system/app/Bloatware c 0 0`を実行し、`/system/app/YouTube`と`/system/app/Bloatware`はモジュール有効化後に削除されます。

システム内のディレクトリを置き換えたい場合は、モジュールディレクトリに同じパスのディレクトリを作成し、このディレクトリに `setfattr -n trusted.overlay.opaque -v y <TARGET>` という属性を設定する必要があります。こうすることで、OverlayFS システムは（/system パーティションを変更することなく）システム内の対応するディレクトリを自動的に置き換えることができます。

`customize.sh` ファイル内に `REPLACE` という変数を宣言し、その中に置換するディレクトリのリストを入れておけば、KernelSU は自動的にモジュールディレクトリに対応した処理を行います。例えば：

REPLACE="
/system/app/YouTube
/system/app/Bloatware
"

このリストは、自動的に `$MODPATH/system/app/YouTube` と `$MODPATH/system/app/Bloatware` というディレクトリを作成し、 `setfattr -n trusted.overlay.opaque -v y $MODPATH/system/app/YouTube` と `setfattr -n trusted.overlay.opaque -v y $MODPATH/system/app/Bloatware` を実行します。モジュールが有効になると、`/system/app/YouTube` と `/system/app/Bloatware` は空のディレクトリに置き換えられます。

::: tip Magisk との違い

KernelSU のシステムレスメカニズムはカーネルの OverlayFS によって実装され、Magisk は現在マジックマウント（bind mount）を使用しています。この2つの実装方法には大きな違いがありますが最終的な目的は同じで、/system パーティションを物理的に変更することなく、/system のファイルを変更できます。
:::

OverlayFS に興味があれば、Linux カーネルの [OverlayFS のドキュメンテーション](https://docs.kernel.org/filesystems/overlayfs.html) を読んでみてください。

### system.prop

このファイルは `build.prop` と同じ形式をとっています。各行は `[key]=[value]` で構成されます。

### sepolicy.rule

もしあなたのモジュールが追加の SEPolicy パッチを必要とする場合は、それらのルールをこのファイルに追加してください。このファイルの各行は、ポリシーステートメントとして扱われます。

## モジュールのインストーラー

KernelSU モジュールインストーラーは、KernelSU Manager アプリでインストールできる、ZIP ファイルにパッケージされた KernelSU モジュールです。最もシンプルな KernelSU モジュールインストーラーは、KernelSU モジュールを ZIP ファイルとしてパックしただけのものです。

```txt
module.zip
│
├── customize.sh                       <--- (任意、詳細は後述)
│                                           このスクリプトは update-binary から読み込まれます
├── ...
├── ...  /* 残りのモジュールのファイル */
│
```

::: warning 警告
KernelSU モジュールは、カスタムリカバリーからのインストールには非対応です！
:::

### カスタマイズ

モジュールのインストールプロセスをカスタマイズする必要がある場合、`customize.sh` という名前のスクリプトを作成してください。このスクリプトは、すべてのファイルが抽出され、デフォルトのパーミッションと secontext が適用された後、モジュールインストーラースクリプトによって読み込み (実行ではなく) されます。これは、モジュールがデバイスの ABI に基づいて追加設定を必要とする場合や、モジュールファイルの一部に特別なパーミッション/コンテキストを設定する必要がある場合に、非常に便利です。

インストールプロセスを完全に制御しカスタマイズしたい場合は、`customize.sh` で `SKIPUNZIP=1` と宣言すればデフォルトのインストールステップをすべてスキップできます。そうすることで、`customize.sh` が責任をもってすべてをインストールするようになります。

`customize.sh`スクリプトは、KernelSU の Busybox `ash` シェルで、「スタンドアロンモード」を有効にして実行します。以下の変数と関数が利用可能です：

#### 変数

- `KSU` (bool): スクリプトが KernelSU 環境で実行されていることを示すための変数で、この変数の値は常に true になります。KernelSU と Magisk を区別するために使用できます。
- `KSU_VER` (string): 現在インストールされている KernelSU のバージョン文字列 (例: `v0.4.0`)
- `KSU_VER_CODE` (int): ユーザー空間に現在インストールされているKernelSUのバージョンコード (例: `10672`)
- `KSU_KERNEL_VER_CODE` (int): 現在インストールされている KernelSU のカーネル空間でのバージョンコード（例：`10672`）
- `BOOTMODE` (bool): KernelSU では常に `true` 
- `MODPATH` (path): モジュールファイルがインストールされるパス
- `TMPDIR` (path): ファイルを一時的に保存しておく場所
- `ZIPFILE` (path): あなたのモジュールのインストールZIP
- `ARCH` (string): デバイスの CPU アーキテクチャ。値は `arm`、`arm64`、`x86`、`x64` のいずれか
- `IS64BIT` (bool): `ARCH` が `arm64` または `x64` のときは `true` 
- `API` (int): 端末の API レベル・Android のバージョン（例：Android 6.0 なら`23`）

::: warning 警告
KernelSU では、MAGISK_VER_CODE は常に25200、MAGISK_VER は常にv25.2です。この2つの変数で KernelSU 上で動作しているかどうかを判断するのはやめてください。
:::

#### 機能

```txt
ui_print <msg>
    コンソールに <msg> を表示します
    カスタムリカバリーのコンソールでは表示されないため、「echo」の使用は避けてください

abort <msg>
    エラーメッセージ<msg>をコンソールに出力し、インストールを終了させます
    終了時のクリーンアップがスキップされてしまうため、「exit」の使用は避けてください

set_perm <target> <owner> <group> <permission> [context]
    [context] が設定されていない場合、デフォルトは "u:object_r:system_file:s0" です。
    この機能は、次のコマンドの略記です：
       chown owner.group target
       chmod permission target
       chcon context target

set_perm_recursive <directory> <owner> <group> <dirpermission> <filepermission> [context]
    [context] が設定されていない場合、デフォルトは "u:object_r:system_file:s0" です。
    <directory> 内のすべてのファイルに対しては以下が実行されます:
       set_perm file owner group filepermission context
    <directory> 内のすべてのディレクトリ（自身を含む）に対しては以下が実行されます:
       set_perm dir owner group dirpermission context
```

## ブートスクリプト

KernelSU では、スクリプトは実行モードによって post-fs-data モードと late_start サービスモードの2種類に分けられます：

- post-fs-data モード
  - 同期処理です。実行が終わるか、10秒が経過するまでブートプロセスが一時停止されます。
  - スクリプトはモジュールがマウントされる前に実行されます。モジュール開発者はモジュールがマウントされる前に、動的にモジュールを調整できます。
  - このステージは Zygote が始まる前に起こるので、Android のほぼすべての処理の前に割り込めます
  - **警告:** `setprop` を使うとブートプロセスのデッドロックを引き起こします! `resetprop -n <prop_name> <prop_value>` を使ってください。
  - **本当に必要な場合だけこのモードでコマンド実行してください**
- late_start サービスモード
  - 非同期処理です。スクリプトは、起動プロセスの残りの部分と並行して実行されます。
  - **ほとんどのスクリプトにはこちらがおすすめです**

KernelSU では、起動スクリプトは保存場所によって一般スクリプトとモジュールスクリプトの2種類に分けられます：

- 一般スクリプト
  - `/data/adb/post-fs-data.d` か `/data/adb/service.d` に配置されます
  - スクリプトが実行可能な状態に設定されている場合にのみ実行されます (`chmod +x script.sh`)
  - `post-fs-data.d` のスクリプトは post-fs-data モードで実行され、`service.d` のスクリプトは late_start サービスモードで実行されます
  - モジュールはインストール時に一般スクリプトを追加するべきではありません
- モジュールスクリプト
  - モジュール独自のフォルダに配置されます
  - モジュールが有効な場合のみ実行されます
  - `post-fs-data.sh` は post-fs-data モードで実行され、`service.sh` は late_start サービスモードで実行されます

すべてのブートスクリプトは、KernelSU の Busybox `ash` シェルで「スタンドアロンモード」を有効にした状態で実行されます。
