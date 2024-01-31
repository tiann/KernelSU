# Perguntas frequentes

## KernelSU oferece suporte ao meu dispositivo?

Primeiro, seu dispositivo deve ser capaz de desbloquear o bootloader. Se não, então não há suporte.

Em seguida, instale o app gerenciador do KernelSU em seu dispositivo e abra-o, se mostrar `Sem suporte` então seu dispositivo não pode ser suportado imediatamente, mas você pode construir a fonte do kernel e integrar o KernelSU para fazê-lo funcionar ou usar [Dispositivos com suporte não oficial](unofficially-support-devices).

## Para usar o KernelSU precisa desbloquear o bootloader?

Certamente, sim.

## KernelSU suporta módulos?

Sim, verifique [Guias de módulo](module.md) por favor.

## KernelSU suporta Xposed?

Sim, você pode usar LSPosed com [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext).

## KernelSU suporta Zygisk?

KernelSU não tem suporte integrado ao Zygisk, mas você pode usar [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext).

## KernelSU é compatível com o Magisk?

O sistema de módulos do KernelSU está em conflito com a montagem mágica do Magisk, se houver algum módulo habilitado no KernelSU, então todo o Magisk não funcionaria.

Mas se você usar apenas o `su` do KernelSU, então funcionará bem com o Magisk. KernelSU modifica o `kernel` e o Magisk modifica o `ramdisk`, eles podem trabalhar juntos.

## KernelSU substituirá o Magisk?

Achamos que não e esse não é o nosso objetivo. O Magisk é bom o suficiente para solução root do espaço do usuário e terá uma longa vida. O objetivo do KernelSU é fornecer uma interface de kernel aos usuários, não substituindo o Magisk.

## KernelSU oferece suporte a dispositivos não GKI?

É possível. Mas você deve baixar o código-fonte do kernel e integrar o KernelSU à árvore do código-fonte e compilar o kernel você mesmo.

## KernelSU oferece suporte a dispositivos abaixo do Android 12?

É o kernel do dispositivo que afeta a compatibilidade do KernelSU e não tem nada a ver com a versão do Android. A única restrição é que os dispositivos lançados com Android 12 devem ser kernel 5.10+ (dispositivos GKI). Então:

1. Os dispositivos lançados com Android 12 devem ser compatíveis.
2. Dispositivos com kernel antigo (alguns dispositivos Android 12 também têm o kernel antigo) são compatíveis (você mesmo deve construir o kernel).

## KernelSU suporta kernel antigo?

É possível, o KernelSU é portado para o kernel 4.14 agora, para o kernel mais antigo, você precisa fazer o backport manualmente e PRs são sempre bem-vindas!

## Como integrar o KernelSU para o kernel antigo?

Por favor, consulte a guia [Como integrar o KernelSU para kernels não GKI](how-to-integrate-for-non-gki).

## Por que a minha versão do Android é 13 e o kernel mostra “android12-5.10”?

A versão do Kernel não tem nada a ver com a versão do Android, se você precisar fazer o flash do kernel, use sempre a versão do kernel, a versão do Android não é tão importante.

## Eu sou GKI1.0, posso usar isso?

GKI1 é completamente diferente do GKI2, você deve compilar o kernel sozinho.

## Como posso fazer `/system` RW?

Não recomendamos que você modifique a partição do sistema diretamente. Você deve usar [Guias de módulo](module.md) para modificá-lo sem sistema. Se você insiste em fazer isso, verifique [magisk_overlayfs](https://github.com/HuskyDG/magic_overlayfs).

## O KernelSU pode modificar hosts? Como posso usar AdAway?

Claro. Mas o KernelSU não tem suporte a hosts integrados, você pode instalar [systemless-hosts](https://github.com/symbuzzer/systemless-hosts-KernelSU-module) para fazer isso.

## Por que existe um enorme arquivo de 1T?

O arquivo `modules.img` de 1T é um arquivo de imagem de disco, **não se preocupe com seu tamanho**, é um tipo especial de arquivo conhecido como [arquivo esparso](https://en.wikipedia.org/wiki/Sparse_file), seu tamanho real é apenas o tamanho do módulo que você usa e diminuirá dinamicamente após você excluir o módulo. Na verdade, ele não ocupa 1T de espaço em disco (na verdade, seu celular pode não ter tanto espaço).

Se você estiver realmente insatisfeito com o tamanho deste arquivo, você pode usar o comando `resize2fs -M` para torná-lo seu tamanho real, mas o módulo pode não funcionar corretamente neste momento e não forneceremos nenhum suporte para isso.
