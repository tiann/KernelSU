---
layout: home
title: 基於核心的 Android Root 解決方案

hero:
  name: KernelSU
  text: 基於核心的 Android root 解決方案
  tagline: ""
  image:
    src: /logo.png
    alt: KernelSU
  actions:
    - theme: brand
      text: 開始瞭解
      link: /zh_TW/guide/what-is-kernelsu
    - theme: alt
      text: 在 GitHub 中檢視
      link: https://github.com/tiann/KernelSU

features:
  - title: 以核心為基礎
    details: KernelSU 以 Linux 核心模式運作，對使用者空間有更強的掌控。
  - title: 白名單存取控制
    details: 僅有被授予 Root 權限的應用程式才可存取 su，而其他應用程式完全無法知悉。
  - title: 可定制的 Root 權限
    details: KernelSU 能夠對 su 的使用者ID（uid）、群組ID（gid）、群組、權限，以及 SELinux 規則進行客製化管理，以此加強 root 權限的安全性。
  - title: 模組支援
    details: KernelSU 支援透過 overlayfs 修改 /system，它甚至可以使 /system 可寫入。

