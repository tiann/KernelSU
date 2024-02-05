# Instalação

## Verifique se o seu dispositivo é compatível

Baixe o app gerenciador do KernelSU em [GitHub Releases](https://github.com/tiann/KernelSU/releases), e instale-o no seu dispositivo:

- Se o app mostrar `Sem suporte`, significa que **você deve compilar o kernel sozinho**. O KernelSU não fornecerá e nunca fornecerá uma boot.img para você instalar.
- Se o app mostrar `Não instalado`, então seu dispositivo é oficialmente suportado pelo KernelSU.

::: info INFORMAÇÕES
Para dispositivos mostrando `Sem suporte`, aqui está os [Dispositivos com suporte não oficial](unofficially-support-devices.md). Você mesmo pode compilar o kernel.
:::

## Backup padrão do boot.img

Antes de fazer o flash, você deve primeiro fazer backup de seu boot.img padrão. Se você encontrar algum bootloop, você sempre pode restaurar o sistema voltando para o boot padrão de fábrica usando o fastboot.

::: warning AVISO
Flashar pode causar perda de dados, certifique-se de executar esta etapa bem antes de prosseguir para a próxima! Você também pode fazer backup de todos os dados do seu telefone, se necessário.
:::

## Conhecimento necessário

### ADB e fastboot

Por padrão, você usará as ferramentas ADB e fastboot neste tutorial, portanto, se você não as conhece, recomendamos pesquisar para aprender sobre elas primeiro.

### KMI

Kernel Module Interface (KMI), versões de kernel com o mesmo KMI são **compatíveis**, isso é o que "geral" significa no GKI. Por outro lado, se o KMI for diferente, então esses kernels não são compatíveis entre si, e atualizar uma imagem do kernel com um KMI diferente do seu dispositivo pode causar um bootloop.

Especificamente, para dispositivos GKI, o formato da versão do kernel deve ser a seguinte:

```txt
KernelRelease :=
Version.PatchLevel.SubLevel-AndroidRelease-KmiGeneration-suffix
w      .x         .y       -zzz           -k            -something
```

`w.x-zzz-k` é a versão KMI. Por exemplo, se a versão do kernel de um dispositivo for `5.10.101-android12-9-g30979850fc20`, então seu KMI será `5.10-android12-9`. Teoricamente, ele pode inicializar normalmente com outros kernels KMI.

::: tip DICA
Observe que o SubLevel na versão do kernel não faz parte do KMI! Isso significa que `5.10.101-android12-9-g30979850fc20` tem o mesmo KMI que `5.10.137-android12-9-g30979850fc20`!
:::

### Nível do patch de segurança {#security-patch-level}

Dispositivos Android mais recentes podem ter mecanismos anti-rollback que não permitem flashar uma boot.img com um nível de patch de segurança antigo. Por exemplo, se o kernel do seu dispositivo for `5.10.101-android12-9-g30979850fc20`, o patch de segurança será `2023-11`, mesmo se você atualizar o kernel consistente com o KMI do kernel, se o nível do patch de segurança for anterior a `2023-11` (como `2023-06`), então isso pode causar bootloop.

Portanto, os kernels com os níveis de patch de segurança mais recentes são preferidos, mantendo a consistência do KMI.

### Versão do kernel vs Versão do Android

Por favor, observe: **A versão do kernel e a versão do Android não são necessariamente iguais!**

Se você descobrir que a versão do seu kernel é `android12-5.10.101`, mas a versão do seu sistema Android é Android 13 ou outra, não se surpreenda, pois o número da versão do sistema Android não é necessariamente igual ao número da versão do kernel Linux. O número da versão do kernel Linux geralmente é consistente com a versão do sistema Android que acompanha o **dispositivo quando ele é enviado**. Se o sistema Android for atualizado posteriormente, a versão do kernel geralmente não será alterada. Se você precisar fazer o flash, **por favor, consulte sempre a versão do kernel!**

## Introdução

Existem vários métodos de instalação do KernelSU, cada um adequado para um cenário diferente, portanto escolha conforme necessário.

1. Instalar com fastboot usando o boot.img fornecido por KernelSU
2. Instalar com um app kernel flash, como KernelFlasher
3. Repare o boot.img manualmente e instale-o
4. Instalar com Recovery personalizado (por exemplo, TWRP)

## Instalar com o boot.img fornecido por KernelSU

Se o `boot.img` do seu dispositivo usa um formato de compactação comumente usado, você pode usar as imagens GKI fornecidas pelo KernelSU para atualizá-lo diretamente. Não requer TWRP ou autocorreção da imagem.

### Encontre o boot.img adequado

O KernelSU fornece um boot.img genérico para dispositivos GKI e você deve liberar o boot.img para a partição boot do dispositivo.

Você pode baixar o boot.img em [GitHub Release](https://github.com/tiann/KernelSU/releases), por favor, observe que você deve usar a versão correta do boot.img. Se você não sabe qual arquivo baixar, leia atentamente a descrição de [KMI](#kmi) e [Nível do patch de segurança](#security-patch-level) neste documento.

Normalmente, existem três arquivos de inicialização em formatos diferentes no mesmo KMI e nível do patch de segurança. Eles são todos iguais, exceto pelo formato de compactação do kernel. Por favor, verifique o formato de compactação do kernel de seu boot.img original. Você deve usar o formato correto, como `lz4`, `gz`. Se você usar um formato de compactação incorreto, poderá encontrar bootloop após flashar o boot.img.

::: info INFORMAÇÕES
1. Você pode usar o magiskboot para obter o formato de compactação de seu boot original; é claro que você também pode perguntar a outras pessoas mais experientes com o mesmo modelo do seu dispositivo. Além disso, o formato de compactação do kernel geralmente não muda, portanto, se você inicializar com êxito com um determinado formato de compactação, poderá tentar esse formato mais tarde.
2. Os dispositivos Xiaomi geralmente usam `gz` ou `uncompressed`.
3. Para dispositivos Pixel, siga as instruções abaixo:
:::

### Flash boot.img para o dispositivo

Use o `adb` para conectar seu dispositivo, execute `adb reboot bootloader` para entrar no modo fastboot e use este comando para flashar o KernelSU:

```sh
fastboot flash boot boot.img
```

::: info INFORMAÇÕES
Se o seu dispositivo suportar `fastboot boot`, você pode usar primeiro `fastboot boot boot.img` para tentar usar o boot.img para inicializar o sistema primeiro. Se algo inesperado acontecer, reinicie-o novamente para inicializar.
:::

### Reiniciar

Após a conclusão do flash, você deve reiniciar o dispositivo:

```sh
fastboot reboot
```

## Instalar com Kernel Flasher

Etapa:

1. Baixe o zip AnyKernel3. Se você não sabe qual arquivo baixar, leia atentamente a descrição de [KMI](#kmi) e [Nível do patch de segurança](#security-patch-level) neste documento.
2. Abra o app Kernel Flash (conceda as permissões de root necessárias) e use o zip AnyKernel3 fornecido para fazer o flash.

Dessa forma, é necessário que o app Kernel Flasher tenha permissões root. Você pode usar os seguintes métodos para conseguir isso:

1. Seu dispositivo está rooteado. Por exemplo, você instalou o KernelSU e deseja atualizar para a versão mais recente ou fez o root por meio de outros métodos (como Magisk).
2. Se o seu telefone não estiver rooteado, mas o telefone suportar o método de inicialização temporária de `fastboot boot boot.img`, você pode usar a imagem GKI fornecida pelo KernelSU para inicializar temporariamente o seu dispositivo, obter permissões de root temporária e, em seguida, usar o Kernel Flasher para obter privilégios de root permanente.

1. [Kernel Flasher](https://github.com/capntrips/KernelFlasher/releases)
2. [Franco Kernel Manager](https://play.google.com/store/apps/details?id=com.franco.kernel)
3. [Ex Kernel Manager](https://play.google.com/store/apps/details?id=flar2.exkernelmanager)

PS. Este método é mais conveniente ao atualizar o KernelSU e pode ser feito sem um computador (faça um backup primeiro).

## Corrigir boot.img manualmente

Para alguns dispositivos, o formato boot.img não é tão comum como `lz4`, `gz` e `uncompressed`. O mais típico é o Pixel, seu formato boot.img é `lz4_legacy` compactado, ramdisk pode ser `gz` também pode ser compactado `lz4_legacy`. Neste momento, se você flashar diretamente o boot.img fornecido pelo KernelSU, o telefone pode não conseguir inicializar. Neste momento, você pode corrigir manualmente o boot.img para conseguir isso.

É sempre recomendado usar `magiskboot` para corrigir imagens, existem duas maneiras:

1. [magiskboot](https://github.com/topjohnwu/Magisk/releases)
2. [magiskboot_build](https://github.com/ookiineko/magiskboot_build/releases/tag/last-ci)

A versão oficial do `magiskboot` só pode rodar em dispositivos Android, se você quiser rodar no PC, você pode tentar a segunda opção.

::: tip DICA
Android-Image-Kitchen não é recomendado agora, porque ele não lida corretamente com os metadados de inicialização (como o nível do patch de segurança). Portanto, pode não funcionar em alguns dispositivos.
:::

### Preparação

1. Obtenha o boot.img padrão do telefone. Você pode obtê-lo com os fabricantes do seu dispositivo Talvez você precise do [payload-dumper-go](https://github.com/ssut/payload-dumper-go).
2. Baixe o arquivo zip AnyKernel3 fornecido pelo KernelSU que corresponde à versão KMI do seu dispositivo. Você pode consultar [Instalar com Recovery personalizado](#install-with-custom-recovery).
3. Descompacte o pacote AnyKernel3 e obtenha o arquivo `Image`, que é o arquivo do kernel do KernelSU.

### Usando o magiskboot em dispositivos Android {#using-magiskboot-on-Android-devices}

1. Baixe o Magisk mais recente em [GitHub Releases](https://github.com/topjohnwu/Magisk/releases).
2. Renomeie o `Magisk-*(versão).apk` para `Magisk-*.zip` e descompacte-o.
3. Envie `Magisk-*/lib/arm64-v8a/libmagiskboot.so` para o seu dispositivo por ADB: `adb push Magisk-*/lib/arm64-v8a/libmagiskboot.so /data/local/tmp/magiskboot`.
4. Envie o boot.img padrão e Image em AnyKernel3 para o seu dispositivo.
5. Entre no ADB shell e no diretório cd `/data/local/tmp/`, em seguida, `chmod +x magiskboot`.
6. Entre no ADB shell e no diretório cd `/data/local/tmp/`, execute `./magiskboot unpack boot.img` para descompactar `boot.img`, você obterá um arquivo `kernel`, este é o seu kernel padrão.
7. Substitua `kernel` por `Image`: `mv -f Image kernel`.
8. Execute `./magiskboot repack boot.img` para reembalar o boot.img, e você obterá um arquivo `new-boot.img`, faça o flash deste arquivo para o dispositivo por fastboot.

### Usando o magiskboot no PC Windows/macOS/Linux {#using-magiskboot-on-PC}

1. Baixe o `magiskboot` adequado para o seu sistema operacional em [magiskboot_build](https://github.com/ookiineko/magiskboot_build/releases/tag/last-ci).
2. Prepare o boot.img padrão e Image em seu PC.
3. `chmod +x magiskboot`
4. Entre no diretório apropriado, execute `./magiskboot unpack boot.img` para descompactar `boot.img`. Você obterá um arquivo `kernel`, este é o seu kernel padrão.
5. Substitua `kernel` por `Image`: `mv -f Image kernel`.
6. Execute `./magiskboot repack boot.img` para reembalar o boot.img, e você obterá um arquivo `new-boot.img`, faça o flash deste arquivo para o dispositivo por fastboot.

::: info INFORMAÇÕES
O `magiskboot` oficial pode executar o dispositivo `Linux` normalmente. Se você for um usuário Linux, você pode usar a versão oficial.
:::

## Instalar com Recovery personalizado {#install-with-custom-recovery}

Pré-requisito: Seu dispositivo deve ter um Recovery personalizado, como TWRP. Se apenas o Recovery oficial estiver disponível, use outro método.

Etapa:

1. Na [página de lançamento](https://github.com/tiann/KernelSU/releases) do KernelSU, baixe o pacote zip começando com AnyKernel3 que corresponde à versão do seu telefone; por exemplo, a versão do kernel do telefone é `android12-5.10. 66`, então você deve baixar o arquivo `AnyKernel3-android12-5.10.66_yyyy-MM.zip` (onde `yyyy` é o ano e `MM` é o mês).
2. Reinicie o telefone no TWRP.
3. Use o adb para colocar AnyKernel3-*.zip no telefone /sdcard e escolha instalá-lo na interface do TWRP; ou você pode diretamente `adb sideload AnyKernel-*.zip` para instalar.

PS. Este método é adequado para qualquer instalação (não limitado à instalação inicial ou atualizações subsequentes), desde que você use TWRP.

## Outros métodos

Na verdade, todos esses métodos de instalação têm apenas uma ideia principal, que é **substituir o kernel original pelo fornecido pelo KernelSU**, desde que isso possa ser alcançado, ele pode ser instalado. Por exemplo, a seguir estão outros métodos possíveis.

1. Primeiro instale o Magisk, obtenha privilégios de root através do Magisk e então use o kernel flasher para fazer o flash no zip AnyKernel do KernelSU.
2. Use algum kit de ferramentas de flash em PCs para flashar no kernel fornecido pelo KernelSU.

Mas se não funcionar, por favor, tente o método `magiskboot`.
