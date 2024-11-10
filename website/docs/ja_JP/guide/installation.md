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

### Kernel バージョンと Android バージョンの違い

注意： **カーネルバージョンと Android バージョンは必ずしも同じではありません**。

カーネルのバージョンは「android12-5.10.101」なのに、Android システムのバージョンは「Android 13」などとなっている場合、驚かないでください。Linux カーネルのバージョン番号は、必ずしも**デバイスの出荷時**にプリインストールされている Android システムのバージョンと一致していません。Android システムが後でアップグレードされた場合、一般的にはカーネルのバージョンは変更されません。書き込む際は、**必ずカーネルバージョンを参照してください**!!!

## インストール方法

KernelSU のインストール方法はいくつかあり、それぞれ適したシーンが異なりますので、必要に応じて選択してください。

1. カスタムリカバリー（TWRPなど）でインストールする
2. Franco Kernel Manager などのカーネル管理アプリでインストールする
3. KernelSU が提供する boot.img を使用し、fastboot でインストールする
4. boot.img を手動でパッチしてインストールする

## カスタムリカバリーでインストール

前提条件：デバイスに TWRP などのカスタムリカバリーがあること。ない場合、または公式リカバリーしかない場合は他の方法を使用してください。

手順:

1. KernelSUの[リリースページ](https://github.com/tiann/KernelSU/releases)から、お使いのデバイスのバージョンにあった AnyKernel3 で始まる ZIP パッケージをダウンロードします。例えば、デバイスのカーネルのバージョンが`android12-5.10. 66`の場合、AnyKernel3-android12-5.10.66_yyyy-MM.zip`（yyyy`は年、`MM`は月）のファイルをダウンロードします。
2. デバイスを TWRP へ再起動します。
3. adb を使用して AnyKernel3-*.zip をデバイスの /sdcard に入れ、TWRP GUI でインストールを選択します。または直接`adb sideload AnyKernel-*.zip` でインストールできます。

この方法は TWRP を使用できるならどのようなインストール（初期インストールやその後のアップグレード）にも適しています。

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

## boot.img を手動でパッチ

デバイスによっては、boot.img のフォーマットが `lz4` でない、`gz` である、無圧縮であるなど、あまり一般的でないことがあります。最も典型的なのは Pixel で、boot.img フォーマットは `lz4_legacy` 圧縮、RAM ディスクは `gz` か `lz4_legacy` 圧縮です。この時、KernelSU が提供した boot.img を直接書き込むとデバイスが起動できなくなる場合があります。その場合は手動で boot.img に対してパッチしてください。

パッチ方式は一般的に2種類あります：

1. [Android-Image-Kitchen](https://forum.xda-developers.com/t/tool-android-image-kitchen-unpack-repack-kernel-ramdisk-win-android-linux-mac.2073775/)
2. [magiskboot](https://github.com/topjohnwu/Magisk/releases)

このうち、Android-Image-Kitchen は PC での操作に適しており、magiskboot はデバイスとの連携が必要です。

### 準備

1. お使いのデバイスの純正 boot.img を入手します。デバイスメーカーから入手できます。[payload-dumper-go](https://github.com/ssut/payload-dumper-go)が必要かもしれません。
2. お使いのデバイスの KMI バージョンに合った、KernelSU が提供する AnyKernel3 の ZIP ファイルをダウンロードします（*カスタムリカバリーでインストール*を参照してください）。
3. AnyKernel3 パッケージを展開し、KernelSU のカーネルファイルである `Image` ファイルを取得します。

### Android-Image-Kitchen を使う

1. Android-Image-Kitchen を PC にダウンロードします。
2. 純正 boot.img を Android-Image-Kitchen のルートフォルダに入れます。
3. Android-Image-Kitchen のルートディレクトリで `./unpackimg.sh boot.img` を実行して、boot.imgを展開します。
4. `split_img` ディレクトリの `boot.img-kernel` を AnyKernel3 から展開した `Image` に置き換えます（boot.img-kernelに名前が変わっていることに注意してください）。
5. Android-Image-Kitchen のルートディレクトリで `./repackimg.sh` を実行すると、 `image-new.img` というファイルが生成されます。

### magiskboot を使う

1. 最新の Magisk を[リリースページ](https://github.com/topjohnwu/Magisk/releases)からダウンロードしてください。
2. `Magisk-*(version).apk` を `Magisk-*.zip` に名前を変更して展開してください。
3. `Magisk-*/lib/arm64-v8a/libmagiskboot.so`を adb でデバイスに転送します：`adb push Magisk-*/lib/arm64-v8a/libmagiskboot.so /data/local/tmp/magiskboot`
4. 純正 boot.img と AnyKernel3 の中の Image をデバイスに転送します。
5. adb shell に入り、`cd /data/local/tmp/` し、`chmod +x magiskboot` を実行します。
6. adb shell に入り、`cd /data/local/tmp/` し、`./magiskboot unpack boot.img` を実行して `boot.img` を抽出します。`kernel` ファイルが純正カーネルです。
7. `kernel` を `Image` で置き換えます: `mv -f Image kernel`
8. `./magiskboot repack boot.img` を実行してブートイメージをリパックします。出来上がった `new-boot.img` を fastboot でデバイスに書き込んでください。

## その他の方法

実はこれらのインストール方法はすべて、**元のカーネルを KernelSU が提供するカーネルに置き換える**という主旨でしかなく、これが実現できれば他の方法でもインストール可能です：

1. まず Magisk をインストールし、Magisk を通じて root 権限を取得し、カーネル管理アプリで KernelSU の AnyKernel ZIPをインストールする
2. PC 上で何らかの書き込みツールを使用し、KernelSU が提供するカーネルを書き込む
