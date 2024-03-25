# App Profile {#app-profile}

App Profile 是 KernelSU 提供的一種針對各種應用程式自訂其使用配置的機制。

對於授予了root 權限（也即可以使用 `su`）的應用程式來說，App Profile 也可以稱為Root Profile，它可以自訂 `su` 的 `uid`, `gid` , `groups` , ` capabilities` 以及 `SELinux context` 規則，從而限制 root 使用者的權限；例如可以針對防火牆應用程式僅授予網路權限，而不授予檔案存取權限，針對凍結類別應用程式僅授予 shell 權限而不是直接給 root ；透過最小化權限原則**把權力關進籠子裡**。

對於沒有被授予 root 權限的普通應用，App Profile 可以控制核心以及模組系統對此應用的行為；例如是否需要針對此應用程式卸載模組造成的修改等。核心和模組系統可以透過此配置決定是否要做一些類似「隱藏痕跡」類別的操作。

## Root Profile {#root-profile}

### UID、GID 和 groups {#uid-gid-and-groups}

Linux 系統中有使用者和群組兩個概念。每個使用者都有一個使用者 ID(UID)，一個使用者可以屬於多個群組，每個群組也有群組 ID(GID)。此 ID 用於識別系統的使用者並確定使用者可以存取哪些系統資源。

UID 為 0 的使用者稱為 root 使用者，GID 為 0 的群組稱為 root 群組；root 使用者群組通常擁有系統的最高權限。

對於 Android 系統來說，每個應用程式都是一個單獨的使用者（不考慮 share uid 的情況），擁有一個唯一的 UID。例如 `0` 是 root 使用者，`1000` 是 `system`，`2000` 是 ADB shell，10000-19999 的是一般使用者。

:::info
此處的 UID 跟 Android 系統的多使用者，或者說工作資料（Work Profile），不是概念。工作資料實際上是對 UID 進行分片實現的，例如 10000-19999 是主使用者，110000-119999 是工作資料；他們中的任何一個普通應用都擁有自己獨有的 UID。
:::

每一個應用程式可以有若干個群組，GID 使其主要的群組，通常與 UID 一致；其他的群組稱為補充群組(groups)。某些權限是透過群組控制的，例如網路訪問，藍牙等。

例如，如果我們在 ADB shell 中執行 `id` 指令，會得到以下輸出：

```sh
oriole:/ $ id
uid=2000(shell) gid=2000(shell) groups=2000(shell),1004(input),1007(log),1011(adb),1015(sdcard_rw),1028(sdcard_r),1078(ext_data_ww) (ext_obb_rw),3001(net_bt_admin),3002(net_bt),3003(inet),3006(net_bw_stats),3009(readproc),3011(uhid),3012(readreadtracefs:s05:
```

其中，UID 為`2000`，GID 也即主要組ID 也為`2000`；除此之外它還在許多補充組裡面，例如`inet` 組代表可以創建`AF_INET` 和`AF_INET6` 的socket（存取網路），`sdcard_rw` 代表可以讀寫sdcard 等。

KernelSU 的 Root Profile 可以自訂執行 `su` 後 root 程式的 UID, GID 和 groups。例如，你可以設定某個 root 應用程式的Root Profile 其UID 為`2000`，這表示此應用程式在使用`su` 的時候，它的實際權限是ADB Shell 等級；你可以去掉groups 中的`inet` ，這樣這個`su` 就無法存取網路。

:::tip 注意
App Profile 只是控制 root 應用程式使用 `su` 後的權限，它並非控制應用程式本身的權限！如果應用程式本身申請了網路存取權限，那麼它即使不使用 `su` 也可以存取網路；為 `su` 去掉 `inet` 群組只是讓 `su` 無法存取網路。
:::

與應用程式透過 `su` 主動切換使用者或群組不同，Root Profile 是在核心中強制實施的，不依賴 root 應用程式的自覺行為，`su` 權限的授予完全取決於使用者而非開發者。

### Capabilities {#capabilities}

Capabilities 是 Linux 的一種分權機制。

傳統的 UNIX 系統為了執行權限檢查，將流程分為兩類：特權程式（其有效使用者 ID 為 0，稱為超級使用者或 root）和非特權程式（其有效 UID 為非零）。特權程式會繞過所有核心權限檢查，而非特權程式則根據其憑證（通常是有效UID、有效GID和補充群組清單）進行完整的權限檢查。

從 Linux 2.2開始，Linux 將傳統上與超級使用者關聯的特權分解為獨立的單元，稱為 Capabilities（有的也翻譯為「權能」），它們可以獨立啟用和停用。

每一個 Capability 代表一個或一類權限。例如 `CAP_DAC_READ_SEARCH` 就代表是否有能力繞過檔案讀取權限檢查和目錄讀取和執行權限檢查。如果一個有效 UID 為 `0` 的使用者（root 使用者）沒有 `CAP_DAC_READ_SEARCH` 或更高 Capalities，這表示即使它是 root 也不能​​隨意讀取檔案。

KernelSU 的 Root Profile 可以自訂執行 `su` 後 root 程式的 Capabilities，從而實現只授予「部分 root 權限」。與上面介紹的UID, GID 不同，某些 root 應用就是需要 `su` 後 UID 是 `0`，此時我們可以透過限制這個 UID 為 `0` 的 root 使用者的 Capabilities，就可以限制它能夠執行的操作。

:::tip 強烈建議
Linux 系統關於 Capability 的[官方文件](https://man7.org/linux/man-pages/man7/capabilities.7.html)，解釋了每一項Capability 所代表的能力，寫的非常詳細，如果你想要自訂Capabilities，請務必先閱讀此文件。
:::

### SELinux {#selinux}

SELinux 是一種強大的強制權限存取控制（MAC）機制。它按照**預設拒絕**的原則運作：任何未經明確允許的行為都會被拒絕。

SELinux 可依兩種全域模式運作：

1. 寬容模式：權限拒絕事件會被記錄下來，但不會被強制執行。
2. 強制模式：權限拒絕事件會被記錄下來**並且**強制執行。

:::warning 警告
現代的 Android 系統極度依賴 SELinux 來保障整個系統的安全性，我們強烈建議您不要使用任何以「寬容模式」運作的自訂系統，因為那樣與裸奔沒什麼區別。
:::

SELinux 的完整概念比較複雜，我們這裡不打算講解它的具體運作方式，建議你先透過以下資料來了解其運作原理：

1. [wikipedia](https://en.wikipedia.org/wiki/Security-Enhanced_Linux)
2. [Redhat: what-is-selinux](https://www.redhat.com/en/topics/linux/what-is-selinux)
3. [ArchLinux: SELinux](https://wiki.archlinux.org/title/SELinux)

KernelSU 的 Root Profile 可以自訂執行 `su` 後 root 程式的 SELinux context，並且可以針對這個 context 設定特定的存取控制規則，從而更精細地控制 root 權限。

通常情況下，應用程式執行 `su` 後，會將進程切換到一個**不受任何限制** 的SELinux 域，例如`u:r:su:s0`，透過 Root Profile，我們可以將它切換到一個自訂的網域，例如 `u:r:app1:s0`，然後為這個網域制定一系列規則：

```sh
type app1
enforce app1
typeattribute app1 mlstrustedsubject
allow app1 * * *
```

注意：此處的 `allow app1 * * *` 僅僅作為演示方便而使用，實際過程中不應使用這個規則，因為它跟 permissive 區別不大。

### 逃逸 {#escalation}

如果 Root Profile 的配置不合理，那麼可能會發生逃逸的情況：Root Profile 的限制會意外失效。

例如，如果你為ADB shell 使用者設定允許root 權限（這是相當常見的情況）；然後你給某個普通應用程式允許root 權限，但是配置它的root profile 中的UID 為2000（ADB shell 使用者的UID）；那麼此時，這個App 可以透過執行兩次 `su` 來獲得完整的root 權限：

1. 第一次執行 `su`，由於 App Profile 強制生效，會正常切換到 UID 為 `2000(adb shell)` 而非 `0(root)`。
2. 第二次執行 `su`，由於此時它 UID 是 `2000`，而你給 `2000(adb shell)` 配置了允許 root，它會獲得完整的 root 權限！

:::warning 注意
這是完全符合預期的行為，並非 BUG！因此我們建議：

如果你的確需要給 adb 授予 root 權限（例如你是開發者），那麼不建議你在配置 Root Profile 的時候將 UID 改成 `2000`，用 `1000(system)` 會更好。
:::

## Non Root Profile {#non-root-profile}

### 卸載模組 {#umount-modules}

KernelSU 提供了一種 systemless 的方式來修改系統分區，這是透過掛載 overlayfs 來實現的。但有些情況下，App 可能會對這種行為比較敏感；因此，我們可以透過設定「卸載模組」來卸載掛載在這些應用程式上的模組。

另外，KernelSU 管理器的設定介面還提供了一個「預設卸載模組」的開關，這個開關預設是**開啟**的，這表示**如果不對應用程式做額外的設定**，預設情況下 KernelSU 或某些模組會對此應用程式執行卸載操作。當然，如果你不喜歡這個設定或這個設定會影響某些 App，你可以有以下選擇：

1. 保持「預設卸載模組」的開關，然後針對不需要「卸載模組」的應用程式進行單獨的設置，在 App Profile 中關閉「卸載模組」；（相當於「白名單」）。
2. 關閉「預設卸載模組」的開關，然後針對需要「卸載模組」的應用程式進行單獨的設置，在 App Profile 中開啟「卸載模組」；（相當於「黑名單」）。

:::info
KernelSU 在 5.10 及以上內核上，內核無須任何修改就可以卸載模組；但在 5.10 以下的設備上，這個開關僅僅是一個“設定”，KernelSU 本身不會做任何動作，如果你希望在 5.10 以前的內核可以卸載模組，你需要將 `path_unmount` 函數向後移植到 `fs/namespace.c`，您可以在[如何為非 GKI 核心整合 KernelSU](/zh_TW/guide/how-to-integrate-for-non-gki.html)的末尾獲取更多資訊。一些模組（如 ZygiskNext）也會透過這個設定決定是否需要卸載。
:::