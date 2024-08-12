# 安裝 {#title}

## 檢查您的裝置是否受支援 {#check-if-your-device-is-supported}

從 [GitHub Releases](https://github.com/tiann/KernelSU/releases) 下載 KernelSU 管理器，然後安裝至裝置並開啟：

- 如果顯示「不支援」，則表示您的裝置不支援 KernelSU，您需要自行編譯核心才能繼續使用，KernelSU 官方也永遠不會提供一個您可以寫入的 Boot 映像。
- 如果顯示「未安裝」，那麼 KernelSU 支援您的裝置。

::: info 提示
對於顯示「不支援」的裝置，這裡有一個[非官方支援裝置清單](unofficially-support-devices.md)，您可以使用這個清單裡的核心自行編譯。
:::

## 備份您的原廠 boot.img {#backup-stock-boot-img}

在寫入核心映像前，您必須預先備份您的原廠 boot.img。如果您在後續寫入中出現了任何問題，您都可以透過使用 Fastboot 寫回原廠 Boot 以還原系統。

::: warning 警告
寫入核心映像可能會造成資料遺失，請確保做好這一步再繼續進行下一步作業！！必要時您還可以備份您手機的所有資料。
:::

## 必要知識 {#necessary-knowledge}

### ADB 和 Fastboot {#adb-and-fastboot}

預設狀況下，您將會使用 ADB 和 Fastboot 工具，如果您不知道它們，建議使用搜尋引擎先瞭解相關內容。

### KMI

KMI 全稱 Kernel Module Interface，相同 KMI 的核心版本是**相容的**，這也是 GKI 中「標準」的涵義所在。反之，如果 KMI 不同，那麼這些核心之間無法彼此相容，寫入與您裝置 KMI 不同的核心映像可能會導致無法開機。

具體來講，對於 GKI 的裝置，其核心版本格式應該如下：

```txt
KernelRelease :=
Version.PatchLevel.SubLevel-AndroidRelease-KmiGeneration-suffix
w      .x         .y       -zzz           -k            -something
```

其中，`w.x-zzz-k` 為 KMI 版本。例如，一部裝置核心版本為 `5.10.101-android12-9-g30979850fc20`，那麼它的 KMI 為 `5.10-android12-9`，理論上寫入其他這個 KMI 的核心也能正常開機。

::: tip 補充
請注意，核心版本中的 SubLevel 並非 KMI 的一部分！也就是說 `5.10.101-android12-9-g30979850fc20` 與 `5.10.137-android12-9-g30979850fc20` 的 KMI 相同！
:::

### 安全性修補程式等級 {#security-patch-level}

較新的 Android 裝置可能具有防回滾機制，不允許寫入具有較舊安全性修補程式等級的啟動映像。例如，如果您的裝置核心為 `5.10.101-android12-9-g30979850fc20`，則其安全修補程式等級為 `2023-11`；即使寫入了 KMI 對應的核心，如果安全修補程式等級早於 `2023-11`（例如 `2023-06`），也可能會導致無法開機。

因此，最好使用具有最新安全性修補程式等級的核心來維護與 KMI 的對應關係。

### 核心版本與 Android 版本 {#kernel-version-vs-android-version}

請注意：**核心版本與 Android 版本並不一定相同！**

如果您發現您的核心版本是 `android12-5.10.101`，然而您 Android 系統的版本為 Android 13 或更高，請不要覺得奇怪，因為 Android 系統的版本與 Linux 核心的版本號碼並非一致。Linux 核心的版本號碼一般與**裝置出廠時隨附的 Android 系統的版本一致**，如果後續 Android 系統更新，核心版本一般不會發生變化。如果您需要寫入，**請以核心版本為準！！**

## 安裝簡介 {#introduction}

自 `0.9.0` 版本以後，在 GKI 裝置上，KernelSU 支援兩種運作模式：

1. `GKI`：使用**通用核心鏡像**（GKI）取代掉裝置原有的核心。
2. `LKM`：使用**可載入核心模組**（LKM）的方式載入到裝置核心中，不會替換掉裝置原有的核心。

這兩種方式適用於不同的場景，你可以根據自己的需求選擇。

### GKI 模式 {#gki-mode}

GKI 模式會替換掉裝置原有的核心，使用 KernelSU 提供的通用核心鏡像。 GKI 模式的優點是：

1. 通用型高，適用於大多數裝置；例如開啟了 KNOX 的三星裝置、或是 LKM 模式無法運作的裝置。還有一些冷門的魔改裝置，也只能使用 GKI 模式。
2. 不依賴官方韌體即可使用；不需要等待官方韌體更新，只要 KMI 一致，就可以使用。

### LKM 模式 {#lkm-mode}

LKM 模式不會替換掉裝置原有的核心，而是使用可載入核心模組的方式載入到裝置核心中。 LKM 模式的優點是：

1. 不會取代裝置原有的核心：如果你對裝置原有的核心有特殊需求，或是你希望在使用第三方核心的同時使用 KernelSU，可以使用 LKM 模式。
2. 升級和 OTA 較為方便：升級 KernelSU 時，可以直接在管理器內部安裝，無需再手動寫入；系統 OTA 後，可以直接安裝到第二個槽位，也無需再手動寫入。
3. 適用於一些特殊場景：例如使用臨時 root 權限也可以載入 LKM，由於不需要替換 boot 分區，因此不會觸發 avb，不會使裝置意外變磚。
4. LKM 可以被暫時卸載：如果你暫時想取消 root，可以卸載 LKM，這個過程不需要寫入分區，甚至也不用重啟裝置。如果你想重新取得 root，只需要重啟裝置即可。

:::tip 兩種模式共存
打開管理器後，你可以在首頁看到裝置目前運行的模式。注意 GKI 模式的優先級高於 LKM ，如你既使用 GKI 核心替換掉了原有的核心，又使用 LKM 的方式修補了 GKI 核心，那麼 LKM 會被忽略，裝置將永遠以 GKI 的模式運作。
:::

### 選哪個？ {#which-one}

如果你的裝置是手機，我們建議您優先考慮 LKM 模式。
如果你的裝置是模擬器、WSA 或 Waydroid 等，我們建議您優先考慮 GKI 模式。

## LKM 安裝 {#lkm-installation}

### 取得官方韌體 {#get-the-official-firmware}

使用 LKM 的模式，需要取得官方韌體，然後在官方韌體的基礎上修補；如果你使用的是第三方核心，可以把第三方核心的 boot.img 作為官方韌體。

取得官方韌體的方法有很多，如果你的裝置支援 `fastboot boot`，那麼我們最推薦以及最簡單的方法是使用 `fastboot boot` 臨時啟動 KernelSU 提供的 GKI 核心，並參考[使用管理器](#use-the-manager)安裝。

如果你的裝置不支援 `fastboot boot`，那麼你可能需要手動去下載官方韌體包，然後從中提取 boot。

與 GKI 模式不同，LKM 模式會修改 `ramdisk`，因此在出廠 Android 13 的裝置上，通常它需要修補的是 `init_boot` 分區而非 `boot` 分區；而 GKI 模式則永遠是修改 `boot` 分區。

### 使用管理器 {#use-the-manager}

開啟管理器，點選右上角的安裝圖標，會出現若干個選項：

1. 選擇並修補一個文件：如果你手機目前沒有 root 權限，你可以選擇這個選項，然後選擇你的官方韌體，管理器會自動修補它。你只需要寫入這個修補後的文件，即可永久取得 root 權限。
2. 直接安裝：如果你手機已經 root，你可以選擇這個選項，管理器會自動獲取你的裝置資訊，然後自動修補官方韌體，然後寫入。你可以考慮使用 `fastboot boot` KernelSU 的 GKI 核心來取得臨時 root 安裝管理器，然後再使用這個選項。**這種方式也是 KernelSU 升級最主要的方式**。
3. 安裝到另一個分割區：如果你的裝置支援 A/B 分區，你可以選擇這個選項，管理器會自動修補官方韌體，然後安裝到另一個分區。這種方式適用於 OTA 後的裝置，你可以在 OTA 後直接安裝到另一個分割區，然後重新啟動裝置即可。

### 使用命令列{#use-the-command-line}

如果你不想使用管理器，你也可以使用命令列來安裝 LKM。KernelSU 提供的 `ksud` 可以幫助你快速修補官方韌體，然後寫入。

這個工具支援 macOS、Linux 和 Windows，你可以在 [GitHub Release](https://github.com/tiann/KernelSU/releases) 下載對應的版本。

使用方法：`ksud boot-patch`。 你可以查看命令列的提示了解具體的使用方法。

```sh
husky:/ # ksud boot-patch -h
Patch boot or init_boot images to apply KernelSU

Usage: ksud boot-patch [OPTIONS]

Options:
  -b, --boot <BOOT>              boot image path, if not specified, will try to find the boot image automatically
  -k, --kernel <KERNEL>          kernel image path to replace
  -m, --module <MODULE>          LKM module path to replace, if not specified, will use the builtin one
  -i, --init <INIT>              init to be replaced
  -u, --ota                      will use another slot when boot image is not specified
  -f, --flash                    Flash it to boot partition after patch
  -o, --out <OUT>                output path, if not specified, will use current directory
      --magiskboot <MAGISKBOOT>  magiskboot path, if not specified, will use builtin one
      --kmi <KMI>                KMI version, if specified, will use the specified KMI
  -h, --help                     Print help
```
需要說明的幾個選項：
1. `--magiskboot` 選項可以指定 magiskboot 的路徑，如果不指定，ksud 會在環境變數中尋找。如果你不知道如何取得 magiskboot，可以參考[這裡](#patch-boot-image)。
2. `--kmi` 選項可以指定 `KMI` 版本，如果你的裝置核心名字沒有遵循 KMI 規範，你可以透過這個選項來指定。

最常見的使用方法為：
```sh
ksud boot-patch -b <boot.img> --kmi android13-5.10
```
## GKI 安裝{#gki-mode-installation}
GKI 的安裝方式有以下幾種，各自適用於不同的場景，請依需求選擇：

1. 使用 KernelSU 提供的 boot.img 透過 Fastboot 安裝
2. 使用核心寫入程式 (例如 KernelFlasher) 安裝
3. 使用自訂 Recovery (例如 TWRP) 安裝
4. 手動修補 boot.img 並安裝

## 使用 KernelSU 提供的 boot.img 安裝 {#install-with-boot-img-provided-by-kernelsu}

如果你的裝置的 `boot.img` 使用常見的壓縮格式，你可以直接寫入 KernelSU 提供的 GKI 核心映像，這種方法無需 TWRP，也無需您的手機有 Root 權限；適用於您初次安裝 KernelSU。

### 找到合適的 boot.img {#find-proper-boot-img}

KernelSU 為 GKI 裝置提供了標準 boot.img，您需要將 boot.img 寫入至裝置的 Boot 分區。

您可以從 [GitHub Release](https://github.com/tiann/KernelSU/releases) 下載 boot.img，請注意，您應該使用正確版本的 boot.img。如果你不知道你該下載哪個檔案，請詳細閱讀文檔中的 [KMI](#kmi) 與[安全性修補程式等級](#security-patch-level)。

通常，在相同的 KMI 和安全性修補程式等級下，會存在三種不同格式的啟動檔案。除了核心壓縮格式之外，它們都是相同的。請檢查您原來的 boot.img 的核心壓縮格式。您應該使用正確的格式，例如 `lz4` 、 `gz`，如果你使用了不正確的壓縮格式，你可能會在寫入後無法開機。

::: info 關於 boot.img 的壓縮格式
1. 您可以透過 magiskboot 以取得您的原始 Boot 的壓縮格式。當然，您也可以詢問與您相同型號的其他更有經驗的使用者。另外，核心的壓縮格式通常不會出現變更，如果您使用的某個壓縮格式成功開機，後續可以優先嘗試這個格式。
2. 小米裝置通常 `gz` 或者 **不壓縮**。
3. Pixel 裝置有些特殊，請遵循下方的指示。
:::

### 將 boot.img 寫入至裝置 {#flash-boot-img-to-device}

使用 `adb` 連接您的裝置，然後執行 `adb reboot bootloader` 進入 fastboot 模式，然後使用此命令寫入 KernelSU：

```sh
fastboot flash boot boot.img
```

::: info 提示
如果您的裝置支援 `fastboot boot`，可以先使用 `fastboot boot boot.img` 來嘗試使用 boot.img 開機進入系統，如果出現意外，重新啟動即可開機。
:::

### 重新開機 {#reboot}

寫入完成後，您應該重新啟動您的裝置：

```sh
fastboot reboot
```

## 使用核心寫入程式安裝 {#install-with-kernel-flasher}

先決條件：您的裝置必須已經 Root。例如您已經安裝了 Magisk 並取得 Root 存取權，或者您已經安裝了舊版本的 KernelSU 需升級到其他版本的 KernelSU；如果您的裝置並未 Root，請嘗試其他方法。

步驟：

1. 下載 AnyKernel3 的 Zip 檔。如果你不知道你該下載哪個檔案，請詳細閱讀文檔中的 [KMI](#kmi) 與[安全性修補程式等級](#security-patch-level)。
2. 開啟核心寫入程式提供的 AnyKernel3 Zip 檔案並寫入核心。

如果您先前並未使用過核心寫入應用程式，可以嘗試下面幾個：

1. [Kernel Flasher](https://github.com/capntrips/KernelFlasher/releases)
2. [Franco Kernel Manager](https://play.google.com/store/apps/details?id=com.franco.kernel)
3. [Ex Kernel Manager](https://play.google.com/store/apps/details?id=flar2.exkernelmanager)

P.S. 這種方法在更新 KernelSU 時比較方便，無需電腦即可完成 (注意備份！)。

## 手動修補 boot.img {#patch-boot-image}

對於某些裝置來說，其 boot.img 格式並不是很常見，不屬於 `lz4`，`gz` 和未壓縮；最典型的就是 Pixel，它的 boot.img 格式是 `lz4_legacy` 壓縮，ramdisk 可能是 `gz` 也可能是 `lz4_legacy` 壓縮；此時如果您直接寫入 KernelSU 提供的 boot.img，手機可能無法開機。這時，您可以透過手動修補 boot.img 來完成。

永遠建議使用 `magiskboot` 來修補映像，一般有兩種修補方法：

1. [magiskboot](https://github.com/topjohnwu/Magisk/releases)
2. [magiskboot_build](https://github.com/ookiineko/magiskboot_build/releases/tag/last-ci)

其中，官方的 `magiskboot` 僅能在 Android 上使用，若您想在電腦上完成，可以嘗試第二個選項。

### 準備 {#preparation}

1. 取得您手機的原廠 boot.img，您可以從您的裝置製造商取得，您也可能需要 [payload-dumper-go](https://github.com/ssut/payload-dumper-go)。
2. 下載 KernelSU 提供的與您的裝置 KMI 一致的 AnyKernel3 Zip 檔 (可參閱[使用自訂 Recovery 安裝](#install-with-custom-recovery))。
3. 解壓縮 AnyKernel3 Zip 檔，取得其中的 `Image` 檔，此檔案為具有 KernelSU 的核心。

### 在 Android 上使用 magiskboot {#using-magiskboot-on-Android-devices}

1. 在 Magisk 的 [Release 頁面](https://github.com/topjohnwu/Magisk/releases) 下載最新的 Magisk。
2. 將 `Magisk-*(version).apk` 重新命名為 `Magisk-*.zip` 並解壓縮。
3. 使用 Adb 將 magiskboot 推入至手機：`adb push Magisk-*/lib/arm64-v8a/libmagiskboot.so /data/local/tmp/magiskboot`。
4. 使用 Adb 將原廠 boot.img 和 AnyKernel3 中的 Image 推入至手機。
5. adb shell 進入 /data/local/tmp/ 目錄，然後賦予先前推入的檔案可執行權限 `chmod +x magiskboot`。
6. adb shell 進入 /data/local/tmp/ 目錄，執行 `./magiskboot unpack boot.img` 此時會將 `boot.img` 解除封裝，得到一個名為 `kernel` 的檔案，這個檔案是您的原廠核心。
7. 使用 `Image` 取代 `kernel`: `mv -f Image kernel`
8. 執行 `./magiskboot repack boot.img` 重新封裝映像，此時您會得到一個 `new-boot.img` 檔案，透過 Fastboot 將這個檔案寫入至裝置即可。

### 在 Windows/macOS/Linux PC 上使用 magiskboot {#using-magiskboot-on-PC}

1. 在 [magiskboot_build](https://github.com/ookiineko/magiskboot_build/releases/tag/last-ci) 下載對應的 magiskboot。
2. (僅linux)賦予檔案可執行權限 `chmod +x magiskboot`。
3. 執行 `./magiskboot unpack boot.img` 此時會將 `boot.img` 解除封裝，得到一個名為 `kernel` 的檔案，這個檔案是您的原廠核心。
4. 使用 `Image` 取代 `kernel`: `mv -f Image kernel`
5. 執行 `./magiskboot repack boot.img` 重新封裝映像，此時您會得到一個 `new-boot.img` 檔案，透過 Fastboot 將這個檔案寫入至裝置即可。

## 使用自訂 Recovery 安裝 {#install-with-custom-recovery}

先決條件：您的裝置必須有自訂的 Recovery，例如 TWRP。如果沒有或者只有官方 Recovery，請使用其他方法。

步驟：

1. 在 KernelSU 的 [Release 頁面](https://github.com/tiann/KernelSU/releases) 下載與您手機版本相符的以 AnyKernel3 開頭的 Zip 檔；例如，手機核心版本為 `android12-5.10.66`，那麼您應該下載 `AnyKernel3-android12-5.10.66_yyyy-MM.zip` 這個檔案 (其中 `yyyy` 為年份，`MM` 為月份)。
2. 重新開機手機至 TWRP。
3. 使用 Adb 將 AnyKernel3-*.zip 放置到手機 `/sdcard` 然後在 TWRP 圖形使用者介面選擇並安裝；或者您也可以直接 `adb sideload AnyKernel-*.zip` 安裝。

PS. 這種方法適用於任何狀況下的安裝 (不限於初次安裝或後續更新)，只要您用 TWRP 就可以進行作業。

## GKI的其他替代方法 {#other-methods}

其實所有這些安裝方法的主旨只有一個，那就是**將原廠核心取代為 KernelSU 提供的核心**。只要能實現這個目的，就可以安裝，比如以下是其他可行的方法：

1. 首先安裝 Magisk，透過 Magisk 取得 Root 權限後使用核心寫入程式寫入 KernelSU 的 AnyKernel Zip。
2. 使用某些 PC 上的寫入工具組寫入 KernelSU 提供的核心。

但是，如果不起作用，請嘗試 Magiskboot 方法。

