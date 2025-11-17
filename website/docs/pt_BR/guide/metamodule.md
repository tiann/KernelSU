# Metamódulo

Metamódulos são um recurso revolucionário no KernelSU que transfere recursos críticos do sistema de módulos do daemon principal para módulos plugáveis. Essa mudança arquitetônica mantém a estabilidade e segurança do KernelSU enquanto libera um maior potencial de inovação para o ecossistema de módulos.

## O que é um Metamódulo?

Um metamódulo é um tipo especial de módulo KernelSU que fornece funcionalidade de infraestrutura central para o sistema de módulos. Ao contrário dos módulos regulares que modificam arquivos do sistema, os metamódulos controlam *como* os módulos regulares são instalados e montados.

Metamódulos são um mecanismo de extensão baseado em plugins que permite a personalização completa da infraestrutura de gerenciamento de módulos do KernelSU. Ao delegar a lógica de montagem e instalação aos metamódulos, o KernelSU evita se tornar um ponto de detecção frágil enquanto permite diversas estratégias de implementação.

**Características principais:**

- **Papel de infraestrutura**: Metamódulos fornecem serviços dos quais os módulos regulares dependem
- **Instância única**: Apenas um metamódulo pode ser instalado por vez
- **Execução prioritária**: Scripts de metamódulos são executados antes dos scripts de módulos regulares
- **Hooks especiais**: Fornece três scripts de hook para instalação, montagem e limpeza

## Por que Metamódulos?

Soluções root tradicionais incorporam a lógica de montagem em seu núcleo, tornando-as mais fáceis de detectar e mais difíceis de evoluir. A arquitetura de metamódulos do KernelSU resolve esses problemas através da separação de preocupações.

**Vantagens estratégicas:**

- **Superfície de detecção reduzida**: O próprio KernelSU não realiza montagens, reduzindo vetores de detecção
- **Estabilidade**: O daemon central permanece estável enquanto as implementações de montagem podem evoluir
- **Inovação**: A comunidade pode desenvolver estratégias alternativas de montagem sem bifurcar o KernelSU
- **Escolha**: Os usuários podem selecionar a implementação que melhor se adapta às suas necessidades

**Flexibilidade de montagem:**

- **Sem montagem**: Para usuários com módulos somente sem montagem, evite completamente a sobrecarga de montagem
- **Montagem OverlayFS**: Abordagem tradicional com suporte a camada de leitura-escrita (via `meta-overlayfs`)
- **Magic mount**: Montagem compatível com Magisk para melhor compatibilidade de aplicativos
- **Implementações personalizadas**: Sobreposições baseadas em FUSE, montagens VFS personalizadas ou abordagens totalmente novas

**Além da montagem:**

- **Extensibilidade**: Adicione recursos como suporte a módulos do kernel sem modificar o núcleo do KernelSU
- **Modularidade**: Atualize implementações independentemente das versões do KernelSU
- **Personalização**: Crie soluções especializadas para dispositivos ou casos de uso específicos

::: warning IMPORTANTE
Sem um metamódulo instalado, os módulos **NÃO** serão montados. Instalações novas do KernelSU requerem a instalação de um metamódulo (como `meta-overlayfs`) para que os módulos funcionem.
:::

## Para Usuários

### Instalando um Metamódulo

Instale um metamódulo da mesma forma que módulos regulares:

1. Baixe o arquivo ZIP do metamódulo (por exemplo, `meta-overlayfs.zip`)
2. Abra o aplicativo KernelSU Manager
3. Toque no botão de ação flutuante (➕)
4. Selecione o arquivo ZIP do metamódulo
5. Reinicie seu dispositivo

O metamódulo `meta-overlayfs` é a implementação de referência oficial que fornece montagem de módulos baseada em overlayfs tradicional com suporte a imagem ext4.

### Verificando o Metamódulo Ativo

Você pode verificar qual metamódulo está atualmente ativo na página de Módulos do aplicativo KernelSU Manager. O metamódulo ativo será exibido na sua lista de módulos com sua designação especial.

### Desinstalando um Metamódulo

::: danger AVISO
Desinstalar um metamódulo afetará **TODOS** os módulos. Após a remoção, os módulos não serão mais montados até que você instale outro metamódulo.
:::

Para desinstalar:

1. Abra o KernelSU Manager
2. Encontre o metamódulo na sua lista de módulos
3. Toque em desinstalar (você verá um aviso especial)
4. Confirme a ação
5. Reinicie seu dispositivo

Após desinstalar, você deve instalar outro metamódulo se quiser que os módulos continuem funcionando.

### Restrição de Metamódulo Único

Apenas um metamódulo pode ser instalado por vez. Se você tentar instalar um segundo metamódulo, o KernelSU impedirá a instalação para evitar conflitos.

Para trocar metamódulos:

1. Desinstale todos os módulos regulares
2. Desinstale o metamódulo atual
3. Reinicie
4. Instale o novo metamódulo
5. Reinstale seus módulos regulares
6. Reinicie novamente

## Para Desenvolvedores de Módulos

Se você está desenvolvendo módulos KernelSU regulares, não precisa se preocupar muito com metamódulos. Seus módulos funcionarão desde que os usuários tenham um metamódulo compatível (como `meta-overlayfs`) instalado.

**O que você precisa saber:**

- **Montagem requer um metamódulo**: O diretório `system` no seu módulo só será montado se o usuário tiver um metamódulo instalado que forneça funcionalidade de montagem
- **Nenhuma alteração de código necessária**: Módulos existentes continuam a funcionar sem modificação

::: tip
Se você está familiarizado com o desenvolvimento de módulos Magisk, seus módulos funcionarão da mesma forma no KernelSU quando o metamódulo estiver instalado, pois ele fornece montagem compatível com Magisk.
:::

## Para Desenvolvedores de Metamódulos

Criar um metamódulo permite que você personalize como o KernelSU lida com instalação de módulos, montagem e desinstalação.

### Requisitos Básicos

Um metamódulo é identificado por uma propriedade especial em seu `module.prop`:

```txt
id=my_metamodule
name=My Custom Metamodule
version=1.0
versionCode=1
author=Your Name
description=Custom module mounting implementation
metamodule=1
```

A propriedade `metamodule=1` (ou `metamodule=true`) marca isso como um metamódulo. Sem essa propriedade, o módulo será tratado como um módulo regular.

### Estrutura de Arquivos

Estrutura de um metamódulo:

```txt
my_metamodule/
├── module.prop              (deve incluir metamodule=1)
│
│      *** Hooks específicos de metamódulo ***
├── metamount.sh             (opcional: manipulador de montagem personalizado)
├── metainstall.sh           (opcional: hook de instalação para módulos regulares)
├── metauninstall.sh         (opcional: hook de limpeza para módulos regulares)
│
│      *** Arquivos de módulo padrão (todos opcionais) ***
├── customize.sh             (personalização de instalação)
├── post-fs-data.sh          (script de estágio post-fs-data)
├── service.sh               (script late_start service)
├── boot-completed.sh        (script de inicialização concluída)
├── uninstall.sh             (script de desinstalação do próprio metamódulo)
├── system/                  (modificações systemless, se necessário)
└── [quaisquer arquivos adicionais]
```

Metamódulos podem usar todos os recursos de módulos padrão (scripts de ciclo de vida, etc.) além de seus hooks especiais de metamódulo.

### Scripts de Hook

Metamódulos podem fornecer até três scripts de hook especiais:

#### 1. metamount.sh - Manipulador de Montagem

**Propósito**: Controla como os módulos são montados durante a inicialização.

**Quando executado**: Durante o estágio `post-fs-data`, antes de qualquer script de módulo ser executado.

**Variáveis de ambiente:**

- `MODDIR`: O caminho do diretório do metamódulo (por exemplo, `/data/adb/modules/my_metamodule`)
- Todas as variáveis de ambiente padrão do KernelSU

**Responsabilidades:**

- Montar todos os módulos habilitados de forma systemless
- Verificar flags `skip_mount`
- Lidar com requisitos específicos de montagem de módulos

::: danger REQUISITO CRÍTICO
Ao realizar operações de montagem, você **DEVE** definir o nome da origem/dispositivo como `"KSU"`. Isso identifica as montagens como pertencentes ao KernelSU.

**Exemplo (correto):**

```sh
mount -t overlay -o lowerdir=/lower,upperdir=/upper,workdir=/work KSU /target
```

**Para APIs de montagem modernas**, defina a string de origem:

```rust
fsconfig_set_string(fs, "source", "KSU")?;
```

Isso é essencial para o KernelSU identificar e gerenciar adequadamente suas montagens.
:::

**Script de exemplo:**

```sh
#!/system/bin/sh
MODDIR="${0%/*}"

# Exemplo: Implementação simples de bind mount
for module in /data/adb/modules/*; do
    if [ -f "$module/disable" ] || [ -f "$module/skip_mount" ]; then
        continue
    fi

    if [ -d "$module/system" ]; then
        # Monte com source=KSU (OBRIGATÓRIO!)
        mount -o bind,dev=KSU "$module/system" /system
    fi
done
```

#### 2. metainstall.sh - Hook de Instalação

**Propósito**: Personalizar como módulos regulares são instalados.

**Quando executado**: Durante a instalação do módulo, após a extração dos arquivos, mas antes da conclusão da instalação. Este script é **executado por source** (não executado) pelo instalador embutido, semelhante a como `customize.sh` funciona.

**Variáveis de ambiente e funções:**

Este script herda todas as variáveis e funções do `install.sh` embutido:

- **Variáveis**: `MODPATH`, `TMPDIR`, `ZIPFILE`, `ARCH`, `API`, `IS64BIT`, `KSU`, `KSU_VER`, `KSU_VER_CODE`, `BOOTMODE`, etc.
- **Funções**:
  - `ui_print <msg>` - Imprime mensagem no console
  - `abort <msg>` - Imprime erro e encerra a instalação
  - `set_perm <target> <owner> <group> <permission> [context]` - Define permissões de arquivo
  - `set_perm_recursive <directory> <owner> <group> <dirpermission> <filepermission> [context]` - Define permissões recursivamente
  - `install_module` - Chama o processo de instalação de módulo embutido

**Casos de uso:**

- Processar arquivos de módulo antes ou depois da instalação embutida (chame `install_module` quando estiver pronto)
- Mover arquivos de módulo
- Validar compatibilidade de módulo
- Configurar estruturas de diretório especiais
- Inicializar recursos específicos do módulo

**Nota**: Este script **NÃO** é chamado ao instalar o próprio metamódulo.

#### 3. metauninstall.sh - Hook de Limpeza

**Propósito**: Limpar recursos quando módulos regulares são desinstalados.

**Quando executado**: Durante a desinstalação do módulo, antes do diretório do módulo ser removido.

**Variáveis de ambiente:**

- `MODULE_ID`: O ID do módulo sendo desinstalado

**Casos de uso:**

- Processar arquivos
- Limpar symlinks
- Liberar recursos alocados
- Atualizar rastreamento interno

**Script de exemplo:**

```sh
#!/system/bin/sh
# Chamado ao desinstalar módulos regulares
MODULE_ID="$1"
IMG_MNT="/data/adb/metamodule/mnt"

# Remover arquivos do módulo da imagem
if [ -d "$IMG_MNT/$MODULE_ID" ]; then
    rm -rf "$IMG_MNT/$MODULE_ID"
fi
```

### Ordem de Execução

Entender a ordem de execução da inicialização é crucial para o desenvolvimento de metamódulos:

```txt
estágio post-fs-data:
  1. Scripts comuns post-fs-data.d são executados
  2. Limpar módulos, restorecon, carregar sepolicy.rule
  3. post-fs-data.sh do metamódulo é executado (se existir)
  4. post-fs-data.sh dos módulos regulares são executados
  5. Carregar system.prop
  6. metamount.sh do metamódulo é executado
     └─> Monta todos os módulos de forma systemless
  7. Estágio post-mount.d é executado
     - Scripts comuns post-mount.d
     - post-mount.sh do metamódulo (se existir)
     - post-mount.sh dos módulos regulares

estágio service:
  1. Scripts comuns service.d são executados
  2. service.sh do metamódulo é executado (se existir)
  3. service.sh dos módulos regulares são executados

estágio boot-completed:
  1. Scripts comuns boot-completed.d são executados
  2. boot-completed.sh do metamódulo é executado (se existir)
  3. boot-completed.sh dos módulos regulares são executados
```

**Pontos-chave:**

- `metamount.sh` é executado **APÓS** todos os scripts post-fs-data (tanto metamódulo quanto módulos regulares)
- Scripts de ciclo de vida do metamódulo (`post-fs-data.sh`, `service.sh`, `boot-completed.sh`) sempre são executados antes dos scripts de módulos regulares
- Scripts comuns em diretórios `.d` são executados antes dos scripts de metamódulo
- O estágio `post-mount` é executado após a conclusão da montagem

### Mecanismo de Symlink

Quando um metamódulo é instalado, o KernelSU cria um symlink:

```sh
/data/adb/metamodule -> /data/adb/modules/<metamodule_id>
```

Isso fornece um caminho estável para acessar o metamódulo ativo, independentemente de seu ID.

**Benefícios:**

- Caminho de acesso consistente
- Detecção fácil do metamódulo ativo
- Simplifica a configuração

### Exemplo do Mundo Real: meta-overlayfs

O metamódulo `meta-overlayfs` é a implementação de referência oficial. Ele demonstra as melhores práticas para desenvolvimento de metamódulos.

#### Arquitetura

`meta-overlayfs` usa uma **arquitetura de diretório duplo**:

1. **Diretório de metadados**: `/data/adb/modules/`
   - Contém `module.prop`, `disable`, marcadores `skip_mount`
   - Rápido para escanear durante a inicialização
   - Pegada de armazenamento pequena

2. **Diretório de conteúdo**: `/data/adb/metamodule/mnt/`
   - Contém arquivos reais do módulo (system, vendor, product, etc.)
   - Armazenado em uma imagem ext4 (`modules.img`)
   - Otimizado de espaço com recursos ext4

#### Implementação do metamount.sh

Veja como `meta-overlayfs` implementa o manipulador de montagem:

```sh
#!/system/bin/sh
MODDIR="${0%/*}"
IMG_FILE="$MODDIR/modules.img"
MNT_DIR="$MODDIR/mnt"

# Montar imagem ext4 se ainda não estiver montada
if ! mountpoint -q "$MNT_DIR"; then
    mkdir -p "$MNT_DIR"
    mount -t ext4 -o loop,rw,noatime "$IMG_FILE" "$MNT_DIR"
fi

# Definir variáveis de ambiente para suporte de diretório duplo
export MODULE_METADATA_DIR="/data/adb/modules"
export MODULE_CONTENT_DIR="$MNT_DIR"

# Executar binário de montagem
# (A lógica de montagem real está em um binário Rust)
"$MODDIR/meta-overlayfs"
```

#### Recursos Principais

**Montagem Overlayfs:**

- Usa overlayfs do kernel para modificações systemless verdadeiras
- Suporta múltiplas partições (system, vendor, product, system_ext, odm, oem)
- Suporte a camada de leitura-escrita via `/data/adb/modules/.rw/`

**Identificação de origem:**

```rust
// De meta-overlayfs/src/mount.rs
fsconfig_set_string(fs, "source", "KSU")?;  // OBRIGATÓRIO!
```

Isso define `dev=KSU` para todas as montagens overlay, permitindo identificação adequada.

### Melhores Práticas

Ao desenvolver metamódulos:

1. **Sempre defina a origem como "KSU"** para operações de montagem - umount do kernel e umount do zygisksu precisam disso para desmontar corretamente
2. **Trate erros graciosamente** - processos de inicialização são sensíveis ao tempo
3. **Respeite flags padrão** - suporte `skip_mount` e `disable`
4. **Registre operações** - use `echo` ou logging para depuração
5. **Teste minuciosamente** - erros de montagem podem causar boot loops
6. **Documente o comportamento** - explique claramente o que seu metamódulo faz
7. **Forneça caminhos de migração** - ajude os usuários a mudar de outras soluções

### Testando Seu Metamódulo

Antes de lançar:

1. **Teste a instalação** em uma configuração limpa do KernelSU
2. **Verifique a montagem** com vários tipos de módulos
3. **Verifique a compatibilidade** com módulos comuns
4. **Teste a desinstalação** e limpeza
5. **Valide o desempenho de inicialização** (metamount.sh está bloqueando!)
6. **Garanta o tratamento adequado de erros** para evitar boot loops

## Perguntas Frequentes

### Eu preciso de um metamódulo?

**Para usuários**: Apenas se você quiser usar módulos que requerem montagem. Se você usa apenas módulos que executam scripts sem modificar arquivos do sistema, não precisa de um metamódulo.

**Para desenvolvedores de módulos**: Não, você desenvolve módulos normalmente. Os usuários precisam de um metamódulo apenas se seu módulo requer montagem.

**Para usuários avançados**: Apenas se você quiser personalizar o comportamento de montagem ou criar implementações alternativas de montagem.

### Posso ter vários metamódulos?

Não. Apenas um metamódulo pode ser instalado por vez. Isso evita conflitos e garante comportamento previsível.

### O que acontece se eu desinstalar meu único metamódulo?

Os módulos não serão mais montados. Seu dispositivo inicializará normalmente, mas as modificações dos módulos não serão aplicadas até que você instale outro metamódulo.

### O meta-overlayfs é obrigatório?

Não. Ele fornece montagem overlayfs padrão compatível com a maioria dos módulos. Você pode criar seu próprio metamódulo se precisar de comportamento diferente.

## Veja Também

- [Guia de Módulos](module.md) - Desenvolvimento geral de módulos
- [Diferença com Magisk](difference-with-magisk.md) - Comparando KernelSU e Magisk
- [Como Compilar](how-to-build.md) - Compilando KernelSU a partir do código-fonte
