---
layout: home
title: Android 上的内核级的 root 方案

hero:
  name: KernelSU
  text: Android 上的内核级的 root 方案
  tagline: ""
  image:
    src: /logo.png
    alt: KernelSU
  actions:
    - theme: brand
      text: 开始了解
      link: /zh_CN/guide/what-is-kernelsu
    - theme: alt
      text: 在 GitHub 中查看
      link: https://github.com/tiann/KernelSU

features:
  - title: 基于内核
    details: KernelSU 运行在内核空间，对用户空间应用有更强的掌控。
  - title: 白名单访问控制
    details: 只有被授权的 App 才可以访问 `su`，而其他 App 无法感知其存在。
  - title: 模块支持
    details: KernelSU 支持通过 overlayfs 修改 /system，它甚至可以使 /system 可写。
  - title: 开源
    details: KernelSU 是 GPL-3 许可下的开源项目。

