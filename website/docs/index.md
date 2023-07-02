---
layout: home
title: A kernel-based root solution for Android

hero:
  name: KernelSU
  text: A kernel-based root solution for Android
  tagline: ""
  image:
    src: /logo.png
    alt: KernelSU
  actions:
    - theme: brand
      text: Get Started
      link: /guide/what-is-kernelsu
    - theme: alt
      text: View on GitHub
      link: https://github.com/tiann/KernelSU

features:
  - title: Kernel-based
    details: KernelSU is working in Linux kernel mode, it has more control over userspace applications.
  - title: Whitelist access control
    details: Only App that is granted root permission can access `su`, other apps cannot perceive su.
  - title: Restricted root permission
    details: KernelSU allows you to customize the uid, gid, groups, capabilities and SELinux rules of su. Lock up the root power in a cage.
  - title: Module & Open source
    details: KernelSU supports modify /system systemlessly by overlayfs and it is open-sourced under GPL-3.

