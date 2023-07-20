---
layout: home
title: Основанное на ядре root-решение для Android

hero:
  name: KernelSU
  text: Основанное на ядре root-решение для Android
  tagline: ""
  image:
    src: /logo.png
    alt: KernelSU
  actions:
    - theme: brand
      text: Начало работы
      link: /ru_RU/guide/what-is-kernelsu
    - theme: alt
      text: Посмотр на GitHub
      link: https://github.com/tiann/KernelSU

features:
  - title: Основанный на ядре
    details: KernelSU работает в режиме ядра Linux, он имеет больше контроля над пользовательскими приложениями.
  - title: Контроль доступа по белому списку
    details: Только приложение, которому предоставлено разрешение root, может получить доступ к `su`, другие приложения не могут воспринимать su.
  - title: Ограниченные root-права
    details: KernelSU позволяет вам настраивать uid, gid, группы, возможности и правила SELinux для su. Заприте root-власть в клетке.
  - title: Модульность и открытый исходный код
    details: KernelSU поддерживает модификацию /system бессистемно с помощью overlayfs и имеет открытый исходный код под лицензией GPL-3.

