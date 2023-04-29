# 安裝 {#title}

## 檢查您的裝置是否受支援 {#check-if-supported}

從 [GitHub Releases](https://github.com/tiann/KernelSU/releases) 或 [酷安](https://www.coolapk.com/apk/me.weishu.kernelsu) 下載 KernelSU 管理員應用程式，然後將應用程式安裝至裝置並開啟：

- 如果應用程式顯示「不支援」，則表示您的裝置不支援 KernelSU，您需要自行編譯核心才能繼續使用，，KernelSU 官方也永遠不會為您提供一個可以刷新的 Boot 映像。
- 如果應用程式顯示「未安裝」，那麼 KernelSU 支援您的裝置；可以進行下一步作業。

:::info
對於顯示「不支援」的裝置，這裡有一個[非官方支援裝置清單](unofficially-support-devices.md)，您可以使用這個清單裡的核心自行編譯。
:::

## 備份您的原廠 boot.img {#backup-boot-image}

在進行刷新作業前，您必須預先備份您的原廠 boot.img。如果您在後續刷新作業中出現了任何問題，您都可以透過使用 Fastboot 刷新回到原廠 Boot 以還原系統。

::: warning
刷新作業可能會造成資料遺失，請確保做好這一步再繼續進行下一步作業！！必要時您還可以備份您手機的所有資料。
:::

## 必要知識 {#acknowage}

### ADB 和 Fastboot {#adb-and-fastboot}

預設狀況下，您將會使用 ADB 和 Fastboot 工具，如果您不知道它們，建議使用搜尋引擎先瞭解相關內容。

### KMI

KMI 全稱 Kernel Module Interface，相同 KMI 的核心版本是**相容的** 這也是 GKI 中「標準」的涵義所在；反之，如果 KMI 不同，那麼這些核心之間無法彼此相容，刷新與您裝置 KMI 不同的核心映像可能會導致開機迴圈。

具體來講，對 GKI 的裝置，其核心版本格式應該如下：

```txt
KernelRelease :=
Version.PatchLevel.SubLevel-AndroidRelease-KmiGeneration-suffix
w      .x         .y       -zzz           -k            -something
```

其中，`w.x-zzz-k` 為 KMI 版本。例如，一部裝置核心版本為 `5.10.101-android12-9-g30979850fc20`，那麼它的 KMI 為 `5.10-android12-9`；理論上刷新其他這個 KMI 的核心也能正常開機。

::: tip
請注意，核心版本中的 SubLevel 並非 KMI 的一部分！也就是說 `5.10.101-android12-9-g30979850fc20` 與 `5.10.137-android12-9-g30979850fc20` 的 KMI 相同！
:::

### 核心版本與 Android 版本 {#kernel-version-vs-android-version}

請注意：**核心版本與 Android 版本並不一定相同！**

如果您發現您的核心版本是 `android12-5.10.101`，然而您 Android 系統的版本為 Android 13 或者其他；請不要覺得奇怪，因為 Android 系統的版本與 Linux 核心的版本號碼並非一致；Linux 核心的版本號碼一般與**裝置出廠時隨附的 Android 系統的版本一致**，如果後續 Android 系統更新，核心版本一般不會發生變化。如果您需要刷新，**請以核心版本為準！！**

## 安裝簡介 {#installation-introduction}

KernelSU 的安裝方法有以下幾種，各自適用於不同的場景，請視需要選擇：

1. 使用自訂 Recovery (如 TWRP) 安裝
2. 使用核心刷新應用程式 (例如 Franco Kernel Manager) 安裝
3. 使用 KernelSU 提供的 boot.img 透過 Fastboot 安裝
4. 手動修補 boot.img 並安裝

## 使用自訂 Recovery 安裝 {#install-by-recovery}

先決條件：您的裝置必須有自訂的 Recovery，例如 TWRP；如果沒有或者只有官方 Recovery，請使用其他方法。

步驟：

1. 在 KernelSU 的 [Release 頁面](https://github.com/tiann/KernelSU/releases) 下載與您手機版本相符的以 AnyKernel3 開頭的 Zip 套件；例如，手機核心版本為 `android12-5.10.66`，那麼您應該下載 `AnyKernel3-android12-5.10.66_yyyy-MM.zip` 這個檔案 (其中 `yyyy` 為年份，`MM` 為月份)。
2. 重新開機手機至 TWRP。
3. 使用 Adb 將 AnyKernel3-*.zip 放置到手機 /sdcard 然後在 TWRP 圖形使用者介面選擇並安裝；或者您也可以直接 `adb sideload AnyKernel-*.zip` 安裝。

PS. 這種方法適用於任何狀況下的安裝 (不限於初次安裝或後續更新)，只要您用 TWRP 就可以進行作業。

## 使用核心刷新應用程式安裝 {#install-by-kernel-flasher}

先決條件：您的裝置必須已經 Root。例如您已經安裝了 Magisk 並取得 Root 存取權，或者您已經安裝了舊版本的 KernelSU 需升級到其他版本的 KernelSU；如果您的裝置並未 Root，請嘗試其他方法。

步驟：

1. 下載 AnyKernel3 的 Zip 檔案；請參閱 *使用自訂 Recovery 安裝* 章節的内容。
2. 開啟核心刷新應用程式提供的 AnyKernel3 Zip 檔案進行刷新。

如果您先前並未使用過核心刷新應用程式，可以嘗試下面幾個方法：

1. [Kernel Flasher](https://github.com/capntrips/KernelFlasher/releases)
2. [Franco Kernel Manager](https://play.google.com/store/apps/details?id=com.franco.kernel)
3. [Ex Kernel Manager](https://play.google.com/store/apps/details?id=flar2.exkernelmanager)

PS. 這種方法在更新 KernelSU 時比較方便，無需電腦即可完成 (注意備份！)。

## 使用 KernelSU 提供的 boot.img 安裝 {#install-by-kernelsu-boot-image}

這種方法無需您有 TWRP，也無需您的手機有 Root 權限；適用於您初次安裝 KernelSU。

### 找到合適的 boot.img {#found-propery-image}

KernelSU 為 GKI 裝置提供了標準 boot.img，您需要將 boot.img 刷新至裝置的 Boot 分割區。

您可以從 [GitHub Release](https://github.com/tiann/KernelSU/releases) 下載 boot.img，請注意，您應該使用正確版本的 boot.img。例如，如果您的裝置顯示核心是 `android12-5.10.101`，需要下載 `android-5.10.101_yyyy-MM.boot-<format>.img`.

其中 `<format>` 指的是您的官方 boot.img 的核心壓縮格式，請檢查您原有 boot.img 的核心壓縮格式，您應該使用正確的格式，例如 `lz4`、`gz`；如果使用不正確的壓縮格式，刷新 Boot 後可能無法開機。

::: info
1. 您可以透過 magiskboot 以取得您的原始 Boot 的壓縮格式；當然，您也可以詢問與您相同型號的其他更有經驗的使用者。另外，核心的壓縮格式通常部會出現變更，如果您使用的某個壓縮格式成功開機，後續可以優先嘗試這個格式。
2. 小米裝置通常 `gz` 或者 **不壓縮**。
3. Pixel 裝置有些特殊，請遵循下方的指示。
:::

### 將 boot.img 刷新至裝置 {#flash-boot-image}

使用 `adb` 連接您的裝置，然後執行 `adb reboot bootloader` 進入 fastboot 模式，然後使用此命令刷新 KernelSU：

```sh
fastboot flash boot boot.img
```

::: info
如果您的裝置支援 `fastboot boot`，可以先使用 `fastboot boot boot.img` 來先嘗試使用 boot.img 開機進入系統，如果出現意外，重新啟動即可開機。
:::

### 重新開機 {#reboot}

刷新完成後，您應該重新啟動您的裝置：

```sh
fastboot reboot
```

## 手動修補 boot.img {#patch-boot-image}

對於某些裝置來說，其 boot.img 格式並不是很常見，比如 `lz4`，`gz` 和未壓縮；最典型的就是 Pixel，它的 boot.img 格式是 `lz4_legacy` 壓縮，ramdisk 可能是 `gz` 也可能是 `lz4_legacy` 壓縮；此時如果您直接刷新 KernelSU 提供的 boot.img，手機可能無法開機；這時，您可以透過手動修補 boot.img 來完成。

一般有兩種修補方法：

1. [Android-Image-Kitchen](https://forum.xda-developers.com/t/tool-android-image-kitchen-unpack-repack-kernel-ramdisk-win-android-linux-mac.2073775/)
2. [magiskboot](https://github.com/topjohnwu/Magisk/releases)

其中，Android-Image-Kitchen 適用於在電腦上作業，magiskboot 需要手機協作。

### 準備 {#patch-preparation}

1. 取得您手機的原廠 boot.img；您可以聯絡您的裝置製造商，您也可能需要[payload-dumper-go](https://github.com/ssut/payload-dumper-go)
2. 下載 KernelSU 提供的與您的裝置 KMI 一致地 AnyKernel3 Zip 檔案 (可參閱 *使用自訂 Recovery 安裝*)。
3. 解壓縮 AnyKernel3 Zip 檔案，取得其中的 `Image` 檔案，此檔案為 KernelSU 的核心檔案。

### 使用 Android-Image-Kitchen {#using-android-image-kitchen}

1. 下載 Android-Image-Kitchen 至您的電腦。
2. 將手機原廠 boot.img 放置於 Android-Image-Kitchen 根目錄。
3. 在 Android-Image-Kitchen 根目錄執行 `./unpackimg.sh boot.img`；此命令會將 boot.img 解除封裝，您會得到一些檔案。
4. 將 `split_img` 目錄中的 `boot.img-kernel` 取代為您從 AnyKernel3 解壓縮出來的 `Image` (注意名稱變更為 boot.img-kernel)。
5. 在 Android-Image-Kitchecn 根目錄執行 `./repackimg.sh`；此時您會得到一個 `image-new.img` 檔案；使用此 boot.img 透過 fastboot 刷新即可 (刷新方法請參閱上一章節)。 

### 使用 magiskboot {#using magiskboot}

1. 在 Magisk 的 [Release 頁面](https://github.com/topjohnwu/Magisk/releases) 下載最新的 Magisk 安裝套件。
2. 將 Magisk-*.apk 重新命名為 Magisk-vesion.zip 然後解壓縮。
3. 將解壓縮後的 `Magisk-v25.2/lib/arm64-v8a/libmagiskboot.so` 檔案，使用 Adb 推入至手機：`adb push Magisk-v25.2/lib/arm64-v8a/libmagiskboot.so /data/local/tmp/magiskboot`
4. 使用 Adb 將原廠 boot.img 和 AnyKernel3 中的 Image 推入至手機。
5. adb shell 進入 /data/local/tmp/ 目錄，然後賦予先前推入檔案的可執行權限 `chmod +x magiskboot`
6. adb shell 進入 /data/local/tmp/ 目錄，執行 `./magiskboot unpack boot.img` 此時會將 `boot.img` 解除封裝，得到一個名為 `kernel` 的檔案，這個檔案是您的原廠核心。
7. 使用 `Image` 取代 `kernel`: `mv -f Image kernel`
8. 執行 `./magiskboot repack boot.img` 重新封裝 img，此時您會得到一個 `new-boot.img` 檔案，透過 Fastboot 將這個檔案刷新至裝置即可。

## 其他替代方法 {#other-methods}

其實所有這些安裝方法的主旨只有一個，那就是**將原廠核心取代為 KernelSU 提供的核心**；只要能實現這個目的，就可以安裝；比如以下是其他可行的方法：

1. 首先安裝 Magisk，透過 Magisk 取得 Root 權限後使用核心刷新程式刷新 KernelSU 的 AnyKernel Zip。
2. 使用某些 PC 上的刷新工具組刷新 KernelSU 提供的核心。
