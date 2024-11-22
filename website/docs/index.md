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
      text: Get started
      link: /guide/what-is-kernelsu
    - theme: alt
      text: View on GitHub
      link: https://github.com/tiann/KernelSU

features:
  - title: Kernel-based
    details: As the name suggests, KernelSU works under the Linux kernel giving it more control over userspace apps.
  - title: Root access control
    details: Only permitted apps may access or see su, all other apps are not aware of this.
  - title: Customizable root privileges
    details: KernelSU allows customization of su's uid, gid, groups, capabilities, and SELinux rules, locking up root privileges.
  - title: Modules
    details: Modules may modify /system systemlessly using OverlayFS enabling great power.
