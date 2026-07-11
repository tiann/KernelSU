# Suporte a x86_64

O KernelSU oferece suporte completo à arquitetura `x86_64`. No entanto, devido a mudanças recentes de segurança no kernel upstream, integrar o KernelSU em kernels `x86_64` modernos exige um tratamento adicional para que nosso dispatcher unificado de syscall funcione corretamente.

## Por que isso quebrou?

Em versões mais novas do kernel, um [commit](https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/commit/?id=1e3ad78334a69b36e107232e337f9d693dcc9df2) foi introduzido para reforçar a proteção da syscall table. Essa mudança converteu desvios indiretos no caminho de system call em uma série de desvios condicionais diretos.

O mecanismo `syscall_hook` do KernelSU depende da modificação de entradas na syscall table para que syscalls interceptadas possam ser encaminhadas ao dispatcher unificado. Como o novo hardening muda o caminho das system calls, o kernel ignora essas modificações na syscall table. Se o KernelSU tentar carregar e aplicar hook na syscall table sem tratar corretamente essa limitação, ele não conseguirá encaminhar as chamadas e abortará a inicialização para evitar um kernel panic.

## Como corrigir?

Agora existem duas formas suportadas de lidar com esse problema de syscall hook em `x86_64`:

1. Ativar a opção de compilação do kernel `KSU_X86_PATCH_SYSCALL_DISPATCHER`.
2. Continuar usando o método original com patches no código-fonte do kernel.

Você só precisa usar uma delas. Não aplique ambas ao mesmo tempo.

### Opção 1: Ativar `KSU_X86_PATCH_SYSCALL_DISPATCHER`

O KernelSU 3.2.6 introduziu um novo mecanismo oficial para `x86_64`: a opção de build `KSU_X86_PATCH_SYSCALL_DISPATCHER`.

Quando essa opção está ativada, o KernelSU aplica um patch dinâmico no dispatcher hardened de syscall em tempo de execução, permitindo que o syscall hook funcione sem exigir o conjunto anterior de patches no código-fonte do kernel. Essa é a abordagem recomendada se você estiver compilando um kernel com KernelSU 3.2.6 ou mais recente.

### Opção 2: Aplicar os patches originais no código-fonte do kernel

Se você não quiser ativar `KSU_X86_PATCH_SYSCALL_DISPATCHER`, ainda pode continuar usando a abordagem original com patches no kernel.

Para fazer o KernelSU funcionar nesses kernels mais novos, aplique um patch que permita contornar esse hardening específico de syscall.

::: danger AVISO DE SEGURANÇA
Ao usar qualquer uma dessas duas soluções, você está intencionalmente contornando ou enfraquecendo uma mitigação projetada para proteger contra vulnerabilidades de execução especulativa.

Isso reabre a superfície de ataque de desvios indiretos para system calls. **Não use nenhuma dessas soluções se você estiver executando um servidor de produção ou um sistema em que a segurança contra side-channel seja crítica.** Essas abordagens se destinam a ambientes de teste, onde o acesso root via KernelSU é priorizado em relação a essa mitigação específica de vulnerabilidade de hardware.
:::

Escolha e aplique os patches correspondentes à versão do seu kernel abaixo. Esses patches criam um recurso chamado `X86_FEATURE_INDIRECT_SAFE` e podem ser ativados com o parâmetro de linha de comando do kernel `syscall_hardening=off`.

```
For kernel 6.6:
https://github.com/android-generic/kernel_common/commit/fe9a9b4c320577c30e1f22d04039e414c6a3cdec
https://github.com/android-generic/kernel_common/commit/df772e99e392f24b395ceaf7b26974e3e4828ee9

For kernel 6.12:
https://github.com/android-generic/kernel-zenith/commit/dd2c602268fdc81f4d3b662f6a15142ac0ec7bcd
https://github.com/android-generic/kernel-zenith/commit/7d99237ae5da61c19447138da3282ae37d43857b

For kernel 6.18:
https://github.com/android-generic/kernel-zenith/commit/40b1c323d1ad29c86e041d665c7f089b9a3ccfb5
https://github.com/android-generic/kernel-zenith/commit/f5813e10b7630e1ccd86fc2c4cf30eef60b64a82
```

## Qual método devo escolher?

- Se você estiver usando KernelSU 3.2.6 ou mais recente e puder alterar a configuração de build do KernelSU, ative `KSU_X86_PATCH_SYSCALL_DISPATCHER`.
- Se preferir manter seu fluxo atual baseado em patches no kernel, continue usando os patches de código-fonte acima.
