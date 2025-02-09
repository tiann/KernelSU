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

Se você instalou um módulo que foi comprovadamente seguro, mas faz com que seu dispositivo não inicialize, então esta situação é facilmente recuperável no KernelSU sem qualquer preocupação. O KernelSU possui mecanismos integrados para recuperar seu dispositivo, incluindo o seguinte:

1. Atualização AB
2. Recupere pressionando o botão de diminuir volume

#### Atualização AB

As atualizações do módulo KernelSU são baseadas no mecanismo de atualização AB do sistema Android usado em atualizações OTA. Quando você instala um novo módulo ou atualiza um existente, isso não modifica diretamente o arquivo do módulo atualmente em uso. Em vez disso, todos os módulos são integrados em uma nova imagem de atualização. Após o sistema ser reiniciado, ele tentará iniciar usando essa nova imagem de atualização. Se o sistema Android inicializar com sucesso, os módulos serão efetivamente atualizados.

Portanto, o método mais simples e comumente usado para recuperar seu dispositivo é **forçar uma reinicialização**. Se você não conseguir iniciar o sistema após instalar um módulo, pode pressionar e segurar o botão liga/desliga por mais de 10 segundos, e o sistema será reiniciado automaticamente. Após a reinicialização, ele retornará ao estado anterior à atualização do módulo, e os módulos atualizados serão desativados automaticamente.

#### Recupere pressionando o botão de diminuir volume

Se a Atualização AB ainda não resolveu o problema, você pode tentar usar o **Modo de Segurança**. Nesse modo, todos os módulos são desativados.

Existem duas maneiras de entrar no Modo de Segurança:

1. O Modo de Segurança integrado de alguns sistemas: Alguns sistemas possuem um Modo de Segurança integrado que pode ser acessado pressionando longamente o botão de diminuir volume. Em outros sistemas (como o HyperOS), o Modo de Segurança pode ser ativado a partir do Recovery. Ao entrar no Modo de Segurança do sistema, o KernelSU também entrará nesse modo e desativará automaticamente os módulos.
2. O Modo de Segurança integrado do KernelSU: Nesse caso, o método é **pressionar a tecla de diminuir volume continuamente por mais de três vezes** após a primeira tela de inicialização.

Após entrar no Modo de Segurança, todos os módulos na página Módulos do gerenciador do KernelSU serão desativados. Porém, você ainda pode realizar a operação de "desinstalação" para desinstalar quaisquer módulos que possam estar causando problemas.

O Modo de Segurança integrado é implementado no kernel, portanto não há possibilidade de perder eventos importantes devido à interceptação. No entanto, para kernels não-GKI, pode ser necessária uma integração manual do código. Para isso, consulte a documentação oficial para orientações.

### Módulos maliciosos

Se os métodos acima não conseguirem recuperar seu dispositivo, é muito provável que o módulo que você instalou tenha operações maliciosas ou tenha danificado seu dispositivo de outra forma. Nesse caso, há apenas duas sugestões:

1. Limpar os dados e instalar o sistema oficial.
2. Consultar o serviço pós-venda.
