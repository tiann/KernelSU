# x86_64 Support

KernelSU fully supports the `x86_64` architecture. However, due to recent upstream kernel security changes, integrating KernelSU on modern `x86_64` kernels requires additional handling so our unified syscall dispatcher can function correctly.

## Why did it break?

In newer kernel versions, a [commit](https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/commit/?id=1e3ad78334a69b36e107232e337f9d693dcc9df2) was introduced to harden the syscall table. This change converted indirect branches within the system call path into a series of direct conditional branches. 

KernelSU's `syscall_hook` mechanism relies on modifying entries in the syscall table so intercepted system calls can be routed to our unified dispatcher. Because the new hardening changes the syscall path, the kernel ignores those syscall table modifications. If KernelSU attempts to load and hook the syscall table without handling this limitation correctly, it will fail to route the calls and cleanly abort initialization to prevent a kernel panic.

## How to fix it?

There are two supported ways to handle this syscall hook issue on `x86_64`:

1. Enable the kernel build option `KSU_X86_PATCH_SYSCALL_DISPATCHER`.
2. Continue using the original kernel source patch method.

You only need to use one of them. Do not apply both at the same time.

### Option 1: Enable `KSU_X86_PATCH_SYSCALL_DISPATCHER`

KernelSU 3.2.6 introduced an official mechanism for `x86_64`: the build option `KSU_X86_PATCH_SYSCALL_DISPATCHER`.

When this option is enabled, KernelSU dynamically patches the hardened syscall dispatcher at runtime so the syscall hook can work without requiring the previous kernel source patch set. This is the recommended approach if you are building a kernel with KernelSU 3.2.6 or newer.

### Option 2: Apply the original kernel source patches

If you do not want to enable `KSU_X86_PATCH_SYSCALL_DISPATCHER`, you can continue to use the original kernel patch approach.

To make KernelSU work on these newer kernels, apply a patch that allows you to bypass this specific syscall hardening.

::: danger SECURITY WARNING
By using either of these two solutions, you are intentionally bypassing or weakening a mitigation designed to protect against speculative execution vulnerabilities.

This re-opens the indirect branch attack surface for system calls. **Do not use either solution if you are running a production server or a system where strict side-channel security is critical.** These approaches are intended for testing environments where root access via KernelSU is prioritized over this specific hardware vulnerability mitigation.
:::

Choose and apply the patches that match your kernel version below. These patches create a feature called `X86_FEATURE_INDIRECT_SAFE` and can be activated using the kernel cmdline `syscall_hardening=off`.

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

## Which method should I choose?

- If you are using KernelSU 3.2.6 or newer and can change the KernelSU build configuration, enable `KSU_X86_PATCH_SYSCALL_DISPATCHER`.
- If you prefer to keep your current kernel-side patch workflow, continue using the original source patches above.

