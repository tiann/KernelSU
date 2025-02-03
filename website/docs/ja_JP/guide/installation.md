# インストール

## デバイスが対応しているか確認する

[GitHub Releases](https://github.com/tiann/KernelSU/releases) から KernelSU Manager アプリをダウンロードし、お使いのデバイスにインストールしてください。

- アプリが「非対応」と表示した場合は、**自分でカーネルをコンパイルする必要がある**という意味です。KernelSU は書き込むためのブートイメージを提供しません。
- アプリが「未インストール」と表示した場合、お使いのデバイスは KernelSU に対応しています。

::: info ヒント
非対応と表示されているデバイスについては、[非公式の対応デバイス](unofficially-support-devices.md)であればご自身でカーネルをビルドできます。
:::

## 純正の boot.img をバックアップ

書き込む前に、まず純正の boot.img をバックアップする必要があります。ブートループが発生した場合は、fastboot を使用して純正のブートイメージを書き込むことでいつでもシステムを復旧できます。

::: warning 警告
書き込みによりデータ損失を引き起こす可能性があります。次のステップに進む前に、このステップを必ず行うようにしてください！また、可能であればすべてのデータをバックアップしてください。
:::

## 必要な知識

### ADB と fastboot

このチュートリアルでは、デフォルトで ADB と fastboot のツールを使用します。ご存じない方は、まず検索エンジンを使って勉強されることをおすすめします。

### KMI

同じ Kernel Module Interface (KMI) のカーネルバージョンは**互換性があります**。これが GKI の「汎用」という意味です。逆に言えば KMI が異なればカーネルには互換性がなく、お使いのデバイスと異なる KMI のカーネルイメージを書き込むと、ブートループが発生する場合があります。

具体的には GKI デバイスの場合、カーネルバージョンの形式は以下のようになります：

```txt
KernelRelease :=
Version.PatchLevel.SubLevel-AndroidRelease-KmiGeneration-suffix
w      .x         .y       -zzz           -k            -something
```

`w.x-zzz-k` は KMI のバージョンです。例えば、デバイスのカーネルバージョンが `5.10.101-android12-9-g30979850fc20` である場合、その KMIは `5.10-android12-9` であり、理論的には他の KMI カーネルでも正常に起動できます。

::: tip ヒント
カーネルバージョンの SubLevel は、KMI の一部ではないことに注意してください。`5.10.101-android12-9-g30979850fc20` は `5.10.137-android12-9-g30979850fc20` と同じ KMI を持っているということになります。
:::

### セキュリティパッチレベル {#security-patch-level}

新しめの Android 端末には、セキュリティパッチレベルが古い boot イメージのフラッシュを許可しないロールバック防止機構が導入されていることがあります。例えば、もしあなたの端末のカーネルが `5.10.101-android12-9-g30979850fc20` であれば、セキュリティパッチレベルは `2023-11` です。たとえ KMI に対応するカーネルをフラッシュしても、セキュリティパッチレベルが (`2023-06` のように) `2023-11` より低い場合、ブートループを起こす可能性があります。

### Kernel バージョンと Android バージョンの違い

注意： **カーネルバージョンと Android バージョンは必ずしも同じではありません**。

カーネルのバージョンは「android12-5.10.101」なのに、Android システムのバージョンは「Android 13」などとなっている場合、驚かないでください。Linux カーネルのバージョン番号は、必ずしも**デバイスの出荷時**にプリインストールされている Android システムのバージョンと一致していません。Android システムが後でアップグレードされた場合、一般的にはカーネルのバージョンは変更されません。書き込む際は、**必ずカーネルバージョンを参照してください**!!!

## はじめに

バージョン [0.9.0](https://github.com/tiann/KernelSU/releases/tag/v0.9.0) 以降、KernelSU は GKI 端末において 2 つの動作モードに対応しています。

1. `GKI`: 端末本来のカーネルを KernelSU によって提供される **汎用カーネルイメージ** (GKI) で置き換えます。
2. `LKM`: 端末本来のカーネルを置き換えずに、カーネルに **ローダブルカーネルモジュール** (LKM) を読み込みます。

これらの 2 つのモードはそれぞれ異なる状況に適しており、必要に応じてどちらかを選ぶことができます。

### GKI モード {#gki-mode}

GKI モードでは、端末本来のカーネルは KernelSU によって提供される汎用カーネルイメージによって置き換えられます。GKI モードの利点は:

1. 強い汎用性があり、大部分の端末に適合。例えば、Samsung 端末は KNOX が有効であり、LKM モードは動作できません。また、GKI モードしか使えないニッチな改造を施された端末もあります。
2. 公式ファームウェアに依存せずに使うことができます。KMI が一致している限り、公式ファームウェアの更新を待つことなく使うことができます。

### LKM モード {#lkm-mode}

LKM モードでは、端末本来のカーネルを置き換えずに、カーネルにローダブルカーネルモジュールを読み込みます。LKM モードの利点は:

1. 端末本来のカーネルを置き換えません。端末本来のカーネルに特別な要件がある場合や、KernelSU をサードパーティのカーネルと両用したい場合、LKM モードを利用できます。
2. アップグレードと OTA がより便利です。KernelSU をアップデートする時は、手動でフラッシュすることなくマネージャーから直接インストールできます。システム OTA の後は、手動でフラッシュすることなく非アクティブなスロットに直接インストールすることができます。
3. いくつかの特殊な状況に適しています。例えば、LKM は一時的な Root 権限を用いて読み込むことができます。boot パーティションを置き換えないので、AVB を動作させず、端末を文鎮化しません。
4. LKM は一時的にアンインストールすることができます。一時的に root アクセスを無効化したい場合、LKM をアンインストールすることができます。この過程でパーティションをフラッシュすることも、また再起動することさえも必要ありません。root アクセスを再度有効化したい場合は、再起動するだけです。

:::tip 2 つのモードの共存
マネージャーを開くと、ホームで現在の端末の動作モードを確認できます。GKI モードの優先度は LKM モードより高いことに留意してください。例えば、本来のカーネルを GKI カーネルで置き換えたうえで LKM を使って GKI カーネルにパッチしている場合、LKM は無視され、端末は常時 GKI モードで動作します。
:::

### どちらを選ぶべきですか? {#which-one}

端末が携帯電話の場合、LKM モードを優先することを推奨します。端末がエミュレーター、WSA、または Waydroid の場合、GKI モードを優先することを推奨します。

## LKM モードのインストール

### 公式ファームウェアの入手

LKM モードを使うためには、公式ファームウェアを入手し、それを基にパッチを当てる必要があります。サードパーティのカーネルを使う場合、その `boot.img` を公式ファームウェアとして利用できます。

公式ファームウェアを入手する方法はたくさんあります。端末が `fastboot boot` に対応している場合、`fastboot boot` を使って一時的に KernelSU が提供する GKI カーネルを起動する、 **最も推奨される、最も簡単な** 方法を推奨します。

端末が `fastboot boot` に対応していない場合、公式ファームウェアのパッケージを手動でダウンロードし、そこから `boot.img` を展開する必要があります。

GKI モードとは異なり、LKM モードは `ramdisk` を展開します。そのため、Android 13 の端末では `boot` パーティションの代わりに `init_boot` パーティションにパッチを当てる必要があります。一方で、GKI モードは常に `boot` パーティションを操作します。

### マネージャーを使う

マネージャーを開き、右上のインストールアイコンをタップすると、いくつかのオプションが現れます:

1. ファイルを選択しパッチを当てる。端末が Root 権限を持っていない場合、このオプションを選択し、公式ファームウェアを選択すると、マネージャーが自動的にパッチを当てます。恒久的に Root 権限を獲得するには、パッチが当てられたファイルをフラッシュするだけです。
2. 直接インストール。端末が既に Root 化されている場合、このオプションを選択すると、マネージャーが自動的に端末の情報を取得し、自動的に公式ファームウェアにパッチを当て、それをフラッシュします。`fastboot boot` で KernelSU の GKI を起動し一時的に Root 権限を獲得したうえでこのオプションを利用することもできます。これは KernelSU をアップグレードする主な方法でもあります。
3. 非アクティブなスロットにインストール。端末が A/B パーティションに対応している場合、このオプションを選択すると、マネージャーが自動的に公式ファームウェアにパッチを当て、もう一方のパーティションにインストールします。この方法は OTA 後の端末に適しており、OTA 後にもう一方のパーティションに直接 KernelSU をインストールし、端末を再起動することができます。

### コマンドラインを使う

マネージャーを使いたくない場合、コマンドラインを使って LKM をインストールできます。KernelSU の提供する `ksud` ツールを使うと、迅速に公式ファームウェアにパッチを当てフラッシュすることができます。

このツールは macOS、Linux および Windows に対応しています。[GitHub Release](https://github.com/tiann/KernelSU/releases) から対応するバージョンをダウンロードしてください。

使用方法: `ksud bootpatch` コマンドの各オプションのヘルプをコマンドラインから確認できます。
```sh
oriole:/ # ksud boot-patch -h
Patch boot or init_boot images to apply KernelSU

Usage: ksud boot-patch [OPTIONS]

Options:
  -b, --boot <BOOT>              boot image path, if not specified, will try to find the boot image automatically
  -k, --kernel <KERNEL>          kernel image path to replace
  -m, --module <MODULE>          LKM module path to replace, if not specified, will use the builtin one
  -i, --init <INIT>              init to be replaced
  -u, --ota                      will use another slot when boot image is not specified
  -f, --flash                    Flash it to boot partition after patch
  -o, --out <OUT>                output path, if not specified, will use current directory
      --magiskboot <MAGISKBOOT>  magiskboot path, if not specified, will use builtin one
      --kmi <KMI>                KMI version, if specified, will use the specified KMI
  -h, --help                     Print help
```

説明が必要ないくつかのオプション:

1. `--magiskboot` オプションは magiskboot のパスを指定します。指定されていない場合、ksud は環境変数を参照します。magiskboot の入手方法がわからない場合、[こちら](#patch-boot-image) を参照してください。
2. `--kmi` オプションは `KMI` バージョンを指定します。端末のカーネル名が KMI の仕様に準拠していない場合は、このオプションで指定することができます。

最も一般的な使い方: 

```sh
ksud boot-patch -b <boot.img> --kmi android13-5.10
```

## GKI モードのインストール

GKI モードのインストール方法はいくつかあり、それぞれ適したシーンが異なりますので、必要に応じて選択してください。

1. KernelSU が提供する boot.img を使用し、fastboot でインストールする
2. KernelFlasher などのカーネル管理アプリでインストールする
3. boot.img を手動でパッチしてインストールする
4. カスタムリカバリー（TWRPなど）でインストールする

## KernelSU が提供する boot.img を使用してインストール

この方法は TWRP や root 権限を必要としないので、KernelSU を初めてインストールする場合に適しています。

### 正しい boot.img を見つける

KernelSU では、GKI デバイス用の汎用 boot.img を提供しています。デバイスの boot パーティションに boot.img をフラッシュする必要があります。

boot.img は、[GitHub Release](https://github.com/tiann/KernelSU/releases) からダウンロードできます。例えば、あなたのデバイスがカーネル `android12-5.10.101` の場合、`android-5.10.101_yyyy-MM.boot-<format>.img`をダウンロードする必要があります。（KMI を同じにしてください！）。

`<format>`は純正 boot.img のカーネル圧縮形式を指します。純正の boot.img のカーネル圧縮形式を確認してください。間違った圧縮形式を使うと、ブートループするかもしれません。

::: info 情報
1. magiskboot を使えば、元のブートの圧縮形式を知ることができます。もちろん、あなたのデバイスと同じモデルを持つ、より経験豊富な他の人にも聞くこともできます。また、カーネルの圧縮形式は通常変更されないので、ある圧縮形式でうまく起動した場合、後でその形式を試すことも可能です。
2. Xiaomi デバイスでは通常 `gz` か**無圧縮**が使われます。
3. Pixel デバイスでは以下の手順に従ってください。
:::

### boot.img をデバイスに書き込む

`adb` でデバイスを接続し、`adb reboot bootloader` で fastboot モードにし、このコマンドで KernelSU を書き込んでください：

```sh
fastboot flash boot boot.img
```

::: info 情報
デバイスが `fastboot boot` をサポートしている場合、まず `fastboot boot.img` を使えば書き込みせずにシステムを起動できます。予期せぬことが起こった場合は、もう一度再起動して起動してください。
:::

### 再起動

書き込みが完了したら、デバイスを再起動します：

```sh
fastboot reboot
```

## カーネル管理アプリでインストール

前提条件：お使いのデバイスが root 化されている必要があります。例えば、Magisk をインストールして root を取得した場合、または古いバージョンの KernelSU をインストールしており、別のバージョンの KernelSU にアップグレードする必要がある場合などです。お使いのデバイスが root 化されていない場合、他の方法をお試しください。

手順:

1. AnyKernel3 ZIP をダウンロードします。ダウンロード方法は、「カスタムリカバリーでインストール」を参照してください。
2. カーネル管理アプリを開き、AnyKernel3 の ZIP をインストールします。

カーネル管理アプリは以下のようなものが人気です：

1. [Kernel Flasher](https://github.com/capntrips/KernelFlasher/releases)
2. [Franco Kernel Manager](https://play.google.com/store/apps/details?id=com.franco.kernel)
3. [Ex Kernel Manager](https://play.google.com/store/apps/details?id=flar2.exkernelmanager)

この方法は KernelSU をアップグレードするときに便利で、パソコンがなくてもできます。（まずはバックアップしてください！）

## boot.img を手動でパッチ {#patch-boot-image}

デバイスによっては、boot.img のフォーマットが `lz4` でない、`gz` である、無圧縮であるなど、あまり一般的でないことがあります。最も典型的なのは Pixel で、boot.img フォーマットは `lz4_legacy` 圧縮、RAM ディスクは `gz` か `lz4_legacy` 圧縮です。この時、KernelSU が提供した boot.img を直接書き込むとデバイスが起動できなくなる場合があります。その場合は手動で boot.img に対してパッチしてください。

常に `magiskboot` を利用してイメージにパッチを当てることが推奨されます。2 つの方法があります:

1. [magiskboot](https://github.com/topjohnwu/Magisk/releases)
2. [magiskboot_build](https://github.com/ookiineko/magiskboot_build/releases/tag/last-ci)

公式の `magisikboot` ビルドは Android 端末でしか動作しないため、PC で作業したい場合、2 つめの方法を試してください。

:::tip
Android-Image-Kitchen は現在非推奨です。セキュリティパッチレベルなどの boot メタデータを正しく扱わないため、一部のデバイスでは動作しない可能性があります。
:::

### 準備

1. お使いのデバイスの純正 boot.img を入手します。デバイスメーカーから入手できます。[payload-dumper-go](https://github.com/ssut/payload-dumper-go)が必要かもしれません。
2. お使いのデバイスの KMI バージョンに合った、KernelSU が提供する AnyKernel3 の ZIP ファイルをダウンロードします（*カスタムリカバリーでインストール*を参照してください）。
3. AnyKernel3 パッケージを展開し、KernelSU のカーネルファイルである `Image` ファイルを取得します。

### Android 端末で magiskboot を使う {#using-magiskboot-on-Android-devices}

1. 最新の Magisk を[リリースページ](https://github.com/topjohnwu/Magisk/releases)からダウンロードしてください。
2. `Magisk-*(version).apk` を `Magisk-*.zip` に名前を変更して展開してください。
3. `Magisk-*/lib/arm64-v8a/libmagiskboot.so`を adb でデバイスに転送します：`adb push Magisk-*/lib/arm64-v8a/libmagiskboot.so /data/local/tmp/magiskboot`
4. 純正 boot.img と AnyKernel3 の中の Image をデバイスに転送します。
5. adb shell に入り、`cd /data/local/tmp/` し、`chmod +x magiskboot` を実行します。
6. adb shell に入り、`cd /data/local/tmp/` し、`./magiskboot unpack boot.img` を実行して `boot.img` を抽出します。`kernel` ファイルが純正カーネルです。
7. `kernel` を `Image` で置き換えます: `mv -f Image kernel`
8. `./magiskboot repack boot.img` を実行してブートイメージをリパックします。出来上がった `new-boot.img` を fastboot でデバイスに書き込んでください。

### Windows/macOS/Linux PC で magiskboot を使う {#using-magiskboot-on-PC}

1. OS に対応する `magiskboot` バイナリを [magiskboot_build](https://github.com/ookiineko/magiskboot_build/releases/tag/last-ci) からダウンロードします。
2. 純正の `boot.img` と `Image` を PC 上に準備します。
3. `chmod +x magiskboot` を実行します。
4. 対応するディレクトリに入り、`./magiskboot unpack boot.img` を実行して `boot.img` を展開します。展開された `kernel` ファイルが純正のカーネルです。
5. 下記のコマンドを実行し、`kernel` を `Image` で置き換えてください: `mv -f Image kernel`
6. `./magiskboot repack boot.img` を実行し、boot イメージを再パックします。作成された `new-boot.img` ファイルを、fastboot を利用して端末にフラッシュします。

::: info
公式の `magiskboot` は、`Linux` 環境においては正常に動作します。Linux を使っている場合、公式のビルドを利用できます。
:::

## カスタムリカバリーでインストール {#install-with-custom-recovery}

前提条件：デバイスに TWRP などのカスタムリカバリーがあること。ない場合、または公式リカバリーしかない場合は他の方法を使用してください。

手順:

1. KernelSUの[リリースページ](https://github.com/tiann/KernelSU/releases)から、お使いのデバイスのバージョンにあった AnyKernel3 で始まる ZIP パッケージをダウンロードします。例えば、デバイスのカーネルのバージョンが`android12-5.10. 66`の場合、AnyKernel3-android12-5.10.66_yyyy-MM.zip`（yyyy`は年、`MM`は月）のファイルをダウンロードします。
2. デバイスを TWRP へ再起動します。
3. adb を使用して AnyKernel3-*.zip をデバイスの /sdcard に入れ、TWRP GUI でインストールを選択します。または直接`adb sideload AnyKernel-*.zip` でインストールできます。

この方法は TWRP を使用できるならどのようなインストール（初期インストールやその後のアップグレード）にも適しています。

## その他の方法

実はこれらのインストール方法はすべて、**元のカーネルを KernelSU が提供するカーネルに置き換える**という主旨でしかなく、これが実現できれば他の方法でもインストール可能です：

1. まず Magisk をインストールし、Magisk を通じて root 権限を取得し、カーネル管理アプリで KernelSU の AnyKernel ZIPをインストールする
2. PC 上で何らかの書き込みツールを使用し、KernelSU が提供するカーネルを書き込む

しかし、これらの方法でうまく行かない場合、`magiskboot` を使う方法を試してみてください。
