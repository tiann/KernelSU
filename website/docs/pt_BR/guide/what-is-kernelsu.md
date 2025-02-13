# O que é KernelSU?

O KernelSU é uma solução root para dispositivos Android GKI, funciona no modo kernel e concede privilégios root para apps do espaço do usuário diretamente no espaço do kernel.

## Características

A principal característica do KernelSU é que ele é baseado em kernel. O KernelSU funciona no modo kernel, portanto pode fornecer uma interface de kernel que nunca tivemos antes. Por exemplo, é possível adicionar pontos de interrupção de hardware a qualquer processo no modo kernel, acessar a memória física de qualquer processo de forma invisível, interceptar qualquer chamada de sistema (syscall) no espaço do kernel, entre outras funcionalidades.

E também, o KernelSU fornece um sistema de módulos via OverlayFS, que permite carregar seu plugin personalizado no sistema. Ele também fornece um mecanismo para modificar arquivos na partição `/system`.

## Como usar o KernelSU?

Por favor, consulte: [Instalação](installation)

## Como compilar o KernelSU?

Por favor, consulte: [Como compilar](how-to-build)

## Discussão

- Telegram: [@KernelSU](https://t.me/KernelSU)
