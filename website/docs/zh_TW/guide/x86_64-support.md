# x86_64 支援

KernelSU 完全支援 `x86_64` 架構。但由於上游核心近期的安全性變更，在較新的 `x86_64` 核心上整合 KernelSU 時，需要額外處理，才能讓統一的 syscall dispatcher 正常運作。

## 為什麼會失效？

在較新的核心版本中，引入了一個 [提交](https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/commit/?id=1e3ad78334a69b36e107232e337f9d693dcc9df2) 來加固 syscall table。這項變更將系統呼叫路徑中的間接跳轉轉換成一系列直接條件分支。

KernelSU 的 `syscall_hook` 機制依賴對系統呼叫表的修改，藉此將被攔截的系統呼叫路由到統一 dispatcher。由於新的加固機制改變了系統呼叫路徑，核心會忽略這些對系統呼叫表的修改。若 KernelSU 在沒有正確處理這項限制的情況下載入並 hook syscall table，將無法正確路由呼叫，並會主動中止初始化，以避免核心 panic。

## 如何修復？

現在有兩種受支援的方式可處理 `x86_64` 上的 syscall hook 問題：

1. 啟用核心編譯選項 `KSU_X86_PATCH_SYSCALL_DISPATCHER`。
2. 繼續使用原本的核心原始碼補丁方案。

二選一即可，請不要同時使用兩種方案。

### 方案 1：啟用 `KSU_X86_PATCH_SYSCALL_DISPATCHER`

KernelSU 3.2.6 為 `x86_64` 新增了官方機制，也就是編譯選項 `KSU_X86_PATCH_SYSCALL_DISPATCHER`。

啟用後，KernelSU 會在執行時動態 patch 已加固的 syscall dispatcher，讓 syscall hook 可以正常運作，而不再依賴先前那組核心原始碼補丁。如果你使用的是 KernelSU 3.2.6 或更新版本，且可以調整 KernelSU 的建置設定，建議優先採用這種方式。

### 方案 2：套用原本的核心原始碼補丁

如果你不想啟用 `KSU_X86_PATCH_SYSCALL_DISPATCHER`，也可以繼續沿用原本的核心補丁方案。

要讓 KernelSU 在這些較新的核心上運作，需要套用補丁以繞過這項 syscall 加固。

::: danger 安全警告
使用這兩種方案中的任一種，都代表你正在主動繞過或削弱一項用於防禦推測執行漏洞的緩解措施。

這會重新暴露系統呼叫路徑中的間接分支攻擊面。**如果你的環境是正式生產伺服器，或對側通道安全有嚴格要求，請不要使用這兩種方案。** 這些方案僅適用於測試環境，前提是你更重視透過 KernelSU 取得 root 能力，而不是這項特定硬體漏洞緩解。
:::

請根據你的核心版本選擇並套用下列對應補丁。這些補丁會建立一個名為 `X86_FEATURE_INDIRECT_SAFE` 的特性，並可透過核心命令列參數 `syscall_hardening=off` 啟用。

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

## 應該選哪種方案？

- 如果你使用的是 KernelSU 3.2.6 或更新版本，且可以修改 KernelSU 建置設定，建議啟用 `KSU_X86_PATCH_SYSCALL_DISPATCHER`。
- 如果你想維持既有的核心補丁工作流程，則繼續使用上面的原始碼補丁方案。
