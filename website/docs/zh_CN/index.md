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
  - title: 受限制的 root 权限
    details: KernelSU 可以自定义 `su` 的 uid, gid, groups, capabilities 和 SELinux 规则：把 root 权限关进笼子里。
  - title: Metamodule 模块系统
    details: 可插拔的模块基础架构，支持无系统修改。安装 meta-overlayfs 等 metamodule 来启用模块挂载。

