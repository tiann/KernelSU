# x86_64 支持

KernelSU 完全支持 `x86_64` 架构。但由于上游内核近期的安全改动，在较新的 `x86_64` 内核上集成 KernelSU 时，需要额外处理，才能让统一的 syscall dispatcher 正常工作。

## 为什么会失效？

在较新的内核版本中，引入了一个 [提交](https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/commit/?id=1e3ad78334a69b36e107232e337f9d693dcc9df2) 来加固 syscall table。这个改动把系统调用路径中的间接跳转转换成了一系列直接条件分支。

KernelSU 的 `syscall_hook` 机制依赖对系统调用表的修改，从而把被拦截的系统调用路由到统一 dispatcher。由于新的加固机制改变了系统调用路径，内核会忽略这些对系统调用表的修改。若 KernelSU 在没有正确处理该限制的情况下加载并 hook syscall table，将无法正确路由调用，并会主动中止初始化，以避免内核 panic。

## 如何修复？

现在有两种受支持的方式来处理 `x86_64` 上的 syscall hook 问题：

1. 启用内核编译选项 `KSU_X86_PATCH_SYSCALL_DISPATCHER`。
2. 继续使用原有的内核源码补丁方案。

二选一即可，不要同时使用两种方案。

### 方案 1：启用 `KSU_X86_PATCH_SYSCALL_DISPATCHER`

KernelSU 3.2.6 为 `x86_64` 新增了官方机制，即编译选项 `KSU_X86_PATCH_SYSCALL_DISPATCHER`。

启用后，KernelSU 会在运行时动态 patch 已加固的 syscall dispatcher，使 syscall hook 可以正常工作，而不再依赖之前那组内核源码补丁。如果你使用的是 KernelSU 3.2.6 或更新版本，并且可以调整 KernelSU 的构建配置，建议优先采用这种方式。

### 方案 2：应用原有的内核源码补丁

如果你不想启用 `KSU_X86_PATCH_SYSCALL_DISPATCHER`，也可以继续沿用原有的内核补丁方案。

要让 KernelSU 在这些较新的内核上工作，需要应用补丁以绕过这项 syscall 加固。

::: danger 安全警告
使用这两种方案中的任意一种，都意味着你在主动绕过或削弱一项用于防御推测执行漏洞的缓解措施。

这会重新暴露系统调用路径中的间接分支攻击面。**如果你的环境是生产服务器，或者对侧信道安全有严格要求，请不要使用这两种方案。** 这些方案仅适用于测试环境，前提是你更重视通过 KernelSU 获得 root 能力，而不是这项特定硬件漏洞缓解。
:::

请根据你的内核版本选择并应用下面对应的补丁。这些补丁会创建一个名为 `X86_FEATURE_INDIRECT_SAFE` 的特性，并可通过内核命令行参数 `syscall_hardening=off` 启用。

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

## 应该选择哪种方案？

- 如果你使用的是 KernelSU 3.2.6 或更新版本，且可以修改 KernelSU 构建配置，建议启用 `KSU_X86_PATCH_SYSCALL_DISPATCHER`。
- 如果你希望继续保持现有的内核补丁工作流，则继续使用上面的源码补丁方案。
