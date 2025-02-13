# Diferenças com Magisk

Embora os módulos do KernelSU e do Magisk tenham muitas semelhanças, existem inevitavelmente algumas diferenças devido aos seus mecanismos de implementação completamente diferentes. Se você deseja que seu módulo funcione tanto no Magisk quanto no KernelSU, é essencial compreender essas diferenças.

## Semelhanças

- Formato de arquivo do módulo: Ambos usam o formato ZIP para organizar os módulos, e o formato dos módulos é praticamente o mesmo.
- Diretório de instalação do módulo: Ambos estão localizados em `/data/adb/modules`.
- Sem sistema: Ambos suportam a modificação de `/system` de forma sem sistema por meio de módulos.
- post-fs-data.sh: O tempo de execução e a semântica são exatamente os mesmos.
- service.sh: O tempo de execução e a semântica são exatamente os mesmos.
- system.prop: Completamente o mesmo.
- sepolicy.rule: Completamente o mesmo.
- BusyBox: Os scripts são executados no BusyBox com o "Modo Autônomo" ativado em ambos os casos.

## Diferenças

Antes de entender as diferenças, é importante saber como identificar se o seu módulo está sendo executado no KernelSU ou no Magisk. Você pode usar a variável de ambiente `KSU` para diferenciá-lo em todos os locais onde você pode executar os scripts do módulo (`customize.sh`, `post-fs-data.sh`, `service.sh`). No KernelSU, essa variável de ambiente será definida como `true`.

Aqui estão algumas diferenças:

- Os módulos KernelSU não podem ser instalados no modo Recovery.
- Os módulos KernelSU não oferece suporte nativo ao Zygisk, mas você pode usar módulos Zygisk através do [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext).
- O método para substituir ou excluir arquivos nos módulos do KernelSU é completamente diferente do Magisk. O KernelSU não suporta o método `.replace`. Em vez disso, você deve criar um arquivo com o comando `mknod filename c 0 0` para excluir o arquivo correspondente.
- Os diretórios do BusyBox são diferentes. O BusyBox integrado no KernelSU está localizado em `/data/adb/ksu/bin/busybox`, enquanto no Magisk está em `/data/adb/magisk/busybox`. **Observe que este é um comportamento interno do KernelSU e pode mudar no futuro!**
- O KernelSU não suporta arquivos `.replace`, mas oferece suporte às variáveis ​​`REMOVE` e `REPLACE` para remover ou substituir arquivos e pastas.
- O KernelSU adiciona o estágio `boot-completed` para executar scripts após a inicialização ser concluída.
- O KernelSU adiciona o estágio `post-mount` para executar scripts após o OverlayFS ser montado.
