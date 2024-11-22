# Perfil do Aplicativo

O Perfil do Aplicativo é um mecanismo fornecido pelo KernelSU para personalizar a configuração de vários apps.

Para apps com privilégios root (ou seja, capazes de usar `su`), o Perfil do Aplicativo também pode ser chamado de Perfil root. Ele permite a customização das regras `uid`, `gid`, `grupos`, `capacidades` e `SELinux` do comando `su`, restringindo assim os privilégios do usuário root. Por exemplo, ele pode conceder permissões de rede apenas para apps de firewall enquanto nega permissões de acesso a arquivos, ou pode conceder permissões de shell em vez de acesso root completo para apps congelados: **mantendo o poder confinado com o princípio do menor privilégio.**

Para apps comuns sem privilégios root, o Perfil do Aplicativo pode controlar o comportamento do kernel e do sistema de módulos em relação a esses apps. Por exemplo, pode determinar se as modificações resultantes dos módulos devem ser abordadas. O kernel e o sistema de módulos podem tomar decisões com base nesta configuração, como realizar operações semelhantes a "ocultar".

## Perfil root

### UID, GID e Grupos

Os sistemas Linux possuem dois conceitos: usuários e grupos. Cada usuário possui um ID de usuário (UID) e um usuário pode pertencer a vários grupos, cada um com seu próprio ID de grupo (GID). Esses IDs são usados ​​para identificar usuários no sistema e determinar quais recursos do sistema eles podem acessar.

Os usuários com UID 0 são conhecidos como usuários root e os grupos com GID 0 são conhecidos como grupos root. O grupo de usuários root normalmente possui os privilégios de sistema mais altos.

No caso do sistema Android, cada app é um usuário separado (excluindo cenários de UID compartilhados) com um UID exclusivo. Por exemplo, `0` representa o usuário root, `1000` representa `system`, `2000` representa o ADB shell e UIDs variando de `10000` a `19999` representam apps comuns.

:::info INFORMAÇÕES
Aqui, o UID mencionado não é o mesmo que o conceito de múltiplos usuários ou perfis de trabalho no sistema Android. Os perfis de trabalho são, na verdade, implementados particionando o intervalo UID. Por exemplo, 10000-19999 representa o usuário principal, enquanto 110000-119999 representa um perfil de trabalho. Cada app comum entre eles possui seu próprio UID exclusivo.
:::

Cada app pode ter vários grupos, com o GID representando o grupo principal, que geralmente corresponde ao UID. Outros grupos são conhecidos como grupos suplementares. Certas permissões são controladas por meio de grupos, como permissões de acesso à rede ou acesso Bluetooth.

Por exemplo, se executarmos o comando `id` no ADB shell, a saída pode ser semelhante a esta:

```sh
oriole:/ $ id
uid=2000(shell) gid=2000(shell) groups=2000(shell),1004(input),1007(log),1011(adb),1015(sdcard_rw),1028(sdcard_r),1078(ext_data_rw),1079(ext_obb_rw),3001(net_bt_admin),3002(net_bt),3003(inet),3006(net_bw_stats),3009(readproc),3011(uhid),3012(readtracefs) context=u:r:shell:s0
```

Aqui, o UID é `2000` e o GID (ID do grupo primário) também é `2000`. Além disso, pertence a vários grupos suplementares, como `inet` (indicando a capacidade de criar soquetes `AF_INET` e `AF_INET6`) e `sdcard_rw` (indicando permissões de leitura/gravação para o cartão SD).

O Perfil root do KernelSU permite a personalização do UID, GID e grupos para o processo root após a execução de `su`. Por exemplo, o Perfil root de um app root pode definir seu UID como `2000`, que significa que ao usar `su`, as permissões reais do app estão no nível do ADB shell. O grupo `inet` pode ser removido, evitando que o comando `su` acesse a rede.

:::tip OBSERVAÇÃO
O Perfil do Aplicativo controla apenas as permissões do processo root após usar `su`, e ele não controla as permissões do próprio app. Se um app solicitou permissão de acesso à rede, ele ainda poderá acessar a rede mesmo sem usar `su`. Remover o grupo `inet` de `su` apenas impede que `su` acesse a rede.
:::

O Perfil root é aplicado no kernel e não depende do comportamento voluntário de apps root, ao contrário da troca de usuários ou grupos por meio de `su`. A concessão da permissão `su` depende inteiramente do usuário e não do desenvolvedor.

### Capacidades

As capacidades são um mecanismo para separação de privilégios no Linux.

Para realizar verificações de permissão, as implementações tradicionais do `UNIX` distinguem duas categorias de processos: processos privilegiados (cujo ID de usuário efetivo é `0`, referido como superusuário ou root) e processos sem privilégios (cujo UID efetivo é diferente de zero). Os processos privilegiados ignoram todas as verificações de permissão do kernel, enquanto os processos não privilegiados estão sujeitos à verificação completa de permissão com base nas credenciais do processo (geralmente: UID efetivo, GID efetivo e lista de grupos suplementares).

A partir do Linux 2.2, o Linux divide os privilégios tradicionalmente associados ao superusuário em unidades distintas, conhecidas como capacidades, que podem ser ativadas e desativadas de forma independente.

Cada capacidade representa um ou mais privilégios. Por exemplo, `CAP_DAC_READ_SEARCH` representa a capacidade de ignorar verificações de permissão para leitura de arquivos, bem como permissões de leitura e execução de diretório. Se um usuário com um UID efetivo `0` (usuário root) não tiver recursos `CAP_DAC_READ_SEARCH` ou superiores, isso significa que mesmo sendo root, ele não pode ler arquivos à vontade.

O Perfil root do KernelSU permite a personalização das capacidades do processo root após a execução de `su`, conseguindo assim conceder parcialmente "privilégios root". Ao contrário do UID e GID mencionados acima, certos apps root exigem um UID de `0` após usar `su`. Nesses casos, limitar as capacidades deste usuário root com UID `0` pode restringir suas operações permitidas.

:::tip FORTE RECOMENDAÇÃO
A [documentação oficial](https://man7.org/linux/man-pages/man7/capabilities.7.html) da capacidade do Linux fornece explicações detalhadas das habilidades representadas por cada capacidade. Se você pretende customizar as capacidade, é altamente recomendável que você leia este documento primeiro.
:::

### SELinux

SELinux é um poderoso mecanismo do Controle de Acesso Obrigatório (MAC). Ele opera com base no princípio de **negação padrão**. Qualquer ação não explicitamente permitida é negada.

O SELinux pode ser executado em dois modos globais:

1. Modo permissivo: Os eventos de negação são registrados, mas não aplicados.
2. Modo de aplicação: Os eventos de negação são registrados e aplicados.

:::warning AVISO
Os sistemas Android modernos dependem fortemente do SELinux para garantir a segurança geral do sistema. É altamente recomendável não usar nenhum sistema personalizado executado em "Modo permissivo", pois não oferece vantagens significativas em relação a um sistema completamente aberto.
:::

Explicar o conceito completo do SELinux é complexo e está além do objetivo deste documento. Recomenda-se primeiro entender seu funcionamento através dos seguintes recursos:

1. [Wikipédia](https://en.wikipedia.org/wiki/Security-Enhanced_Linux)
2. [Red Hat: O que é SELinux?](https://www.redhat.com/pt-br/topics/linux/what-is-selinux)
3. [ArchLinux: SELinux](https://wiki.archlinux.org/title/SELinux)

O Perfil root do KernelSU permite a personalização do contexto SELinux do processo root após a execução de `su`. Regras específicas de controle de acesso podem ser definidas para este contexto para permitir um controle refinado sobre os privilégios root.

Em cenários típicos, quando um app executa `su`, ele alterna o processo para um domínio SELinux com **acesso irrestrito**, como `u:r:su:s0`. Através do Perfil root, este domínio pode ser mudado para um domínio personalizado, como `u:r:app1:s0`, e uma série de regras podem ser definidas para este domínio:

```sh
type app1
enforce app1
typeattribute app1 mlstrustedsubject
allow app1 * * *
```

Observe que a regra `allow app1 * * *` é usada apenas para fins de demonstração. Na prática, esta regra não deve ser utilizada extensivamente, pois não difere muito do Modo permissivo.

### Escalação

Se a configuração do Perfil root não estiver definida corretamente, poderá ocorrer um cenário de escalação. As restrições impostas pelo Perfil root poderão falhar involuntariamente.

Por exemplo, se você conceder permissão root a um usuário ADB shell (que é um caso comum) e, em seguida, conceder permissão root a um app normal, mas configurar seu Perfil root com UID 2000 (que é o UID do usuário ADB shell), o app pode obter acesso root completo executando o comando `su` duas vezes:

1. A primeira execução `su` está sujeita à aplicação do Perfil do Aplicativo e mudará para UID `2000` (ADB shell) em vez de `0` (root).
2. A segunda execução `su`, como o UID é `2000` e você concedeu acesso root ao UID `2000` (ADB shell) na configuração, o app obterá privilégios root completo.

:::warning OBSERVAÇÃO
Este comportamento é totalmente esperado e não é um bug. Portanto, recomendamos o seguinte:

Se você realmente precisa conceder privilégios root ao ADB (por exemplo, como desenvolvedor), não é aconselhável alterar o UID para `2000` ao configurar o Perfil root. Usar `1000` (system) seria uma melhor escolha.
:::

## Perfil não root

### Desmontar módulos

O KernelSU fornece um mecanismo sem sistema para modificar partições do sistema, obtido através da montagem de OverlayFS. No entanto, alguns apps podem ser sensíveis a esse comportamento. Assim, podemos descarregar módulos montados nesses apps configurando a opção "Desmontar módulos".

Além disso, a interface de configurações do gerenciador do KernelSU fornece uma opção para "Desmontar módulos por padrão". Por padrão, essa opção está **ativada**, o que significa que o KernelSU ou alguns módulos descarregarão módulos para este app, a menos que configurações adicionais sejam aplicadas. Se você não preferir esta configuração ou se ela afetar determinados apps, você terá as seguintes opções:

1. Mantenha a opção "Desmontar módulos por padrão" e desative individualmente a opção "Desmontar módulos" no Perfil do Aplicativo para apps que exigem carregamento do módulo (agindo como uma "lista de permissões").
2. Desative a opção "Desmontar módulos por padrão" e ative individualmente a opção "Desmontar módulos" no Perfil do Aplicativo para apps que exigem descarregamento do módulo (agindo como uma "lista negra").

:::info INFORMAÇÕES
Em dispositivos que utilizam a versão do kernel 5.10 e superior, o kernel realiza qualquer ação adicional do descarregamento de módulos. No entanto, para dispositivos que executam versões do kernel abaixo de 5.10, essa opção é apenas uma opção de configuração e o próprio KernelSU não executa nenhuma ação. Se você quiser usar a opção "Desmontar módulos" em versões do kernel anteriores a 5.10 você precisa portar a função `path_umount` em `fs/namespace.c`, você pode obter mais informações no final da página [Como integrar o KernelSU para kernels não GKI](https://kernelsu.org/pt_BR/guide/how-to-integrate-for-non-gki.html). Alguns módulos, como ZygiskNext, também podem usar essa opção para determinar se o descarregamento do módulo é necessário.
:::
