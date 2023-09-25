# Instalação

## Verifique se o seu dispositivo é compatível

Baixe o app gerenciador KernelSU em [GitHub Releases](https://github.com/tiann/KernelSU/releases) ou [Coolapk market](https://www.coolapk.com/apk/me.weishu.kernelsu), e instale-o no seu dispositivo:

- Se o app mostrar `Sem suporte`, significa que **você deve compilar o kernel sozinho**, o KernelSU não fornecerá e nunca fornecerá uma boot image para você atualizar.
- Se o app mostrar `Não instalado`, então seu dispositivo é oficialmente suportado pelo KernelSU.

::: info INFORMAÇÕES
Para dispositivos mostrando `Sem suporte`, aqui está os [Dispositivos com suporte não oficial](unofficially-support-devices.md), você mesmo pode compilar o kernel.
:::

## Backup padrão boot.img

Antes de atualizar, você deve primeiro fazer backup do seu boot.img padrão. Se você encontrar algum bootloop, você sempre pode restaurar o sistema voltando para o boot de fábrica usando o fastboot.

::: warning AVISO
Flashar pode causar perda de dados, certifique-se de executar esta etapa bem antes de prosseguir para a próxima! Você também pode fazer backup de todos os dados do seu telefone, se necessário.
:::

## Conhecimento necessário

### ADB e fastboot

Por padrão, você usará as ferramentas ADB e fastboot neste tutorial, portanto, se você não as conhece, recomendamos pesquisar para aprender sobre elas primeiro.

### KMI

Kernel Module Interface (KMI), versões de kernel com o mesmo KMI são **compatíveis** Isso é o que "geral" significa no GKI; por outro lado, se o KMI for diferente, então esses kernels não são compatíveis entre si, e atualizar uma imagem do kernel com um KMI diferente do seu dispositivo pode causar um bootloop.

Especificamente, para dispositivos GKI, o formato da versão do kernel deve ser o seguinte:

```txt
KernelRelease :=
Version.PatchLevel.SubLevel-AndroidRelease-KmiGeneration-suffix
w      .x         .y       -zzz           -k            -something
```

`w.x-zzz-k` é a versão KMI. Por exemplo, se a versão do kernel de um dispositivo for `5.10.101-android12-9-g30979850fc20`, então seu KMI será `5.10-android12-9`; teoricamente, ele pode inicializar normalmente com outros kernels KMI.

::: tip DICA
Observe que o SubLevel na versão do kernel não faz parte do KMI! Isso significa que `5.10.101-android12-9-g30979850fc20` tem o mesmo KMI que `5.10.137-android12-9-g30979850fc20`!
:::

### Versão do kernel vs Versão do Android

Por favor, observe: **A versão do kernel e a versão do Android não são necessariamente iguais!**

Se você descobrir que a versão do seu kernel é `android12-5.10.101`, mas a versão do seu sistema Android é Android 13 ou outra; não se surpreenda, pois o número da versão do sistema Android não é necessariamente igual ao número da versão do kernel Linux; O número da versão do kernel Linux geralmente é consistente com a versão do sistema Android que acompanha o **dispositivo quando ele é enviado**. Se o sistema Android for atualizado posteriormente, a versão do kernel geralmente não será alterada. Se você precisar atualizar, **por favor, consulte sempre a versão do kernel!!**

## Introdução

Existem vários métodos de instalação do KernelSU, cada um adequado para um cenário diferente, portanto escolha conforme necessário.

1. Instalar com Recovery personalizado (por exemplo, TWRP)
2. Instalar com um app kernel flash, como Franco Kernel Manager
3. Instalar com fastboot usando boot.img fornecido por KernelSU
4. Repare o boot.img manualmente e instale-o

## Instalar com Recovery personalizado

Pré-requisito: Seu dispositivo deve ter um Recovery personalizado, como TWRP; se não ou apenas o Recovery oficial estiver disponível, use outro método.

Etapa:

1. Na [página de lançamento](https://github.com/tiann/KernelSU/releases) do KernelSU, baixe o pacote zip começando com AnyKernel3 que corresponde à versão do seu telefone; por exemplo, a versão do kernel do telefone é `android12-5.10. 66`, então você deve baixar o arquivo `AnyKernel3-android12-5.10.66_yyyy-MM.zip` (onde `yyyy` é o ano e `MM` é o mês).
2. Reinicie o telefone no TWRP.
3. Use o adb para colocar AnyKernel3-*.zip no telefone /sdcard e escolha instalá-lo na interface do TWRP; ou você pode diretamente `adb sideload AnyKernel-*.zip` para instalar.

PS. Este método é adequado para qualquer instalação (não limitado à instalação inicial ou atualizações subsequentes), desde que você use TWRP.

## Instalar com Kernel Flasher

Pré-requisito: Seu dispositivo deve estar rooteado. Por exemplo, você instalou o Magisk para obter root ou instalou uma versão antiga do KernelSU e precisa atualizar para outra versão do KernelSU; se o seu dispositivo não estiver rooteado, tente outros métodos.

Etapa:

1. Baixe o zip AnyKernel3; consulte a seção *Instalando com Recovery Personalizado* para obter instruções de download.
2. Abra o app Kernel Flash e use o zip AnyKernel3 fornecido para fazer o flash.

Se você nunca usou algum app kernel flash antes, os seguintes são os mais populares.

1. [Kernel Flasher](https://github.com/capntrips/KernelFlasher/releases)
2. [Franco Kernel Manager](https://play.google.com/store/apps/details?id=com.franco.kernel)
3. [Ex Kernel Manager](https://play.google.com/store/apps/details?id=flar2.exkernelmanager)

PS. Este método é mais conveniente ao atualizar o KernelSU e pode ser feito sem um computador (backup primeiro).

## Instale com boot.img fornecido por KernelSU

Este método não requer que você tenha TWRP, nem que seu telefone tenha privilégios de root; é adequado para sua primeira instalação do KernelSU.

### Encontre o boot.img adequado

O KernelSU fornece um boot.img genérico para dispositivos GKI e você deve liberar o boot.img para a partição de inicialização do dispositivo.

Você pode baixar o boot.img em [GitHub Release](https://github.com/tiann/KernelSU/releases), por favor, observe que você deve usar a versão correta do boot.img. Por exemplo, se o seu dispositivo exibe o kernel `android12-5.10.101` , você precisa baixar `android-5.10.101_yyyy-MM.boot-<format>.img`. (Mantenha o KMI consistente!)

Onde `<format>` se refere ao formato de compactação do kernel do seu boot.img oficial, por favor, verifique o formato de compactação do kernel do seu boot.img original, você deve usar o formato correto, por exemplo: `lz4`, `gz`; se você usar um formato de compactação incorreto, poderá encontrar bootloop.

::: info INFORMAÇÕES
1. Você pode usar o magiskboot para obter o formato de compactação de seu boot original; é claro que você também pode perguntar a outras crianças mais experientes com o mesmo modelo do seu dispositivo. Além disso, o formato de compactação do kernel geralmente não muda; portanto, se você inicializar com êxito com um determinado formato de compactação, poderá tentar esse formato mais tarde.
2. Os dispositivos Xiaomi geralmente usam `gz` ou **uncompressed**.
3. Para dispositivos Pixel, siga as instruções abaixo.
:::

### Flash boot.img para o dispositivo

Use o `adb` para conectar seu dispositivo, execute `adb reboot bootloader` para entrar no modo fastboot e use este comando para atualizar o KernelSU:

```sh
fastboot flash boot boot.img
```

::: info INFORMAÇÕES
Se o seu dispositivo suportar `fastboot boot`, você pode primeiro usar `fastboot boot boot.img` para tentar usar o boot.img para inicializar o sistema primeiro. Se algo inesperado acontecer, reinicie-o novamente para inicializar.
:::

### Reiniciar

Após a conclusão do flash, você deve reiniciar o dispositivo:

```sh
fastboot reboot
```

## Corrigir boot.img manualmente

Para alguns dispositivos, o formato boot.img não é tão comum, como `lz4`, `gz` e `uncompressed`; o mais típico é o Pixel, seu formato boot.img é `lz4_legacy` compactado, ramdisk pode ser `gz` também pode ser compactado `lz4_legacy`; neste momento, se você atualizar diretamente o boot.img fornecido pelo KernelSU, o telefone pode não conseguir inicializar; neste momento, você pode corrigir manualmente o boot.img para conseguir isso.

Geralmente existem dois métodos de patch:

1. [Android-Image-Kitchen](https://forum.xda-developers.com/t/tool-android-image-kitchen-unpack-repack-kernel-ramdisk-win-android-linux-mac.2073775/)
2. [magiskboot](https://github.com/topjohnwu/Magisk/releases)

Entre eles, o Android-Image-Kitchen é adequado para operação no PC e o magiskboot precisa da cooperação do telefone.

### Preparação

1. Obtenha o stock boot.img do telefone; você pode obtê-lo com os fabricantes do seu dispositivo, você pode precisar do [payload-dumper-go](https://github.com/ssut/payload-dumper-go)
2. Baixe o arquivo zip AnyKernel3 fornecido pelo KernelSU que corresponde à versão KMI do seu dispositivo (você pode consultar *Instalar com Recovery personalizado*).
3. Descompacte o pacote AnyKernel3 e obtenha o arquivo `Image`, que é o arquivo do kernel do KernelSU.

### Usando Android-Image-Kitchen

1. Baixe o Android-Image-Kitchen para o seu computador.
2. Coloque o stock boot.img na pasta raiz do Android-Image-Kitchen.
3. Execute `./unpackimg.sh boot.img` no diretório raiz do Android-Image-Kitchen, este comando descompactará o boot.img e você obterá alguns arquivos.
4. Substitua `boot.img-kernel` no diretório `split_img` pela `Image` que você extraiu do AnyKernel3 (observe a mudança de nome para boot.img-kernel).
5. Execute `./repackimg.sh` no diretório raiz de 在 Android-Image-Kitchen; E você obterá um arquivo chamado `image-new.img`; Atualize este boot.img por fastboot (consulte a seção anterior).

### Usando magiskboot

1. Baixe o Magisk mais recente em [GitHub Releases](https://github.com/topjohnwu/Magisk/releases)
2. Renomeie o Magisk-*.apk para Magisk-vesion.zip e descompacte-o.
3. Envie `Magisk-v25.2/lib/arm64-v8a/libmagiskboot.so` para o seu dispositivo por adb: `adb push Magisk-v25.2/lib/arm64-v8a/libmagiskboot.so /data/local/tmp/magiskboot`
4. Envie stock boot.img e Image em AnyKernel3 para o seu dispositivo.
5. Entre no diretório adb shell e cd `/data/local/tmp/` e, em seguida, `chmod +x magiskboot`
6. Entre no shell adb e no diretório cd `/data/local/tmp/`, execute `./magiskboot unpack boot.img` para descompactar `boot.img`, você obterá um arquivo `kernel`, este é o seu kernel padrão.
7. Substitua `kernel` por `Image`: `mv -f Image kernel`
8. Execute `./magiskboot repack boot.img` para reembalar o boot img, e você obterá um arquivo `new-boot.img`, atualize este arquivo para o dispositivo por fastboot.

## Outros métodos

Na verdade, todos esses métodos de instalação têm apenas uma ideia principal, que é **substituir o kernel original pelo fornecido pelo KernelSU**; desde que isso possa ser alcançado, ele pode ser instalado; por exemplo, a seguir estão outros métodos possíveis.

1. Primeiro instale o Magisk, obtenha privilégios de root através do Magisk e então use o kernel flasher para atualizar no zip AnyKernel do KernelSU.
2. Use algum kit de ferramentas de atualização em PCs para atualizar no kernel fornecido KernelSU.
