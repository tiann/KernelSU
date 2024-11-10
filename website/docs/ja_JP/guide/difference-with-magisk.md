# Magisk との違い

KernelSU モジュールと Magisk モジュールには多くの共通点がありますが、実装の仕組みが全く異なるため、必然的にいくつかの相違点が存在します。Magisk と KernelSU の両方でモジュールを動作させたい場合、これらの違いを理解する必要があります。

## 似ているところ

- モジュールファイルの形式：どちらもzip形式でモジュールを整理しており、モジュールの形式はほぼ同じです。
- モジュールのインストールディレクトリ：どちらも `/data/adb/modules` に配置されます。
- システムレス：どちらもモジュールによるシステムレスな方法で /system を変更できます。
- post-fs-data.sh: 実行時間と意味は全く同じです。
- service.sh: 実行時間と意味は全く同じです。
- system.prop：全く同じです。
- sepolicy.rule：全く同じです。
- BusyBox：スクリプトは BusyBox で実行され、どちらの場合も「スタンドアロンモード」が有効です。

## 違うところ

違いを理解する前に、モジュールが KernelSU で動作しているか Magisk で動作しているかを区別する方法を知っておく必要があります。環境変数 `KSU` を使うとモジュールスクリプトを実行できるすべての場所 (`customize.sh`, `post-fs-data.sh`, `service.sh`) で区別できます。KernelSU では、この環境変数に `true` が設定されます。

以下は違いです：

- KernelSU モジュールは、リカバリーモードではインストールできません。
- KernelSU モジュールには Zygisk のサポートが組み込まれていません（ただし[ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext)を使うと Zygisk モジュールを使用できます）。
- KernelSU モジュールにおけるファイルの置換や削除の方法は、Magisk とは全く異なります。KernelSU は `.replace` メソッドをサポートしていません。その代わり、`mknod filename c 0 0` で同名のファイルを作成し、対応するファイルを削除する必要があります。
- BusyBox 用のディレクトリが違います。KernelSU の組み込み BusyBox は `/data/adb/ksu/bin/busybox` に、Magisk では `/data/adb/magisk/busybox` に配置されます。**これは KernelSU の内部動作であり、将来的に変更される可能性があることに注意してください!**
- KernelSU は `.replace` ファイルをサポートしていません。しかし、KernelSU はファイルやフォルダを削除したり置き換えたりするための `REMOVE` と `REPLACE` 変数をサポートしています。
