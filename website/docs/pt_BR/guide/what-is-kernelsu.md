# O que é KernelSU?

O KernelSU é uma solução root para dispositivos Android GKI, funciona no modo kernel e concede privilégios root para apps do espaço do usuário diretamente no espaço do kernel.

## Características

A principal característica do KernelSU é que ele é **baseado em kernel**. O KernelSU funciona no modo kernel, portanto pode fornecer uma interface de kernel que nunca tivemos antes. Por exemplo, podemos adicionar um ponto de interrupção de hardware a qualquer processo no modo kernel, podemos acessar a memória física de qualquer processo sem que ninguém perceba, podemos interceptar qualquer syscall no espaço do kernel, etc.

E também, o KernelSU fornece um sistema de módulos via OverlayFS, que permite carregar seu plugin personalizado no sistema. Ele também fornece um mecanismo para modificar arquivos na partição `/system`.

## Como usar o KernelSU?

Por favor, consulte: [Instalação](installation)

## Como compilar o KernelSU?

Por favor, consulte: [Como compilar?](how-to-build)

## Discussão

- Telegram: [@KernelSU](https://t.me/KernelSU)
