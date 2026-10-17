# Scripts de execução única

Um script de execução única (one-shot) é um ZIP que o gerenciador do KernelSU **executa uma vez** em vez de instalar. Ele é empacotado como um módulo, mas nada permanece no dispositivo: o gerenciador extrai o ZIP para um diretório privado, solicita os parâmetros que ele declara, mostra o código-fonte do `action.sh` e o executa em um shell root. Use-o para tarefas como desbloquear uma configuração oculta ou exportar um log; qualquer coisa que precise ser executada na inicialização ou modificar as partições do sistema deve ser um [módulo](module.md).

## Estrutura do pacote

```txt
<id>-<version>.zip
├── module.prop      # the usual module.prop fields + oneshot=true
├── action.sh        # required: the script the manager runs
├── params.json      # optional: parameters asked before running
├── customize.sh     # optional: only used if the ZIP is installed as a module
└── other files      # data the script reads via $MODPATH
```

O `module.prop` usa os mesmos campos de um [módulo](module.md#module-prop) e adiciona `oneshot`:

```txt
id=example_script
name=Example Script
version=v1
versionCode=1
author=Alice
description=Does a thing once
oneshot=true
```

O `oneshot` é lido da mesma forma que o `metamodule`: o valor, ignorando espaços em branco ao redor, é `1` ou `true` (sem diferenciar maiúsculas de minúsculas). Qualquer outro valor, ou a ausência da linha `oneshot`, indica um módulo comum.

Não inclua arquivos exclusivos de módulos, como `post-fs-data.sh`, `service.sh`, `system.prop`, `sepolicy.rule` ou um diretório `system/`: eles nunca são usados em uma execução única. Um script de execução única não pode ser um metamodule.

## Executando um script

Há duas formas de executar um script de execução única:

- **Pela loja de módulos**: entradas do registro [KernelSU Modules](https://modules.kernelsu.org) com `kind: script` exibem um selo "Script" e um botão **Run** em vez de **Instalar**. O download é verificado em relação ao tamanho e ao SHA-256 do registro, e o `module.prop` deve conter o `id` registrado.
- **Por um ZIP local**: se você selecionar um único ZIP do armazenamento (ou abrir um a partir de outro app) e o `module.prop` dele tiver `oneshot=true`, a página de execução será aberta em vez do instalador. A página também oferece **Install as module anyway**.

Antes de executar, o gerenciador verifica se o `module.prop` declara `oneshot` com um `id` válido e se o `action.sh` existe na raiz do ZIP, e então extrai o ZIP. Caminhos absolutos, `..`, links simbólicos e arquivos especiais são rejeitados, e o conteúdo extraído é limitado a 64 MiB e 4096 entradas.

A página de execução mostra o formulário de parâmetros, o código-fonte do `action.sh` e a última execução neste dispositivo. Após uma confirmação, o gerenciador executa o script em um novo shell root no namespace de montagem global, como o botão Ação de um módulo:

```sh
cd <dir> && <env> /data/adb/ksu/bin/busybox sh ./action.sh </dev/null
```

A saída é exibida como a saída da Ação e pode ser salva. O diretório extraído é removido em seguida. O gerenciador lembra apenas a última execução (versão e código de saída); não há estado de instalação nem verificação de atualizações.

### Variáveis de ambiente

| Variável | Valor |
| --- | --- |
| `ASH_STANDALONE` | `1` |
| `KSU` | `true` |
| `KSU_VER`, `KSU_VER_CODE`, `KSU_KERNEL_VER_CODE` | Versões do KernelSU, como para módulos |
| `KSU_LATE_LOAD` | `1` apenas no [modo late-load](module.md#late-load-mode) |
| `KSU_MODULE` | o ID do script |
| `KSU_ONESHOT` | `1`, para distinguir uma execução única do botão Ação de um módulo instalado |
| `MODPATH` | o diretório de extração; leia seus arquivos de dados a partir daqui |
| `PATH` | `$PATH:/data/adb/ksu/bin` |
| `KSU_PARAM_<KEY>` | uma por parâmetro, consulte [`params.json`](#params-json) |

O stdin é `/dev/null`, então o script não pode ler entradas de forma interativa; declare parâmetros em vez disso.

### Códigos de saída

| Código de saída | Significado |
| --- | --- |
| `0` | Sucesso. |
| `2` | Não aplicável a este dispositivo; exibido como um aviso, não como uma falha. |
| qualquer outro | Falha. |

## `params.json` {#params-json}

O `params.json` é opcional. Ele fica na raiz do ZIP, é um JSON em UTF-8 e tem no máximo 64 KiB. O gerenciador cria um formulário a partir dele e passa cada valor ao `action.sh` como `KSU_PARAM_<KEY>`.

```json
{
  "schemaVersion": 1,
  "params": [
    {
      "key": "ENABLE_FASTBOOT",
      "type": "bool",
      "label": "Also enable fastboot",
      "description": "Optional longer help text",
      "default": true
    },
    { "key": "MODE", "type": "enum", "label": "Mode", "default": "a",
      "options": [ { "value": "a", "label": "A" }, { "value": "b" } ] },
    { "key": "NAME", "type": "string", "label": "Name", "default": "", "maxLength": 64,
      "required": false },
    { "key": "COUNT", "type": "int", "label": "Count", "default": 1, "min": 0, "max": 10 }
  ]
}
```

| Campo | Regra |
| --- | --- |
| `schemaVersion` | Deve ser `1`. |
| `params` | Até 16 parâmetros. |
| `key` | `^[A-Z][A-Z0-9_]{0,31}$`, único. O valor chega ao script como `KSU_PARAM_<key>`. |
| `type` | `bool`, `enum`, `string` ou `int`. |
| `label` | Obrigatório, de 1 a 200 caracteres. Os rótulos são exibidos como foram escritos (em um único idioma). |
| `description` | Opcional, mesmas regras de `label`. |

Por tipo:

| Tipo | Campos | Passado como |
| --- | --- | --- |
| `bool` | `default`: `true` / `false` (padrão `false`). | `true` / `false` |
| `enum` | `options`: de 1 a 32 entradas `{value, label?}`; `value` corresponde a `^[A-Za-z0-9._-]{1,64}$` e é único. `default` deve ser um dos valores (padrão: a primeira opção). | o valor da opção |
| `string` | `default` (padrão `""`), `maxLength` de 1 a 1024 (padrão 256), `required` (padrão `false`; um valor vazio é permitido, a menos que seja obrigatório). Os valores não podem conter NUL nem quebras de linha. | como digitado |
| `int` | `min` / `max` e `default` opcionais (padrão `min`, ou `0` sem `min`); inteiros com sinal de 32 bits. | decimal |

O `default` de cada parâmetro deve satisfazer suas próprias restrições. Se o `params.json` for inválido, o gerenciador se recusa a executar o script.

::: warning
Não há verificação de formato para valores `string`. Cada valor é passado entre aspas simples, então nunca é expandido pelo shell que inicia o `action.sh`, mas o `action.sh` ainda deve validá-lo e colocá-lo entre aspas ao usá-lo.
:::

## Exemplo

```sh
#!/system/bin/sh
# action.sh
MODPATH=${MODPATH:-${0%/*}}

if [ "$(getprop ro.miui.ui.version.name)" = "" ]; then
  echo "Not a MIUI device"
  exit 2
fi

echo "Mode: ${KSU_PARAM_MODE:-a}"
if [ "$KSU_PARAM_ENABLE_FASTBOOT" = "true" ]; then
  sh "$MODPATH/fastboot.sh" || exit 1
fi
echo "Done"
```

## Compatibilidade com gerenciadores antigos

Gerenciadores sem suporte a execução única instalam o ZIP como um módulo comum cujo botão Ação executa o `action.sh`. Mantenha o `action.sh` funcionando também nesse caso: nele, `KSU_ONESHOT`, `MODPATH` e `KSU_PARAM_*` não são definidos e o diretório de trabalho é o diretório do módulo. Use valores padrão como alternativa, como no exemplo acima:

```sh
MODPATH=${MODPATH:-${0%/*}}
MODE=${KSU_PARAM_MODE:-a}
```

## Publicação

Para publicar um script de execução única no [KernelSU Modules](https://modules.kernelsu.org), defina `kind: script` no `module.yaml`. Consulte [Script entries](https://github.com/KernelSU-Modules/tools/blob/main/docs/developers/module-yaml.md#script-entries-kind-script) para as regras do registro.
