---
layout: home
title: Android 向けのカーネルベース root ソリューション

hero:
  name: KernelSU
  text: Android 向けのカーネルベース root ソリューション
  tagline: ""
  image:
    src: /logo.png
    alt: KernelSU
  actions:
    - theme: brand
      text: はじめる
      link: /ja_JP/guide/what-is-kernelsu
    - theme: alt
      text: GitHub で表示
      link: https://github.com/tiann/KernelSU

features:
  - title: カーネルベース
    details: KernelSU は Linux カーネルモードで動作し、ユーザー空間よりも高度な制御が可能です。
  - title: ホワイトリストの権限管理
    details: root 権限を許可したアプリのみが su にアクセスでき、他のアプリは su を見つけられません。
  - title: Metamodule システム
    details: プラグ可能なモジュールインフラストラクチャにより、systemless方式で/systemを変更可能。metamodule(meta-overlayfsなど)をインストールすることでモジュールのマウント機能を有効化。
  - title: オープンソース
    details: KernelSU は GPL-3 でライセンスされたオープンソースプロジェクトです。

