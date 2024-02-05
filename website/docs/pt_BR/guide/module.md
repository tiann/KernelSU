# Guias de módulo

O KernelSU fornece um mecanismo de módulo que consegue modificar o diretório do sistema enquanto mantém a integridade da partição do sistema. Este mecanismo é conhecido como "sem sistema".

O mecanismo de módulo do KernelSU é quase o mesmo do Magisk. Se você está familiarizado com o desenvolvimento de módulos Magisk, o desenvolvimento de módulos KernelSU é muito semelhante. Você pode pular a introdução dos módulos abaixo e só precisa ler [Diferença com Magisk](difference-with-magisk.md).

## BusyBox

O KernelSU vem com um recurso binário BusyBox completo (incluindo suporte completo ao SELinux). O executável está localizado em `/data/adb/ksu/bin/busybox`. O BusyBox do KernelSU suporta o "ASH Standalone Shell Mode" alternável em tempo de execução. O que este Modo Autônomo significa é que ao executar no shell `ash` do BusyBox, cada comando usará diretamente o miniaplicativo dentro do BusyBox, independentemente do que estiver definido como `PATH`. Por exemplo, comandos como `ls`, `rm`, `chmod` **NÃO** usarão o que está em `PATH` (no caso do Android por padrão será `/system/bin/ls`, `/system/bin/rm` e `/system/bin/chmod` respectivamente), mas em vez disso chamará diretamente os miniaplicativos internos do BusyBox. Isso garante que os scripts sempre sejam executados em um ambiente previsível e sempre tenham o conjunto completo de comandos, independentemente da versão do Android em que estão sendo executados. Para forçar um comando a **NÃO** usar o BusyBox, você deve chamar o executável com caminhos completos.

Cada script shell executado no contexto do KernelSU será executado no shell `ash` do BusyBox com o Modo Autônomo ativado. Para o que é relevante para desenvolvedores terceirizados, isso inclui todos os scripts de inicialização e scripts de instalação de módulos.

Para aqueles que desejam usar o recurso “Modo Autônomo” fora do KernelSU, existem 2 maneiras de ativá-los:

1. Defina a variável de ambiente `ASH_STANDALONE` como `1`.<br>Exemplo: `ASH_STANDALONE=1 /data/adb/ksu/bin/busybox sh <script>`
2. Alternar com opções de linha de comando:<br>`/data/adb/ksu/bin/busybox sh -o standalone <script>`

Para garantir que todos os shells `sh` subsequentes executados também sejam executados no Modo Autônomo, a opção 1 é o método preferido (e é isso que o KernelSU e o gerenciador do KernelSU usam internamente), pois as variáveis ​​de ambiente são herdadas para os subprocesso.

::: tip DIFERENÇA COM MAGISK

O BusyBox do KernelSU agora está usando o arquivo binário compilado diretamente do projeto Magisk. **Obrigado ao Magisk!** Portanto, você não precisa se preocupar com problemas de compatibilidade entre scripts BusyBox no Magisk e KernelSU porque eles são exatamente iguais!
:::

## Módulos KernelSU

Um módulo KernelSU é uma pasta colocada em `/data/adb/modules` com a estrutura abaixo:

```txt
/data/adb/modules
├── .
├── .
|
├── $MODID                  <--- A pasta é nomeada com o ID do módulo
│   │
│   │      *** Identidade do módulo ***
│   │
│   ├── module.prop         <--- Este arquivo armazena os metadados do módulo
│   │
│   │      *** Conteúdo principal ***
│   │
│   ├── system              <--- Esta pasta será montada se skip_mount não existir
│   │   ├── ...
│   │   ├── ...
│   │   └── ...
│   │
│   │      *** Sinalizadores de status ***
│   │
│   ├── skip_mount          <--- Se existir, o KernelSU NÃO montará sua pasta de sistema
│   ├── disable             <--- Se existir, o módulo será desabilitado
│   ├── remove              <--- Se existir, o módulo será removido na próxima reinicialização
│   │
│   │      *** Arquivos opcionais ***
│   │
│   ├── post-fs-data.sh     <--- Este script será executado em post-fs-data
│   ├── post-mount.sh       <--- Este script será executado em post-mount
│   ├── service.sh          <--- Este script será executado no late_start service
│   ├── boot-completed.sh   <--- Este script será executado na inicialização concluída
|   ├── uninstall.sh        <--- Este script será executado quando o KernelSU remover seu módulo
│   ├── system.prop         <--- As propriedades neste arquivo serão carregadas como propriedades do sistema por resetprop
│   ├── sepolicy.rule       <--- Regras adicionais de sepolicy personalizadas
│   │
│   │      *** Gerado automaticamente, NÃO CRIE OU MODIFIQUE MANUALMENTE ***
│   │
│   ├── vendor              <--- Um link simbólico para $MODID/system/vendor
│   ├── product             <--- Um link simbólico para $MODID/system/product
│   ├── system_ext          <--- Um link simbólico para $MODID/system/system_ext
│   │
│   │      *** Quaisquer arquivos/pastas adicionais são permitidos ***
│   │
│   ├── ...
│   └── ...
|
├── another_module
│   ├── .
│   └── .
├── .
├── .
```

::: tip DIFERENÇA COM MAGISK
O KernelSU não possui suporte integrado para o Zygisk, portanto não há conteúdo relacionado ao Zygisk no módulo. No entanto, você pode usar [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext) para suportar módulos Zygisk. Neste caso, o conteúdo do módulo Zygisk é idêntico ao suportado pelo Magisk.
:::

### module.prop

`module.prop` é um arquivo de configuração para um módulo. No KernelSU, se um módulo não contiver este arquivo, ele não será reconhecido como um módulo. O formato deste arquivo é o seguinte:

```txt
id=<string>
name=<string>
version=<string>
versionCode=<int>
author=<string>
description=<string>
```

- `id` deve corresponder a esta expressão regular: `^[a-zA-Z][a-zA-Z0-9._-]+$`<br>
  Exemplo: ✓ `a_module`, ✓ `a.module`, ✓ `module-101`, ✗ `a module`, ✗ `1_module`, ✗ `-a-module`<br>
  Este é o **identificador exclusivo** do seu módulo. Você não deve alterá-lo depois de publicado.
- `versionCode` deve ser um **número inteiro**. Isso é usado para comparar versões
- Outros que não foram mencionados acima podem ser qualquer string de **linha única**.
- Certifique-se de usar o tipo de quebra de linha `UNIX (LF)` e não o `Windows (CR+LF)` ou `Macintosh (CR)`.

### Shell scripts

Por favor, leia a seção [Scripts de inicialização](#scripts-de-inicializacao) para entender a diferença entre `post-fs-data.sh` e `service.sh`. Para a maioria dos desenvolvedores de módulos, `service.sh` deve ser bom o suficiente se você precisar apenas executar um script de inicialização. Se precisar executar o script após a inicialização ser concluída, use `boot-completed.sh`. Se você quiser fazer algo após montar OverlayFS, use `post-mount.sh`.

Em todos os scripts do seu módulo, use `MODDIR=${0%/*}` para obter o caminho do diretório base do seu módulo, **NÃO** codifique o caminho do seu módulo em scripts.

::: tip DIFERENÇA COM MAGISK
Você pode usar a variável de ambiente `KSU` para determinar se um script está sendo executado no KernelSU ou Magisk. Se estiver executando no KernelSU, esse valor será definido como `true`.
:::

### Diretório `system`

O conteúdo deste diretório será sobreposto à partição /system do sistema usando OverlayFS após a inicialização do sistema. Isso significa que:

1. Arquivos com o mesmo nome daqueles no diretório correspondente no sistema serão substituídos pelos arquivos deste diretório.
2. Pastas com o mesmo nome daquelas no diretório correspondente no sistema serão mescladas com as pastas neste diretório.

Se você deseja excluir um arquivo ou pasta no diretório original do sistema, você precisa criar um arquivo com o mesmo nome do arquivo/pasta no diretório do módulo usando `mknod filename c 0 0`. Dessa forma, o sistema OverlayFS irá automaticamente "branquear" este arquivo como se ele tivesse sido excluído (a partição /system não foi realmente alterada).

Você também pode declarar uma variável chamada `REMOVE` contendo uma lista de diretórios em `customize.sh` para executar operações de remoção, e o KernelSU executará automaticamente `mknod <TARGET> c 0 0` nos diretórios correspondentes do módulo. Por exemplo:

```sh
REMOVE="
/system/app/YouTube
/system/app/Bloatware
"
```

A lista acima irá executar `mknod $MODPATH/system/app/YouTube c 0 0` e `mknod $MODPATH/system/app/Bloatware c 0 0`; e `/system/app/YouTube` e `/system/app/Bloatware` serão removidos após o módulo entrar em vigor.

Se você deseja substituir um diretório no sistema, você precisa criar um diretório com o mesmo caminho no diretório do módulo e, em seguida, definir o atributo `setfattr -n trusted.overlay.opaque -v y <TARGET>` para este diretório. Desta forma, o sistema OverlayFS substituirá automaticamente o diretório correspondente no sistema (sem alterar a partição /system).

Você pode declarar uma variável chamada `REPLACE` em seu arquivo `customize.sh`, que inclui uma lista de diretórios a serem substituídos, e o KernelSU executará automaticamente as operações correspondentes em seu diretório de módulo. Por exemplo:

```sh
REPLACE="
/system/app/YouTube
/system/app/Bloatware
"
```

Esta lista criará automaticamente os diretórios `$MODPATH/system/app/YouTube` e `$MODPATH/system/app/Bloatware` e, em seguida, executará `setfattr -n trusted.overlay.opaque -v y $MODPATH/system/app/YouTube` e `setfattr -n trusted.overlay.opaque -v y $MODPATH/system/app/Bloatware`. Após o módulo entrar em vigor, `/system/app/YouTube` e `/system/app/Bloatware` serão substituídos por diretórios vazios.

::: tip DIFERENÇA COM MAGISK

O mecanismo sem sistema do KernelSU é implementado através do OverlayFS do kernel, enquanto o Magisk atualmente usa montagem mágica (montagem de ligação). Os dois métodos de implementação têm diferenças significativas, mas o objetivo final é o mesmo: modificar os arquivos /system sem modificar fisicamente a partição /system.
:::

Se você estiver interessado em OverlayFS, é recomendável ler a [documentação sobre OverlayFS](https://docs.kernel.org/filesystems/overlayfs.html) do Kernel Linux.

### system.prop

Este arquivo segue o mesmo formato de `build.prop`. Cada linha é composta por `[key]=[value]`.

### sepolicy.rule

Se o seu módulo exigir alguns patches adicionais de sepolicy, adicione essas regras a este arquivo. Cada linha neste arquivo será tratada como uma declaração de política.

## Instalador do módulo

Um instalador do módulo KernelSU é um módulo KernelSU empacotado em um arquivo zip que pode ser atualizado no app gerenciador KernelSU. O instalador do módulo KernelSU mais simples é apenas um módulo KernelSU compactado como um arquivo zip.

```txt
module.zip
│
├── customize.sh                       <--- (Opcional, mais detalhes posteriormente)
│                                           Este script será fornecido por update-binary
├── ...
├── ...  /* O resto dos arquivos do módulo */
│
```

::: warning AVISO
O módulo KernelSU **NÃO** é compatível para instalação no Recovery personalizado!
:::

### Personalização

Se você precisar personalizar o processo de instalação do módulo, opcionalmente você pode criar um script no instalador chamado `customize.sh`. Este script será **sourced** (não executado!) pelo script do instalador do módulo depois que todos os arquivos forem extraídos e as permissões padrão e o contexto secundário forem aplicados. Isso é muito útil se o seu módulo exigir configuração adicional com base na API do dispositivo ou se você precisar definir permissões/secontext especiais para alguns dos arquivos do seu módulo.

Se você quiser controlar e personalizar totalmente o processo de instalação, declare `SKIPUNZIP=1` em `customize.sh` para pular todas as etapas de instalação padrão. Ao fazer isso, seu `customize.sh` será responsável por instalar tudo sozinho.

O script `customize.sh` é executado no shell BusyBox `ash` do KernelSU com o "Modo Autônomo" ativado. As seguintes variáveis ​​e funções estão disponíveis:

#### Variáveis

- `KSU` (bool): uma variável para marcar que o script está sendo executado no ambiente KernelSU, e o valor desta variável sempre será `true`. Você pode usá-lo para distinguir entre KernelSU e Magisk.
- `KSU_VER` (string): a string da versão do KernelSU atualmente instalado (por exemplo, `v0.4.0`).
- `KSU_VER_CODE` (int): o código da versão do KernelSU atualmente instalado no espaço do usuário (por exemplo: `10672`).
- `KSU_KERNEL_VER_CODE` (int): o código da versão do KernelSU atualmente instalado no espaço do kernel (por exemplo: `10672`).
- `BOOTMODE` (bool): sempre seja `true` no KernelSU.
- `MODPATH` (path): o caminho onde os arquivos do seu módulo devem ser instalados.
- `TMPDIR` (path): um lugar onde você pode armazenar arquivos temporariamente.
- `ZIPFILE` (path): zip de instalação do seu módulo.
- `ARCH` (string): a arquitetura da CPU do dispositivo. O valor é `arm`, `arm64`, `x86` ou `x64`.
- `IS64BIT` (bool): `true` se `$ARCH` for `arm64` ou `x64`.
- `API` (int): o nível da API (versão do Android) do dispositivo (por exemplo: `23` para Android 6.0).

::: warning AVISO
No KernelSU, `MAGISK_VER_CODE` é sempre `25200` e `MAGISK_VER` é sempre `v25.2`. Por favor, não use essas duas variáveis ​​para determinar se ele está sendo executado no KernelSU ou não.
:::

#### Funções

```txt
ui_print <msg>
    imprima <msg> no console
    Evite usar 'echo', pois ele não será exibido no console de recuperação personalizado

abort <msg>
    imprima mensagem de erro <msg> para consolar e encerrar a instalação
    Evite usar 'exit', pois isso irá pular as etapas de limpeza de encerramento

set_perm <target> <owner> <group> <permission> [context]
    se [context] não estiver definido, o padrão é "u:object_r:system_file:s0"
    esta função é uma abreviação para os seguintes comandos:
       chown owner.group target
       chmod permission target
       chcon context target

set_perm_recursive <directory> <owner> <group> <dirpermission> <filepermission> [context]
    se [context] não está definido, o padrão é "u:object_r:system_file:s0"
    para todos os arquivos em <directory>, ele chamará:
       contexto de permissão de arquivo do grupo proprietário do arquivo set_perm
    para todos os diretórios em <directory> (including itself), ele vai ligar:
       set_perm dir owner group dirpermission context
```

## Scripts de inicialização

No KernelSU, os scripts são divididos em dois tipos com base em seu modo de execução: modo post-fs-data e modo de serviço late_start:

- modo post-fs-data
  - Esta etapa está BLOQUEANDO. O processo de inicialização é pausado antes da execução ser concluída ou 10 segundos se passaram.
  - Os scripts são executados antes de qualquer módulo ser montado. Isso permite que um desenvolvedor de módulo ajuste dinamicamente seus módulos antes de serem montados.
  - Este estágio acontece antes do início do Zygote, o que significa praticamente tudo no Android.
  - **AVISO:** Usar `setprop` irá bloquear o processo de inicialização! Por favor, use `resetprop -n <prop_name> <prop_value>` em vez disso.
  - **Execute scripts neste modo apenas se necessário.**
- modo de serviço late_start
  - Esta etapa é SEM BLOQUEIO. Seu script é executado em paralelo com o restante do processo de inicialização.
  - **Este é o estágio recomendado para executar a maioria dos scripts**.

No KernelSU, os scripts de inicialização são divididos em dois tipos com base no local de armazenamento: scripts gerais e scripts de módulo:

- Scripts gerais
  - Colocado em `/data/adb/post-fs-data.d`, `/data/adb/service.d`, `/data/adb/post-mount.d` ou `/data/adb/boot-completed.d`.
  - Somente executado se o script estiver definido como executável (`chmod +x script.sh`).
  - Os scripts em `post-fs-data.d` são executados no modo post-fs-data e os scripts em `service.d` são executados no modo de serviço late_start.
  - Os módulos **NÃO** devem adicionar scripts gerais durante a instalação.
- Scripts de módulo
  - Colocado na própria pasta do módulo.
  - Executado apenas se o módulo estiver ativado.
  - `post-fs-data.sh` é executado no modo post-fs-data, `service.sh` é executado no modo de serviço late_start, `boot-completed.sh` é executado na inicialização concluída e `post-mount.sh` é executado em OverlayFS montado.

Todos os scripts de inicialização serão executados no shell BusyBox `ash` do KernelSU com o "Modo Autônomo" ativado.
