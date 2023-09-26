# FAQ

## KernelSU oferece suporte ao meu dispositivo?

Primeiro, seus dispositivos devem ser capazes de desbloquear o bootloader. Se não puder, então não há suporte.

Em seguida, instale o app gerenciador KernelSU em seu dispositivo e abra-o, se mostrar `Sem suporte` então seu dispositivo não pode ser suportado imediatamente, mas você pode construir a fonte do kernel e integrar o KernelSU para fazê-lo funcionar ou usar [dispositivos de suporte não oficial](unofficially-support-devices).

## O KernelSU precisa desbloquear o Bootloader?

Certamente, sim

## O KernelSU suporta módulos?

Sim, mas está na versão inicial, pode apresentar bugs. Aguarde até que fique estável :)

## KernelSU suporta Xposed?

Sim, [Dreamland](https://github.com/canyie/Dreamland) e [TaiChi](https://taichi.cool) funcionam agora. Para o LSPosed, você pode fazer funcionar [Zygisk on KernelSU](https://github.com/Dr-TSNG/ZygiskOnKernelSU)

## KernelSU suporta Zygisk?

KernelSU não tem suporte integrado ao Zygisk, mas você pode usar [Zygisk on KernelSU](https://github.com/Dr-TSNG/ZygiskOnKernelSU).

## O KernelSU é compatível com Magisk?

O sistema de módulos do KernelSU está em conflito com a montagem mágica do Magisk, se houver algum módulo habilitado no KernelSU, então todo o Magisk não funcionaria.

Mas se você usar apenas o `su` do KernelSU, então funcionará bem com o Magisk: KernelSU modifica o `kernel` e o Magisk modifica o `ramdisk`, eles podem trabalhar juntos.

## O KernelSU substituirá o Magisk?

Achamos que não e esse não é o nosso objetivo. Magisk é bom o suficiente para solução raiz do espaço do usuário e terá uma vida longa. O objetivo do KernelSU é fornecer uma interface de kernel aos usuários, não substituindo o Magisk.

## O KernelSU pode oferecer suporte a dispositivos não GKI?

É possível. Mas você deve baixar o código-fonte do kernel e integrar o KernelSU à árvore de código-fonte e compilar o kernel você mesmo.

## O KernelSU pode oferecer suporte a dispositivos abaixo do Android 12?

É o kernel do dispositivo que afeta a compatibilidade do KernelSU e não tem nada a ver com a versão do Android. A única restrição é que os dispositivos lançados com Android 12 devem ser kernel 5.10+ (dispositivos GKI). Então:

1. Os dispositivos lançados com Android 12 devem ser compatíveis.
2. Dispositivos com kernel antigo (alguns dispositivos Android 12 também têm kernel antigo) são compatíveis (você mesmo deve construir o kernel)

## O KernelSU pode suportar kernel antigo?

É possível, o KernelSU é portado para o kernel 4.14 agora, para o kernel mais antigo, você precisa fazer o backport manualmente e PRs são bem-vindos!

## Como integrar o KernelSU para o kernel antigo?

Por favor, consulte a guia [Como integrar o KernelSU para kernels não GKI](how-to-integrate-for-non-gki)

## Por que minha versão do Android é 13 e o kernel mostra “android12-5.10”?

A versão do Kernel não tem nada a ver com a versão do Android, se você precisar fazer o flash do kernel, use sempre a versão do kernel, a versão do Android não é tão importante.

## Existe algum namespace de montagem --mount-master/global no KernelSU?

Não existe agora (talvez no futuro), mas há muitas maneiras de mudar manualmente para o namespace de montagem global, como:

1. `nsenter -t 1 -m sh` para obter um shell no namespace de montagem global.
2. Adicione `nsenter --mount=/proc/1/ns/mnt` ao comando que você deseja executar, o comando será executado no namespace de montagem global. O KernelSU também está [usando desta forma](https://github.com/tiann/KernelSU/blob/77056a710073d7a5f7ee38f9e77c9fd0b3256576/manager/app/src/main/java/me/weishu/kernelsu/ui/util/KsuCli.kt#L115)

## Eu sou GKI1.0, posso usar isso?

GKI1 é completamente diferente do GKI2, você deve compilar o kernel sozinho.
