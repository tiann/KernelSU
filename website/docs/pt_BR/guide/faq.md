# Perguntas frequentes

## KernelSU oferece suporte ao meu dispositivo?

O KernelSU suporta dispositivos rodando Android com bootloader desbloqueado. No entanto, o suporte oficial é apenas para kernels Linux GKI 5.10+ (na prática isso significa que seu dispositivo precisa ter Android 12 de fábrica para ser compatível).

Você pode verificar facilmente o suporte para o seu dispositivo através do gerenciador do KernelSU, que está disponível [aqui](https://github.com/tiann/KernelSU/releases). 

Se o app mostrar `Não instalado`, significa que seu dispositivo é oficialmente suportado pelo KernelSU.

Se o app mostrar `Sem suporte`, significa que seu dispositivo não é oficialmente suportado no momento. No entanto, você pode compilar o código-fonte do kernel e integrar o KernelSU para fazê-lo funcionar, ou usar [Dispositivos com suporte não oficial](unofficially-support-devices).

## Para usar o KernelSU precisa desbloquear o bootloader?

Certamente, sim.

## KernelSU suporta módulos?

Sim, a maioria dos módulos Magisk funcionam no KernelSU. Verifique [Guias de módulo](module.md) para mais informações.

## KernelSU suporta Xposed?

Sim, você pode usar LSPosed (ou outro derivado moderno do Xposed) com [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext).

## KernelSU suporta Zygisk?

KernelSU não tem suporte integrado ao Zygisk, mas você pode usar um módulo como [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext) para suportá-lo.

## KernelSU é compatível com o Magisk?

O sistema de módulos do KernelSU está em conflito com a montagem mágica do Magisk. Se houver algum módulo ativado no KernelSU, todo o Magisk deixará de funcionar.

No entanto, se você usar apenas o `su` do KernelSU, ele funcionará bem com o Magisk. O KernelSU modifica o `kernel`, enquanto o Magisk modifica o `ramdisk`, permitindo que ambos trabalhem juntos.

## KernelSU substituirá o Magisk?

Acreditamos que não, e esse não é o nosso objetivo. O Magisk é bom o suficiente para solução root do espaço do usuário e terá uma longa vida. O objetivo do KernelSU é fornecer uma interface de kernel aos usuários, não substituindo o Magisk.

## KernelSU oferece suporte a dispositivos não-GKI?

É possível. Mas você deve baixar o código-fonte do kernel e integrar o KernelSU à árvore do código-fonte e compilar o kernel você mesmo.

## KernelSU oferece suporte a dispositivos abaixo do Android 12?

É o kernel do dispositivo que afeta a compatibilidade do KernelSU e não tem nada a ver com a versão do Android. A única restrição é que os dispositivos lançados com Android 12 devem ser kernel 5.10+ (dispositivos GKI). Então:

1. Os dispositivos lançados com Android 12 devem ser compatíveis.
2. Dispositivos com kernel antigo (alguns dispositivos com Android 12 também têm o kernel antigo) são compatíveis (você mesmo deve compilar o kernel).

## KernelSU suporta kernel antigo?

É possível, o KernelSU é portado para o kernel 4.14 agora. Para kernels mais antigo, você precisa portar manualmente e PRs são sempre bem-vindas!

## Como integrar o KernelSU para um kernel antigo?

Por favor, verifique o guia [Integração para dispositivos não-GKI](how-to-integrate-for-non-gki).

## Por que a minha versão do Android é 13 e o kernel mostra "android12-5.10"?

A versão do Kernel não tem nada a ver com a versão do Android. Se você precisar fazer o flash do kernel, use sempre a versão do kernel, a versão do Android não é tão importante.

## Eu sou GKI 1.0, posso usar isso?

GKI 1.0 é completamente diferente do GKI 2.0, você deve compilar o kernel sozinho.

## Como posso fazer `/system` RW?

Não recomendamos que você modifique a partição do sistema diretamente. Por favor, verifique [Guias de módulo](module.md) para modificá-lo sem sistema. Se você insiste em fazer isso, verifique [magisk_overlayfs](https://github.com/HuskyDG/magic_overlayfs).

## KernelSU pode modificar hosts? Como posso usar AdAway?

Claro. Mas o KernelSU não tem suporte a hosts integrados, você pode instalar um módulo como [systemless-hosts](https://github.com/symbuzzer/systemless-hosts-KernelSU-module) para fazer isso.

## Por que existe um enorme arquivo de 1 TB?

O arquivo `modules.img` de 1 TB é um arquivo de imagem de disco. **Não se preocupe com seu tamanho**; ele é um tipo especial de arquivo conhecido como [arquivo esparso](https://en.wikipedia.org/wiki/Sparse_file). Seu tamanho real é apenas o tamanho do módulo que você usa e diminuirá dinamicamente após a exclusão do módulo. Na verdade, ele não ocupa 1 TB de espaço em disco (seu celular pode não ter tanto espaço).

Se você realmente se incomodar com o tamanho desse arquivo, você pode usar o comando `resize2fs -M` para ajustá-lo ao tamanho real. Porém, o módulo pode não funcionar corretamente nesse caso, e não forneceremos suporte para isso.

## Por que meu dispositivo mostra o tamanho de armazenamento errado?

Certos dispositivos usam métodos não padrão para calcular o tamanho de armazenamento do dispositivo, o que pode levar a cálculos imprecisos nos apps e menus do sistema, especialmente ao lidar com arquivos esparsos de 1 TB. Embora esse problema pareça ser específico para os dispositivos Samsung, afetando principalmente os apps e serviços da Samsung, é importante observar que a discrepância está principalmente no tamanho total do armazenamento, enquanto o cálculo do espaço livre permanece preciso.
