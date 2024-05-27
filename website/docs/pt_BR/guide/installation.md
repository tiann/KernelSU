# Instalação

## Verifique se o seu dispositivo é compatível

Baixe o gerenciador do KernelSU em [GitHub Releases](https://github.com/tiann/KernelSU/releases), e instale-o no seu dispositivo:

- Se o app mostrar `Sem suporte`, significa que **você deve compilar o kernel sozinho**. O KernelSU não fornecerá e nunca fornecerá um boot.img para você instalar.
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
w      .x         .y       -zzz           -k            -alguma coisa
```

`w.x-zzz-k` é a versão KMI. Por exemplo, se a versão do kernel de um dispositivo for `5.10.101-android12-9-g30979850fc20`, então seu KMI será `5.10-android12-9`. Teoricamente, ele pode inicializar normalmente com outros kernels KMI.

::: tip DICA
Observe que o SubLevel na versão do kernel não faz parte do KMI! Isso significa que `5.10.101-android12-9-g30979850fc20` tem o mesmo KMI que `5.10.137-android12-9-g30979850fc20`!
:::

### Nível do patch de segurança {#security-patch-level}

Dispositivos Android mais recentes podem ter mecanismos anti-rollback que não permitem flashar um boot.img com um nível do patch de segurança antigo. Por exemplo, se o kernel do seu dispositivo for `5.10.101-android12-9-g30979850fc20`, o patch de segurança será `2023-11`, mesmo se você atualizar o kernel correspondente ao KMI do kernel, se o nível do patch de segurança for anterior a `2023-11` (como `2023-06`), então isso pode causar bootloop.

Portanto, os kernels com os níveis do patch de segurança mais recentes são preferidos para manter a correspondência com o KMI.

### Versão do kernel vs Versão do Android

Por favor, observe: **A versão do kernel e a versão do Android não são necessariamente iguais!**

Se você descobrir que a versão do seu kernel é `android12-5.10.101`, mas a versão do seu sistema Android é Android 13 ou outra, não se surpreenda, pois o número da versão do sistema Android não é necessariamente igual ao número da versão do kernel Linux. O número da versão do kernel Linux geralmente é correspondente à versão do sistema Android que acompanha o **dispositivo quando ele é enviado**. Se o sistema Android for atualizado posteriormente, a versão do kernel geralmente não será alterada. Então, antes de flashar qualquer coisa, **consulte sempre a versão do kernel!**

## Introdução

Desde a versão [0.9.0](https://github.com/tiann/KernelSU/releases/tag/v0.9.0), KernelSU suporta dois modos de execução em dispositivos GKI:

1. `GKI`: Substitua o kernel original do dispositivo pelo **Generic Kernel Image** (GKI) fornecido pelo KernelSU.
2. `LKM`: Carregue o **Loadable Kernel Module** (LKM) no kernel do dispositivo sem substituir o kernel original.

Esses dois modos são adequados para diferentes cenários e você pode escolher de acordo com suas necessidades.

### Modo GKI {#gki-mode}

No modo GKI, o kernel original do dispositivo será substituído pela imagem genérica do kernel fornecida pelo KernelSU. As vantagens do modo GKI são:

1. Forte universalidade, adequada para a maioria dos dispositivos. Por exemplo, a Samsung ativou dispositivos KNOX e o modo LKM não pode funcionar. Existem também alguns dispositivos modificados de nicho que só podem usar o modo GKI.
2. Pode ser usado sem depender de firmware oficial e não há necessidade de esperar por atualizações oficiais de firmware, desde que o KMI seja consistente, ele pode ser usado.

### Modo LKM {#lkm-mode}

No modo LKM, o kernel original do dispositivo não será substituído, mas o módulo do kernel carregável será carregado no kernel do dispositivo. As vantagens do modo LKM são:

1. Não substituirá o kernel original do dispositivo. Se você tiver os requisitos especiais para o kernel original do dispositivo ou quiser usar o KernelSU enquanto usa um kernel de terceiros, poderá usar o modo LKM.
2. É mais conveniente atualizar o OTA. Ao atualizar o KernelSU, você pode instalá-lo diretamente no gerenciador sem flashar manualmente. Após o sistema OTA, você pode instalá-lo diretamente no segundo slot sem flashar manualmente.
3. Adequado para alguns cenários especiais, por exemplo, o LKM também pode ser carregado com privilégios root temporários. Como não é necessário substituir a partição boot, ele não acionará o AVB e não causará o bloqueio do dispositivo.
4. O LKM pode ser desinstalado temporariamente. Se você deseja desativar temporariamente o acesso root, você pode desinstalar o LKM, este processo não requer o flash de partições, nem mesmo a reinicialização do dispositivo. Se quiser ativar o root novamente, basta reiniciar o dispositivo.

:::tip COEXISTÊNCIA DE DOIS MODOS
Após abrir o gerenciador, você pode ver o modo atual do dispositivo na página inicial. Observe que a prioridade do modo GKI é maior que a do LKM. Por exemplo, se você usar o kernel GKI para substituir o kernel original e usar LKM para corrigir o kernel GKI, o LKM será ignorado e o dispositivo sempre será executado no modo GKI.
:::

### Qual escolher? {#which-one}

Se o seu aparelho for um celular, recomendamos que você priorize o modo LKM. Se o seu dispositivo for um emulador, WSA ou Waydroid, recomendamos que você priorize o modo GKI.

## Instalação do LKM

### Obtenha o firmware oficial

Para usar o modo LKM, você precisa obter o firmware oficial e corrigi-lo com base no firmware oficial. Se você usar um kernel de terceiros, poderá usar o `boot.img` do kernel de terceiros como firmware oficial.

Existem muitas maneiras de obter o firmware oficial. Se o seu dispositivo suportar `fastboot boot`, então recomendamos **o método mais recomendado e o mais simples** que é usar `fastboot boot` para inicializar temporariamente o kernel GKI fornecido pelo KernelSU, depois instalar o gerenciador e, finalmente, instalá-lo diretamente no gerenciador. Este método não exige que você baixe manualmente o firmware oficial, nem extraia manualmente o boot.

Se o seu dispositivo não suportar `fastboot boot`, pode ser necessário baixar manualmente o pacote de firmware oficial e extrair o boot dele.

Ao contrário do modo GKI, o modo LKM modificará o `ramdisk`, portanto, em dispositivos com Android 13, ele precisa corrigir a partição `init_boot` em vez da partição `boot`, enquanto isso, o modo GKI sempre opera a partição `boot`.

### Use o gerenciador

Abra o gerenciador, clique no ícone de instalação no canto superior direito e diversas opções aparecerão:

1. Selecione e corrija um arquivo. Se o seu telefone não tiver privilégios root, você pode escolher esta opção e, em seguida, selecionar seu firmware oficial, o gerenciador irá corrigi-lo automaticamente. Você só precisa flashar este arquivo corrigido para obter privilégios root permanentemente.
2. Instale diretamente. Se o seu telefone já estiver rooteado, você pode escolher esta opção, o gerenciador obterá automaticamente as informações do seu dispositivo e, em seguida, corrigirá o firmware oficial e irá fazer o flash automaticamente. Você pode considerar usar `fastboot boot` e o kernel GKI do KernelSU para obter root temporário e instalar o gerenciador, e então usar esta opção. Esta também é a principal forma de atualizar o KernelSU.
3. Instale em outra partição. Se o seu dispositivo suportar partição A/B, você pode escolher esta opção, o gerenciador irá corrigir automaticamente o firmware oficial e, em seguida, instalá-lo em outra partição. Este método é adequado para dispositivos após o OTA, você pode instalá-lo diretamente em outra partição após o OTA e, em seguida, reiniciar o dispositivo.

### Use a linha de comando

Se não quiser usar o gerenciador, você também pode usar a linha de comando para instalar o LKM. A ferramenta `ksud` fornecida pelo KernelSU pode ajudá-lo a corrigir rapidamente o firmware oficial e depois fazer o flash.

Esta ferramenta oferece suporte ao macOS, Linux e Windows. Você pode baixar a versão correspondente em [GitHub Release](https://github.com/tiann/KernelSU/releases).

Uso: `ksud boot-patch` você pode verificar a ajuda da linha de comando para opções específicas.

```sh
oriole:/ # ksud boot-patch -h
Patch boot ou imagens init_boot para aplicar o KernelSU

Uso: ksud boot-patch [OPTIONS]

Opções:
  -b, --boot <BOOT>              Caminho da imagem boot, se não for especificado, tentará encontrar a imagem boot automaticamente
  -k, --kernel <KERNEL>          Caminho da imagem do kernel para substituir
  -m, --module <MODULE>          O caminho do módulo LKM a ser substituído, se não for especificado, usará o integrado
  -i, --init <INIT>              init a ser substituído
  -u, --ota                      Usará outro slot quando a imagem boot não for especificada
  -f, --flash                    Flash para a partição boot após o patch
  -o, --out <OUT>                Caminho de saída, se não for especificado, usará o diretório atual
      --magiskboot <MAGISKBOOT>  Caminho do magiskboot, se não for especificado, usará um integrado
      --kmi <KMI>                A versão do KMI, se especificada, usará o KMI especificado
  -h, --help                     Imprimir ajuda
```

Algumas opções que precisam ser explicadas:

1. A opção `--magiskboot` pode especificar o caminho do magiskboot. Se não for especificado, o ksud irá procurá-lo nas variáveis ​​de ambiente. Se você não sabe como obter o magiskboot, você pode verificar [aqui](#patch-boot-image).
2. A opção `--kmi` pode especificar a versão do `KMI`. Se o nome do kernel do seu dispositivo não seguir a especificação KMI, você poderá especificá-lo através desta opção.

O uso mais comum é:

```sh
ksud boot-patch -b <boot.img> --kmi android13-5.10
```

## Instalação no modo GKI

Existem vários métodos de instalação para o modo GKI, cada um adequado para um cenário diferente, portanto escolha conforme necessário.

1. Instalar com fastboot usando o boot.img fornecido pelo KernelSU
2. Instalar com um app kernel flash, como o Kernel Flasher
3. Corrigir boot.img manualmente e instala-lo
4. Instalar com Recovery personalizado (por exemplo, TWRP)

## Instalar com o boot.img fornecido pelo KernelSU

Se o `boot.img` do seu dispositivo usa um formato de compactação comumente usado, você pode usar as imagens GKI fornecidas pelo KernelSU para atualizá-lo diretamente. Não requer TWRP ou autocorreção da imagem.

### Encontre o boot.img adequado

O KernelSU fornece um boot.img genérico para dispositivos GKI e você deve flashar o boot.img para a partição boot do dispositivo.

Você pode baixar o boot.img em [GitHub Release](https://github.com/tiann/KernelSU/releases), por favor, observe que você deve usar a versão correta do boot.img. Se você não sabe qual arquivo baixar, leia atentamente a descrição do [KMI](#kmi) e [Nível do patch de segurança](#security-patch-level) neste documento.

Normalmente, existem três arquivos de inicialização em formatos diferentes no mesmo KMI e nível do patch de segurança. Eles são todos iguais, exceto pelo formato de compactação do kernel. Por favor, verifique o formato de compactação do kernel de seu boot.img original. Você deve usar o formato correto, como `lz4` ou `gz`. Se você usar um formato de compactação incorreto, poderá encontrar bootloop após flashar o boot.img.

::: info FORMATO DE COMPACTAÇÃO DO BOOT.IMG
1. Você pode usar o magiskboot para obter o formato de compactação de seu boot original; alternativamente, você também pode solicitá-lo a membros/desenvolvedores da comunidade com o mesmo modelo do seu dispositivo. Além disso, o formato de compactação do kernel geralmente não muda, portanto, se você inicializar com êxito com um determinado formato de compactação, poderá tentar esse formato mais tarde.
2. Os dispositivos Xiaomi geralmente usam `gz` ou `uncompressed`.
3. Para dispositivos Pixel, siga as instruções abaixo:
:::

### Flash o boot.img para o dispositivo

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

1. Baixe o ZIP AnyKernel3. Se você não sabe qual arquivo baixar, leia atentamente a descrição do [KMI](#kmi) e [Nível do patch de segurança](#security-patch-level) neste documento.
2. Abra o app Kernel Flasher (conceda as permissões de root necessárias) e use o ZIP AnyKernel3 fornecido para fazer o flash.

Dessa forma, é necessário que o app Kernel Flasher tenha privilégios root. Você pode usar os seguintes métodos para conseguir isso:

1. Seu dispositivo está rooteado. Por exemplo, você instalou o KernelSU e deseja atualizar para a versão mais recente ou fez o root por meio de outros métodos (como Magisk).
2. Se o seu dispositivo não estiver rooteado, mas suportar o método de inicialização temporária como `fastboot boot boot.img`, você pode usar a imagem GKI fornecida pelo KernelSU para inicializar temporariamente o seu dispositivo, obter privilégios root temporário e, em seguida, usar o Kernel Flasher para obter privilégios root permanente.

1. [Kernel Flasher](https://github.com/capntrips/KernelFlasher/releases)
2. [Franco Kernel Manager](https://play.google.com/store/apps/details?id=com.franco.kernel)
3. [Ex Kernel Manager](https://play.google.com/store/apps/details?id=flar2.exkernelmanager)

Observação: Este método é mais conveniente ao atualizar o KernelSU e pode ser feito sem um computador (faça um backup primeiro).

## Corrigir boot.img manualmente {#patch-boot-image}

Para alguns dispositivos, o formato boot.img não é tão comum como `lz4`, `gz` e `uncompressed`. O mais típico é o Pixel, seu formato boot.img é `lz4_legacy` compactado, ramdisk pode ser `gz` e também pode ser compactado `lz4_legacy`. Atualmente, se você flashar diretamente o boot.img fornecido pelo KernelSU, o telefone pode não conseguir inicializar. Neste momento, você pode corrigir manualmente o boot.img para conseguir isso.

É sempre recomendado usar `magiskboot` para corrigir imagens, existem duas maneiras:

1. [magiskboot](https://github.com/topjohnwu/Magisk/releases)
2. [magiskboot_build](https://github.com/ookiineko/magiskboot_build/releases/tag/last-ci)

A versão oficial do `magiskboot` só pode rodar em dispositivos Android, se você quiser rodar no PC, você pode tentar a segunda opção.

::: tip DICA
Android-Image-Kitchen não é recomendado por enquanto, porque ele não lida corretamente com os metadados de inicialização (como o nível do patch de segurança). Portanto, pode não funcionar em alguns dispositivos.
:::

### Preparação

1. Obtenha o boot.img padrão do telefone. Você pode obtê-lo com os fabricantes do seu dispositivo. Talvez você precise do [payload-dumper-go](https://github.com/ssut/payload-dumper-go).
2. Baixe o arquivo ZIP AnyKernel3 fornecido pelo KernelSU que corresponde à versão KMI do seu dispositivo. Você pode consultar [Instalar com Recovery personalizado](#install-with-custom-recovery).
3. Descompacte o pacote AnyKernel3 e obtenha o arquivo `Image`, que é o arquivo do kernel do KernelSU.

### Usando o magiskboot em dispositivos Android {#using-magiskboot-on-Android-devices}

1. Baixe o Magisk mais recente em [GitHub Releases](https://github.com/topjohnwu/Magisk/releases).
2. Renomeie o `Magisk-*(versão).apk` para `Magisk-*.zip` e descompacte-o.
3. Envie `Magisk-*/lib/arm64-v8a/libmagiskboot.so` para o seu dispositivo por ADB: `adb push Magisk-*/lib/arm64-v8a/libmagiskboot.so /data/local/tmp/magiskboot`.
4. Envie o boot.img padrão e Image em AnyKernel3 para o seu dispositivo.
5. Entre no ADB shell e no diretório cd `/data/local/tmp/`, em seguida, `chmod +x magiskboot`.
6. Entre no ADB shell e no diretório cd `/data/local/tmp/`, execute `./magiskboot unpack boot.img` para descompactar `boot.img`, você obterá um arquivo `kernel`, este é o seu kernel padrão.
7. Substitua `kernel` por `Image` executando o comando: `mv -f Image kernel`.
8. Execute `./magiskboot repack boot.img` para reembalar o boot.img, e você obterá um arquivo `new-boot.img`, faça o flash deste arquivo para o dispositivo por fastboot.

### Usando o magiskboot no PC Windows/macOS/Linux {#using-magiskboot-on-PC}

1. Baixe o `magiskboot` adequado para o seu sistema operacional em [magiskboot_build](https://github.com/ookiineko/magiskboot_build/releases/tag/last-ci).
2. Prepare o `boot.img` padrão e `Image` em seu PC.
3. Execute `chmod +x magiskboot`.
4. Entre no diretório apropriado, execute `./magiskboot unpack boot.img` para descompactar `boot.img`. Você obterá um arquivo `kernel`, este é o seu kernel padrão.
5. Substitua `kernel` por `Image` executando o comando: `mv -f Image kernel`.
6. Execute `./magiskboot repack boot.img` para reembalar o boot.img, e você obterá um arquivo `new-boot.img`, faça o flash deste arquivo para o dispositivo por fastboot.

::: info INFORMAÇÕES
O `magiskboot` oficial pode executar o dispositivo `Linux` normalmente. Se você for um usuário Linux, você pode usar a versão oficial.
:::

## Instalar com Recovery personalizado {#install-with-custom-recovery}

Pré-requisito: Seu dispositivo deve ter um Recovery personalizado, como TWRP. Se não houver Recovery personalizado disponível para o seu dispositivo, use outro método.

Etapas:

1. Em [GitHub Releases](https://github.com/tiann/KernelSU/releases), baixe o pacote ZIP começando com AnyKernel3 que corresponde à versão do seu telefone. Por exemplo, a versão do kernel do dispositivo é `android12-5.10. 66`, então você deve baixar o arquivo `AnyKernel3-android12-5.10.66_yyyy-MM.zip` (onde `yyyy` é o ano e `MM` é o mês).
2. Reinicie o telefone no TWRP.
3. Use o ADB para colocar AnyKernel3-*.zip no dispositivo em `/sdcard` e escolha instalá-lo na interface do TWRP, ou você pode diretamente executar `adb sideload AnyKernel-*.zip` para instalar.

Observação: Este método é adequado para qualquer instalação (não limitado à instalação inicial ou atualizações subsequentes), desde que você use o TWRP.

## Outros métodos

Na verdade, todos esses métodos de instalação têm apenas uma ideia principal, que é **substituir o kernel original pelo fornecido pelo KernelSU**, desde que isso possa ser alcançado, ele pode ser instalado. Por exemplo, a seguir estão outros métodos possíveis.

1. Primeiro instale o Magisk, obtenha privilégios root através do Magisk e então use o Kernel Flasher para fazer o flash no ZIP AnyKernel3 do KernelSU.
2. Use algum kit de ferramentas de flash em PC para flashar no kernel fornecido pelo KernelSU.

No entanto, se não funcionar, por favor, tente o método `magiskboot`.
