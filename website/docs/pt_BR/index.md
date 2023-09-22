---
layout: home
title: Uma solução root baseada em kernel para Android

hero:
  name: KernelSU
  text: Uma solução root baseada em kernel para Android
  tagline: ""
  image:
    src: /logo.png
    alt: KernelSU
  actions:
    - theme: brand
      text: Iniciar
      link: /pt_BR/guide/what-is-kernelsu
    - theme: alt
      text: Ver no GitHub
      link: https://github.com/tiann/KernelSU

features:
  - title: Baseado em kernel
    details: KernelSU está funcionando no modo kernel Linux, tem mais controle sobre os apps do espaço do usuário.
  - title: Controle de acesso à lista de permissões
    details: Somente apps que recebem permissão de root podem acessar su, outros apps não podem perceber su.
  - title: Permissão de root restrita
    details: KernelSU permite que você personalize o uid, gid, grupos, recursos e regras SELinux do su. Tranque o poder root em uma gaiola.
  - title: Módulo e Código aberto
    details: KernelSU suporta modificação /system sem sistema por overlayfs e é de código aberto sob GPL-3.
