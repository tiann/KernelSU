# x86_64 サポート

KernelSU は `x86_64` アーキテクチャを完全にサポートしています。ただし、upstream kernel の最近のセキュリティ変更により、最新の `x86_64` カーネルへ KernelSU を統合するには、統一された syscall dispatcher を正しく動作させるための追加対応が必要です。

## なぜ動かなくなったのですか？

新しいカーネルバージョンでは、syscall table を強化するための [コミット](https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/commit/?id=1e3ad78334a69b36e107232e337f9d693dcc9df2) が導入されました。この変更により、システムコール経路内の間接分岐が一連の直接条件分岐へ変換されました。

KernelSU の `syscall_hook` は、フックしたシステムコールを統一 dispatcher にルーティングするために syscall table のエントリ変更へ依存しています。新しい hardening はシステムコール経路を変更するため、カーネルはそれらの syscall table 変更を無視します。KernelSU がこの制限を正しく処理しないまま syscall table を hook しようとすると、呼び出しを正しくルーティングできず、kernel panic を防ぐために初期化を中断します。

## どうすればよいですか？

`x86_64` の syscall hook 問題には 2 つの公式サポートされた方法があります。

1. カーネルのビルドオプション `KSU_X86_PATCH_SYSCALL_DISPATCHER` を有効にする。
2. 従来どおりカーネルソースパッチ方式を使う。

必要なのはどちらか一方だけです。両方を同時に適用しないでください。

### 方法 1: `KSU_X86_PATCH_SYSCALL_DISPATCHER` を有効にする

KernelSU 3.2.6 では、`x86_64` 向けの新しい公式メカニズムとして `KSU_X86_PATCH_SYSCALL_DISPATCHER` ビルドオプションが追加されました。

このオプションを有効にすると、KernelSU は hardened syscall dispatcher を実行時に動的パッチし、従来のカーネルソースパッチを要求せずに syscall hook を動作させます。KernelSU 3.2.6 以降でカーネルをビルドする場合は、この方法を推奨します。

### 方法 2: 従来のカーネルソースパッチを適用する

`KSU_X86_PATCH_SYSCALL_DISPATCHER` を有効にしたくない場合は、これまでのカーネルパッチ方式を継続して使えます。

これらの新しいカーネルで KernelSU を動作させるには、この syscall hardening を回避できるパッチを適用してください。

::: danger セキュリティ警告
この 2 つの方法のいずれかを使うことは、speculative execution 脆弱性への対策を意図的に回避または弱めることを意味します。

これにより、system call に対する間接分岐の攻撃面が再び開かれます。**本番サーバーや、厳格な side-channel セキュリティが重要なシステムでは、これら 2 つの方法のいずれも使わないでください。** これらの方法は、特定のハードウェア脆弱性対策よりも KernelSU による root アクセスを優先するテスト環境向けです。
:::

以下からカーネルバージョンに一致するパッチを選んで適用してください。これらのパッチは `X86_FEATURE_INDIRECT_SAFE` という機能を作成し、カーネルコマンドライン `syscall_hardening=off` で有効化できます。

```
For kernel 6.6:
https://github.com/android-generic/kernel_common/commit/fe9a9b4c320577c30e1f22d04039e414c6a3cdec
https://github.com/android-generic/kernel_common/commit/df772e99e392f24b395ceaf7b26974e3e4828ee9

For kernel 6.12:
https://github.com/android-generic/kernel-zenith/commit/dd2c602268fdc81f4d3b662f6a15142ac0ec7bcd
https://github.com/android-generic/kernel-zenith/commit/7d99237ae5da61c19447138da3282ae37d43857b

For kernel 6.18:
https://github.com/android-generic/kernel-zenith/commit/40b1c323d1ad29c86e041d665c7f089b9a3ccfb5
https://github.com/android-generic/kernel-zenith/commit/f5813e10b7630e1ccd86fc2c4cf30eef60b64a82
```

## どちらを選ぶべきですか？

- KernelSU 3.2.6 以降を使っていて、KernelSU のビルド設定を変更できるなら `KSU_X86_PATCH_SYSCALL_DISPATCHER` を有効にしてください。
- 現在のカーネルパッチ運用を維持したいなら、上記の従来のソースパッチを使い続けてください。
