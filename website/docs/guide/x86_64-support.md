# x86_64 Support

KernelSU fully supports the `x86_64` architecture. However, due to recent upstream kernel security changes, integrating KernelSU on modern `x86_64` kernels requires manual patching to allow our unified syscall dispatcher to function correctly.

## Why did it break?

In newer kernel versions, a [commit](https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/commit/?id=1e3ad78334a69b36e107232e337f9d693dcc9df2) was introduced to harden the syscall table. This change converted indirect branches within the system call path into a series of direct conditional branches. 

KernelSU's `syscall_hook` mechanism relies on an indirect jump to route intercepted system calls to our unified dispatcher. Because the new hardening forces direct calls, the kernel blocks our dispatcher. If KernelSU attempts to load and hook the syscall table without the proper mitigations disabled, it will fail to route the calls and cleanly abort initialization to prevent a kernel panic.

## How to fix it?

To make KernelSU work on these newer kernels, you must apply a patch that allows you to bypass this specific syscall hardening.

::: danger SECURITY WARNING
By applying these patches and disabling syscall hardening, you are intentionally bypassing a mitigation designed to protect against speculative execution vulnerabilities. 

This re-opens the indirect branch attack surface for system calls. **Do not apply these patches if you are running a production server or a system where strict side-channel security is critical.** This is intended for testing environments where root access via KernelSU is prioritized over this specific hardware vulnerability mitigation.
:::

Choose & apply the patches that match your kernel version below. These patches will create a feature called `X86_FEATURE_INDIRECT_SAFE` and can be activated using the cmdline `syscall_hardening=off`.

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
