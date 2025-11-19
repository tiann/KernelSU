---
layout: home
title: Sebuah solusi root kernel-based untuk Android

hero:
  name: KernelSU
  text: Sebuah solusi root kernel-based untuk Android
  tagline: ""
  image:
    src: /logo.png
    alt: KernelSU
  actions:
    - theme: brand
      text: Permulaan
      link: /id_ID/guide/what-is-kernelsu
    - theme: alt
      text: Lihat di GitHub
      link: https://github.com/tiann/KernelSU

features:
  - title: Kernel-based
    details: KernelSU bekerja dalam mode Linux kernel, dan mempunyai kelebihan diatas aplikasi userspace.
  - title: Kontrol akses daftar putih
    details: Hanya aplikasi yang diberikan izin root yang bisa mengakses `su`, aplikasi lain tidak bisa mengakses su.
  - title: Sistem Metamodule
    details: Infrastruktur modul yang dapat dipasang memungkinkan modifikasi systemless pada /system. Pasang metamodule seperti meta-overlayfs untuk mengaktifkan pemasangan modul.
  - title: Sumber terbuka
    details: KernelSU adalah projek sumber terbuka dibawah lisensi GPL-3.
