# Resgate do bootloop

Ao atualizar um dispositivo, podem ocorrer situações em que o dispositivo fica "bloqueado". Em teoria, se você usar o fastboot apenas para atualizar a partição boot ou instalar módulos inadequados que causam falha na inicialização do dispositivo, isso pode ser restaurado por meio de operações apropriadas. Este documento tem como objetivo fornecer alguns métodos de emergência para ajudá-lo a recuperar um dispositivo "bloqueado".

## Bloqueio por flashar partição boot

No KernelSU, as seguintes situações podem causar bloqueio de inicialização ao flashar a partição boot:

1. Você flashou uma imagem boot no formato errado. Por exemplo, se o formato de boot do seu dispositivo for `gz`, mas você flashou uma imagem no formato `lz4`, o dispositivo não inicializá.
2. Seu dispositivo precisa desativar a verificação AVB para inicializar corretamente, o que geralmente exige a limpeza de todos os dados do dispositivo.
3. Seu kernel tem alguns bugs ou não é adequado para o flash do seu dispositivo.

Não importa qual seja a situação, você pode recuperar **flashando a imagem de boot padrão**. Portanto, no início do guia de instalação, recomendamos fortemente que você faça backup de seu boot padrão antes de realizar o flash. Se você não fez backup, poderá obter o boot original de fábrica de outros usuários com o mesmo dispositivo ou do firmware oficial.

## Bloqueio por módulos

A instalação de módulos pode ser uma das causas mais comuns de bloqueio do seu dispositivo, mas devemos alertá-lo seriamente: **NÃO INSTALE MÓDULOS DE FONTES DESCONHECIDAS!** Como os módulos têm privilégios root, eles podem causar danos irreversíveis ao seu dispositivo!

### Módulos normais

Se você instalou um módulo que foi comprovadamente seguro, mas faz com que seu dispositivo não inicialize, então esta situação é facilmente recuperável no KernelSU sem qualquer preocupação. O KernelSU possui o Modo de Segurança integrado para recuperar seu dispositivo:

#### Recupere pressionando o botão de diminuir volume {#volume-down}

Você pode tentar usar o **Modo de Segurança** para resgatar o dispositivo. Depois de entrar no Modo de Segurança, todos os módulos são desativados.

Existem duas maneiras de entrar no Modo de Segurança:

1. O Modo de Segurança integrado de alguns sistemas: Alguns sistemas possuem um Modo de Segurança integrado que pode ser acessado pressionando longamente o botão de diminuir volume. Em outros sistemas (como o MIUI/HyperOS), o Modo de Segurança pode ser ativado a partir do Recovery. Ao entrar no Modo de Segurança do sistema, o KernelSU também entrará nesse modo e desativará automaticamente os módulos.
2. O Modo de Segurança integrado do KernelSU: Nesse caso, o método é **pressionar a tecla de diminuir volume continuamente por mais de três vezes** após a primeira tela de inicialização. Observe que é pressionar-soltar, pressionar-soltar, pressionar-soltar, e não segurar.

Após entrar no Modo de Segurança, todos os módulos na página Módulos do gerenciador do KernelSU serão desativados. Porém, você ainda pode realizar a operação de "desinstalação" para desinstalar quaisquer módulos que possam estar causando problemas.

O Modo de Segurança integrado é implementado no kernel, portanto não há possibilidade de perder eventos importantes devido à interceptação. No entanto, para kernels não-GKI, pode ser necessária uma integração manual do código. Para isso, consulte a documentação oficial para orientações.

::: warning
O KernelSU registra o ouvinte da tecla de volume durante a inicialização do módulo do kernel (carregado quando o kernel executa o processo init no modo LKM) e cancela o registro no estágio `on_post_fs_data` (antes da animação de inicialização). Você precisa entender o momento e pressionar rapidamente a tecla de diminuir o volume três vezes após a primeira tela de inicialização. Se o dispositivo inicializar rápido ou a operação não for oportuna, o modo de segurança pode não ser ativado.

Se o módulo gravar códigos não razoáveis no initrc que façam com que o dispositivo não consiga inicializar, esses códigos ainda serão executados mesmo no modo de segurança.
:::

#### Resgate Manual {#manual-rescue}

Quando o modo de segurança não conseguir resolver o problema, você pode tentar o resgate manual. Escolha os métodos a seguir de acordo com o status do dispositivo.

**Método 1: Use o ksud para gerenciar módulos via ADB**

Se o dispositivo conseguir obter o shell root via ADB, você pode usar a linha de comando `ksud` diretamente para desativar ou desinstalar o módulo problemático:

::: tip
Após montar as partições `metadata` e `data`, você pode executar o comando `/data/adb/ksud` no modo de Recuperação para gerenciar os módulos.

Como os dispositivos GKI compartilham o `init`, o módulo do kernel do KernelSU ainda será carregado no modo de Recuperação, e você deve conseguir usar a maioria dos recursos do `ksud` (como definir recursos) normalmente.
:::

```
adb shell
su
ksud module list          # Listar todos os módulos
ksud module disable <id>  # Desativar módulo problemático
ksud module uninstall <id> # Ou desinstalar diretamente
reboot
```

**Método 2: Limpeza manual através da Recuperação (Recovery)**

Se você não conseguir entrar no sistema (nem mesmo o ADB puder ser conectado), precisará de uma Recuperação de terceiros (como o TWRP) no dispositivo.

O carregamento do módulo do KernelSU depende do arquivo de injeção init.rc do lado do kernel e do processo ksud no espaço do usuário. Após excluir esses arquivos e reiniciar, o KernelSU não carregará nenhum módulo.

**Passos operacionais:**

1. Entre na Recuperação (como o TWRP).
2. Monte a partição de dados (data):
   ```
   mount /data
   ```
   (Pode ser necessário descriptografar a partição de dados primeiro. A operação específica depende do dispositivo e do método de descriptografia.)
3. Exclua o ksud para evitar o carregamento do módulo:
   ```
   rm -f /data/adb/ksud
   ```
4. (Opcional) Monte a partição de metadados e exclua o arquivo de injeção init.rc gerado pelo módulo:
   ```
   mount /metadata
   rm -f /metadata/ksu/modules.rc
   rm -f /metadata/watchdog/ksu/modules.rc
   ```
5. Reinicie o dispositivo:
   ```
   reboot
   ```

O KernelSU pulará o carregamento de todos os módulos após a reinicialização. Após entrar no sistema, você pode reabrir o gerenciador KernelSU para lidar com os problemas dos módulos.

### Formatação de dados ou outros módulos maliciosos

Se os métodos acima não conseguirem recuperar seu dispositivo, é muito provável que o módulo que você instalou tenha operações maliciosas ou tenha danificado seu dispositivo de outra forma. Nesse caso, há apenas duas sugestões:

1. Limpar os dados e instalar o sistema oficial completamente.
2. Consultar o serviço pós-venda.
