# Diferença com Magisk

Embora existam muitas semelhanças entre os módulos KernelSU e os módulos Magisk, existem inevitavelmente algumas diferenças devido aos seus mecanismos de implementação completamente diferentes. Se você deseja que seu módulo seja executado no Magisk e no KernelSU, você deve entender essas diferenças.

## Semelhanças

- Formato de arquivo do módulo: ambos usam o formato zip para organizar os módulos, e o formato dos módulos é quase o mesmo.
- Diretório de instalação do módulo: ambos localizados em `/data/adb/modules`.
- Sem sistema: ambos suportam a modificação de `/system` de maneira sem sistema por meio de módulos.
- post-fs-data.sh: o tempo de execução e a semântica são exatamente os mesmos.
- service.sh: o tempo de execução e a semântica são exatamente os mesmos.
- system.prop: completamente o mesmo.
- sepolicy.rule: completamente o mesmo.
- BusyBox: os scripts são executados no BusyBox com o "Modo Autônomo" ativado em ambos os casos.

## Diferenças

Antes de entender as diferenças, você precisa saber diferenciar se o seu módulo está rodando no KernelSU ou Magisk. Você pode usar a variável de ambiente `KSU` para diferenciá-la em todos os locais onde você pode executar os scripts do módulo (`customize.sh`, `post-fs-data.sh`, `service.sh`). No KernelSU, esta variável de ambiente será definida como `true`.

Aqui estão algumas diferenças:

- Os módulos KernelSU não podem ser instalados no modo Recovery.
- Os módulos KernelSU não têm suporte integrado para Zygisk (mas você pode usar módulos Zygisk através do [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext).
- O método para substituir ou excluir arquivos nos módulos KernelSU é completamente diferente do Magisk. O KernelSU não suporta o método `.replace`. Em vez disso, você precisa criar um arquivo com o mesmo nome `mknod filename c 0 0` para excluir o arquivo correspondente.
- Os diretórios do BusyBox são diferentes. O BusyBox integrado no KernelSU está localizado em `/data/adb/ksu/bin/busybox`, enquanto no Magisk está em `/data/adb/magisk/busybox`. **Observe que este é um comportamento interno do KernelSU e pode mudar no futuro!**
- O KernelSU não suporta arquivos `.replace`, entretanto, o KernelSU suporta as variáveis ​​`REMOVE` e `REPLACE` para remover ou substituir arquivos e pastas.
- O KernelSU adiciona o estágio `boot-completed` para executar alguns scripts na inicialização concluída.
- O KernelSU adiciona o estágio `post-mount` para executar alguns scripts após montar OverlayFS.
