# App Profile

App Profile は KernelSU が提供する仕組みで、さまざまなアプリの設定を柔軟にカスタマイズできます。

root 権限（`su` を利用できること）を付与したアプリの場合、App Profile は Root Profile とも呼べます。`su` コマンドの `uid`、`gid`、`groups`、`capabilities`、`SELinux` ルールを調整できるため、root ユーザーの特権を細かく制限できます。たとえばファイアウォールアプリにだけネットワーク権限を与えてファイルアクセスを禁止したり、凍結アプリには完全な root の代わりに shell 権限のみを与えたりできます。**すなわち最小権限の原則で力を箱の中に閉じ込める**ことができます。

root 権限を持たない通常のアプリに対しても、App Profile は kernel やモジュールシステムがそのアプリをどのように扱うかを制御できます。モジュールによる変更をアプリで見せるべきかなどを決められ、設定に基づいて kernel／モジュールシステムは「隠す」といった挙動を選択できます。

## Root Profile

### UID・GID・グループ

Linux にはユーザーとグループの 2 つの概念があります。各ユーザーにはユーザー ID (UID) があり、ユーザーは複数のグループに所属できます。それぞれのグループにはグループ ID (GID) があり、これらの ID がシステム内でのユーザー識別やアクセス可能なリソースの判断に使われます。

UID が 0 のユーザーは root ユーザー、GID が 0 のグループは root グループと呼ばれます。root グループは一般的にシステムでもっとも強い権限を持ちます。

Android では各アプリ（shared UID を除く）が独立したユーザーとして動作し、固有の UID を持ちます。たとえば `0` は root、`1000` は `system`、`2000` は ADB shell、`10000`〜`19999` は一般アプリに割り当てられます。

::: info
ここでいう UID は Android のマルチユーザーやワークプロファイルとは別の概念です。ワークプロファイルは UID の範囲を分割することで実装されています。たとえば 10000-19999 がメインユーザー、110000-119999 がワークプロファイルを表し、その中の各アプリは独自の UID を持っています。
:::

各アプリは複数のグループに所属でき、GID はそのうちのプライマリグループを表します（多くの場合 UID と同じです）。その他のグループは補助グループです。ネットワークアクセスや Bluetooth など、いくつかの権限はグループで管理されています。

例として、ADB shell で `id` コマンドを実行すると次のような結果になります。

```sh
oriole:/ $ id
uid=2000(shell) gid=2000(shell) groups=2000(shell),1004(input),1007(log),1011(adb),1015(sdcard_rw),1028(sdcard_r),1078(ext_data_rw),1079(ext_obb_rw),3001(net_bt_admin),3002(net_bt),3003(inet),3006(net_bw_stats),3009(readproc),3011(uhid),3012(readtracefs) context=u:r:shell:s0
```

ここでは UID が `2000`、GID（プライマリグループ ID）も `2000` です。さらに `inet`（`AF_INET` や `AF_INET6` を作成できる、つまりネットワークアクセスが可能）、`sdcard_rw`（SD カードの読み書きが可能）といった補助グループにも所属しています。

KernelSU の Root Profile を使うと、`su` 実行後の root プロセスの UID・GID・所属グループをカスタマイズできます。たとえばある root アプリの Root Profile で UID を `2000` に設定すれば、そのアプリが `su` を使っても実際の権限は ADB shell 相当になります。またグループから `inet` を外せば、`su` コマンドがネットワークにアクセスできなくなります。

::: tip 注意
App Profile が制御するのは `su` 使用後の root プロセスの権限だけであり、アプリ自身の権限ではありません。アプリがネットワーク権限を要求して許可されていれば、`su` を使わなくてもネットワークにアクセスできます。`su` から `inet` グループを外すのは、`su` にネットワークアクセスをさせないためだけの措置です。
:::

Root Profile は kernel によって強制され、`su` でユーザーやグループを切り替えるようなアプリ側の自発的な動作には依存しません。`su` 権限を与えるかどうかは開発者ではなくユーザー自身が完全にコントロールできます。

### Capabilities

Capability は Linux における特権分離の仕組みです。

従来の `UNIX` 実装では、権限チェックのためにプロセスを 2 種類に分けます。effective UID が `0` の特権プロセス（superuser/root）と、それ以外の非特権プロセスです。特権プロセスは kernel の権限チェックをすべてバイパスし、非特権プロセスはプロセスの資格情報（effective UID・effective GID・補助グループ一覧など）に基づいた完全なチェックを受けます。

Linux 2.2 以降は、従来 root が一括して持っていた権限を capability と呼ばれる単位に分割し、それぞれを独立して有効化・無効化できるようになりました。

各 capability は 1 つ以上の権限を表します。たとえば `CAP_DAC_READ_SEARCH` は、ファイル読み取りやディレクトリの読み取り・実行に必要なチェックをバイパスできる権限です。effective UID が `0` のユーザー（root）であっても、`CAP_DAC_READ_SEARCH` などを持っていなければ自由にファイルを読めません。

KernelSU の Root Profile では `su` 実行後の root プロセスの capability もカスタマイズでき、いわば「部分的な root 権限」を与えられます。上で触れた UID / GID と異なり、`su` 後も UID `0` が必要な root アプリもあります。その場合は UID `0` のまま capability を制限することで、許可される操作を限定できます。

::: tip 強く推奨
Linux の capability については[公式ドキュメント](https://man7.org/linux/man-pages/man7/capabilities.7.html)に詳細がまとまっています。capability をカスタマイズする予定があるなら、まずこのドキュメントを読んでください。
:::

### SELinux

SELinux は強力な Mandatory Access Control (MAC) 仕組みで、**default deny**（明示的に許可されていない操作はすべて拒否）という原則で動作します。

SELinux には 2 つのグローバルモードがあります。

1. Permissive モード: 拒否イベントを記録しますが強制しません。
2. Enforcing モード: 拒否イベントを記録し、かつ強制します。

::: warning
近年の Android は SELinux に大きく依存してシステム全体の安全性を保っています。何のメリットもないので、「Permissive モード」で動作するカスタムシステムの利用は強く非推奨です。
:::

SELinux の全容を説明するのは非常に複雑で、このドキュメントの範囲を超えています。まずは以下の資料で仕組みを理解することをおすすめします。

1. [Wikipedia](https://en.wikipedia.org/wiki/Security-Enhanced_Linux)
2. [Red Hat: What Is SELinux?](https://www.redhat.com/en/topics/linux/what-is-selinux)
3. [ArchLinux: SELinux](https://wiki.archlinux.org/title/SELinux)

KernelSU の Root Profile では、`su` 実行後の root プロセスの SELinux コンテキストもカスタマイズできます。特定のコンテキストに対してアクセス制御ルールを定義し、root 権限をきめ細かく制御できます。

一般的なシナリオでは、アプリが `su` を実行すると `u:r:su:s0` のような**制限のない** SELinux ドメインに切り替わります。Root Profile を介して `u:r:app1:s0` のようなカスタムドメインに切り替え、そのドメイン向けに次のようなルールを定義できます。

```sh
type app1
enforce app1
typeattribute app1 mlstrustedsubject
allow app1 * * *
```

`allow app1 * * *` というルールは例示目的であり、実際にはあまり使うべきではありません。Permissive モードとほとんど変わらなくなってしまうからです。

### エスカレーション

Root Profile の設定が適切でないと、意図せず制限を回避される恐れがあります。

たとえば ADB shell ユーザーに root 権限を与えていて（よくある構成です）、さらに通常アプリにも root 権限を付与したあと、そのアプリの Root Profile で UID 2000（ADB shell の UID）を設定したとします。この場合、アプリは `su` を 2 回実行するだけで完全な root を得られます。

1. 1 回目の `su` は App Profile の制限を受け、UID を `0` ではなく `2000`（ADB shell）に変更します。
2. 2 回目の `su` は、既に UID が `2000` であり、その UID に root 権限を許可しているため、完全な root 権限を取得します。

::: warning 注意
この挙動は仕様でありバグではありません。したがって以下を推奨します。

本当に ADB に root 権限が必要な（開発者などの）場合でも、Root Profile で UID を `2000` に変更するのは避けてください。`1000`（system）を使う方が安全です。
:::

## 非 root プロファイル

### モジュールのアンマウント

KernelSU は OverlayFS をマウントすることで systemless な形でシステムパーティションを変更します。しかしこの挙動に敏感なアプリもあります。そのような場合は「Umount modules」オプションを設定して、対象アプリではモジュールをアンマウントさせることができます。

KernelSU マネージャーの設定画面には「Umount modules by default」という項目もあります。デフォルトではこのオプションは**有効**で、追加設定をしない限り KernelSU や一部モジュールはそのアプリでモジュールをアンマウントします。この挙動を望まない、あるいは一部アプリに影響する場合は、次のいずれかの方法を取ってください。

1. 「Umount modules by default」を有効のままにし、モジュールを読み込みたいアプリの App Profile では個別に「Umount modules」を無効にする（ホワイトリスト方式）。
2. 「Umount modules by default」を無効にし、アンマウントしたいアプリでのみ個別に「Umount modules」を有効にする（ブラックリスト方式）。

::: info
カーネル 5.10 以降を実行しているデバイスでは、カーネルが追加の処理なしにモジュールをアンマウントします。一方 5.10 未満のデバイスでは、このオプションは設定値を示すだけで KernelSU は何もしません。5.10 より前のカーネルで「Umount modules」を使いたい場合は、`fs/namespace.c` にある `path_umount` 関数をバックポートする必要があります。詳細は [Integrate for non-GKI devices](https://kernelsu.org/guide/how-to-integrate-for-non-gki.html#how-to-backport-path_umount) ページの末尾を参照してください。Zygisksu など一部モジュールも、モジュールをアンマウントすべきかどうかを判断するためにこのオプションを参照します。
:::
