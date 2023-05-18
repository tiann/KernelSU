# よくある質問

## 私のデバイスは KernelSU に対応していますか?

まず、お使いのデバイスがブートローダーのロックを解除できる必要があります。もしできないのであれば、サポート外です。

もし KernelSU アプリで「非対応」と表示されたら、そのデバイスは最初からサポートされていないことになりますが、カーネルソースをビルドして KernelSU を組み込むか、[非公式の対応デバイス](unofficially-support-devices)で動作させることが可能です。

## KernelSU を使うにはブートローダーのロックを解除する必要がありますか？

はい。

## KernelSU はモジュールに対応していますか?

はい。ただし初期バージョンであるためバグがある可能性があります。安定するのをお待ちください。

## KernelSU は Xposed に対応していますか?

はい。[Dreamland](https://github.com/canyie/Dreamland) や [TaiChi](https://taichi.cool) が動作します。LSPosed については、[Zygisk on KernelSU](https://github.com/Dr-TSNG/ZygiskOnKernelSU) を使うと動作するようにできます。

## KernelSU は Zygisk に対応していますか?

KernelSU は Zygisk サポートを内蔵していません。[Zygisk on KernelSU](https://github.com/Dr-TSNG/ZygiskOnKernelSU) を使ってください。

## KernelSU は Magisk と互換性がありますか?

KernelSU のモジュールシステムは Magisk のマジックマウントと競合しており、KernelSU で有効になっているモジュールがある場合、Magisk 全体が動作しなくなります。

しかし、KernelSU の `su` だけを使うのであれば、Magisk とうまく連携することができます。KernelSU は `kernel` を、Magisk は `ramdisk` を修正するため、両者は共存できます。

## KernelSU は Magisk の代わりになりますか？

私たちはそうは思っていませんし、それが目標でもありません。Magisk はユーザ空間の root ソリューションとして十分であり、長く使われ続けるでしょう。KernelSU の目標は、ユーザーにカーネルインターフェースを提供することであり、Magisk の代用ではありません。

## KernelSU は GKI 以外のデバイスに対応できますか？

可能です。ただしカーネルソースをダウンロードし、KernelSU をソースツリーに統合して、自分でカーネルをビルドする必要があります。

## KernelSU は Android 12 以下のデバイスに対応できますか？

KernelSU の互換性に影響を与えるのはデバイスのカーネルであり、Android のバージョンとは無関係です。唯一の制限は、Android 12 で発売されたデバイスはカーネル5.10以上（GKI デバイス）でなければならないことです：

1. Android 12 をプリインストールして発売された端末は対応しているはずです。
2. カーネルが古い端末（一部の Android 12 端末はカーネルも古い）は対応可能ですが、カーネルは自分でビルドする必要があります。

## KernelSU は古いカーネルに対応できますか？

KernelSU は現在カーネル4.14にバックポートされていますが、それ以前のカーネルについては手動でバックポートする必要があります。プルリクエスト歓迎です！

## 古いカーネルに KernelSU を組み込むには？

[ガイド](../../guide/how-to-integrate-for-non-gki) を参考にしてください。

## Android のバージョンが13なのに、カーネルは「android12-5.10」と表示されるのはなぜ？

カーネルのバージョンは Android のバージョンと関係ありません。カーネルを書き込む必要がある場合は、常にカーネルのバージョンを使用してください。Android のバージョンはそれほど重要ではありません。

## KernelSU に-mount-master/global のマウント名前空間はありますか？

今はまだありませんが（将来的にはあるかもしれません）、グローバルマウントの名前空間に手動で切り替える方法は、以下のようにたくさんあります：

1. `nsenter -t 1 -m sh` でシェルをグローバル名前空間にします。
2. `nsenter --mount=/proc/1/ns/mnt` を実行したいコマンドに追加すればグローバル名前空間で実行されます。 KernelSU は [このような使い方](https://github.com/tiann/KernelSU/blob/77056a710073d7a5f7ee38f9e77c9fd0b3256576/manager/app/src/main/java/me/weishu/kernelsu/ui/util/KsuCli.kt#L115) もできます。

## GKI 1.0 なのですが、使えますか？

GKI1 は GKI2 と全く異なるため、カーネルは自分でビルドする必要があります。
