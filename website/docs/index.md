---
layout: home

hero:
  name: KernelSU
  text: A kernel-based root solution for Android GKI devices
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
    details: KernslSU is working in Linux kernel mode, it has more control over userspace applications.
  - title: Whitelist access control
    details: Only App that is granted root permission can access `su`, other apps cannot perceive su.
  - title: Module support
    details: KernelSU supports modify /system systemlessly by overlayfs, it can even make system writable.
  - title: Open source
    details: KernelSU is a open source project under GPL-3 License.

