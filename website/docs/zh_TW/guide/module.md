# 模組指南 {#introduction}

KernelSU 提供了一個模組機制，它可以在保持系統分割區完整性的同時達到修改系統分割區的效果；這種機制一般被稱為 systemless。

KernelSU 的模組運作機制與 Magisk 幾乎相同，如果您熟悉 Magisk 模組的開發，那麼開發 KernelSU 的模組大同小異，您可以跳過下列有關模組的介紹，只需要瞭解 [KernelSU 模組與 Magisk 模組的異同](difference-with-magisk.md)。

## WebUI

KernelSU 的模組支援顯示互動介面，請參閱 [WebUI 文檔](module-webui.md).

## Busybox

KernelSU 提供了一個完備的 BusyBox 二進位檔案 (包括完整的 SELinux 支援)。可執行檔位於 `/data/adb/ksu/bin/busybox`。
KernelSU 的 BusyBox 支援同時執行時可切換的 "ASH Standalone Shell Mode"。
這種讀了模式意味著在執行 BusyBox 的 ash shell 時，每個命令都會直接使用 BusyBox 中內建的應用程式，而不論 PATH 的設定為何。
例如，`ls`、`rm`、`chmod` 等命令將不會使用 PATH 中設定的命令 (在 Android 的狀況下，預設狀況下分別為 `/system/bin/ls`、`/system/bin/rm` 和 `/system/bin/chmod`)，而是直接呼叫 BusyBox 內建的應用程式。
這確保了腳本始終在可預測的環境中執行，並始終具有完整的命令套件，不論它執行在哪個 Android 版本上。
要強制下一個命令不使用 BusyBox，您必須使用完整路徑呼叫可執行檔。

在 KernelSU 上下文中執行的每個 shell 腳本都將在 BusyBox 的 ash shell 中以獨立模式執行。對於第三方開發人員相關的內容，包括所有開機腳本和模組安裝腳本。

對於想要在 KernelSU 之外使用這個「獨立模式」功能的使用者，有兩種啟用方法：

1. 將環境變數 `ASH_STANDALONE` 設為 `1`。例如：`ASH_STANDALONE=1 /data/adb/ksu/bin/busybox sh <script>`
2. 使用命令列選項切換：`/data/adb/ksu/bin/busybox sh -o standalone <script>`

為了確保所有後續的 `sh` shell 都在獨立模式下執行，第一種是首選方法 (這也是 KernelSU 和 KernelSU 管理員內部使用的方法)，因為環境變數會被繼承到子處理程序中。

::: tip 與 Magisk 的差異
KernelSU 的 BusyBox 現在是直接使用 Magisk 專案編譯的二進位檔案，**感謝 Magisk！**
因此，您完全不必擔心 BusyBox 腳本與在 Magisk 和 KernelSU 之間的相容性問題，因為它們完全相同！
:::

## KernelSU 模組 {#kernelsu-modules}

KernelSU 模組是一個放置於 `/data/adb/modules` 且滿足下列結構的資料夾：

```txt
/data/adb/modules
├── .
├── .
|
├── $MODID                  <--- 模組的資料夾名稱與模組 ID 相同
│   │
│   │      *** 模組識別 ***
│   │
│   ├── module.prop         <--- 這個檔案儲存與模組相關的中繼資料，例如模組 ID、版本等
│   │
│   │      *** 主要內容 ***
│   │
│   ├── system              <--- 這個資料夾會在 skip_mount 不存在時被掛接至系統
│   │   ├── ...
│   │   ├── ...
│   │   └── ...
│   │
│   │      *** 狀態旗標 ***
│   │
│   ├── skip_mount          <--- 如果這個檔案存在，那麼 KernelSU 將不會掛接您的系統資料夾
│   ├── disable             <--- 如果這個檔案存在，那麼模組將會被停用
│   ├── remove              <--- 如果這個檔案存在，那麼模組將會在下次重新開機時被移除
│   │
│   │      *** 選用檔案 ***
│   │
│   ├── post-fs-data.sh     <--- 這個腳本將會在 post-fs-data 中執行
│   ├── service.sh          <--- 這個腳本將會在 late_start 服務中執行
|   ├── uninstall.sh        <--- 這個腳本將會在 KernelSU 移除模組時執行
│   ├── system.prop         <--- 這個檔案中指定的屬性將會在系統啟動時透過 resetprop 變更
│   ├── sepolicy.rule       <--- 這個檔案中的 SELinux 原則將會在系統開機時載入
│   │
│   │      *** 自動產生的目錄，不要手動建立或修改！ ***
│   │
│   ├── vendor              <--- A symlink to $MODID/system/vendor
│   ├── product             <--- A symlink to $MODID/system/product
│   ├── system_ext          <--- A symlink to $MODID/system/system_ext
│   │
│   │      *** 允許的其他額外檔案/資料夾 ***
│   │
│   ├── ...
│   └── ...
|
├── another_module
│   ├── .
│   └── .
├── .
├── .
```

::: tip 與 Magisk 的差異
KernelSU 沒有內建的針對 Zygisk 的支援，因此模組中沒有與 Zygisk 相關的內容，但您可以透過 [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext) 以支援 Zygisk 模組，此時 Zygisk 模組的內容與 Magisk 所支援的 Zygisk 完全相同。
:::

### module.prop

module.prop 是一個模組的組態檔案，在 KernelSU 中如果模組中不包含這個檔案，那麼它將不被認為是一個模組；這個檔案的格式如下：

```txt
id=<string>
name=<string>
version=<string>
versionCode=<int>
author=<string>
description=<string>
```

- id 必須與這個規則運算式相符：`^[a-zA-Z][a-zA-Z0-9._-]+$` 例如：✓ `a_module`，✓ `a.module`，✓ `module-101`，✗ `a  module`，✗ `1_module`，✗ `-a-module`。這是您的模組的唯一識別碼，發表後將無法變更。
- versionCode 必須是一個整數，用於比較版本。
- 其他未在上方提到的內容可以是任何單行字串。
- 請確保使用 `UNIX (LF)` 分行符號類型，而非 `Windows (CR + LF)` 或 `Macintosh (CR)`。

### Shell 腳本 {#shell-scripts}

請閱讀 [開機腳本](#boot-scripts) 章節，以瞭解 `post-fs-data.sh` 和 `service.sh` 之間的差別。對於大多數模組開發人員來說，如果您只需要執行一個開機腳本，`service.sh` 應該已經足夠了。

在您的模組中的所有腳本中，請使用 `MODDIR=${0%/*}` 以取得您的模組基本目錄路徑；請不要在腳本中以硬式編碼的方式加入您的模組路徑。

:::tip 與 Magisk 的差異
您可以透過環境變數 `KSU` 來判斷腳本是執行在 KernelSU 還是 Magisk 中，如果執行在 KernelSU，這個值會被設為 `true`。
:::

### `system` 目錄 {#system-directories}

這個目錄的內容會在系統啟動後，以 `overlayfs` 的方式覆疊在系統的 `/system` 分割區之上，這表示：

1. 系統中對應目錄的相同名稱的檔案會被此目錄中的檔案覆寫。
2. 系統中對應目錄的相同名稱的檔案會與此目錄的檔案合併。

如果您想要刪除系統先前的目錄中的某個檔案或資料夾，您需要在模組目錄中透過 `mknod filename c 0 0` 以建立一個 `filename` 的相同名稱的檔案；這樣 overlayfs 系統會自動「whiteout」等效刪除這個檔案 (`/system` 分割區並未被變更)。

您也可以在 `customize.sh` 中宣告一個名為 `REMOVE` 並且包含一系列目錄的變數以執行移除作業，KernelSU 會自動為您在模組對應目錄執行 `mknod <TARGET> c 0 0`。例如：

```sh
REMOVE="
/system/app/YouTube
/system/app/Bloatware
"
```

上方的清單將會執行：`mknod $MODPATH/system/app/YouTuBe c 0 0` 和 `mknod $MODPATH/system/app/Bloatware c 0 0`；並且 `/system/app/YouTube` 和 `/system/app/Bloatware` 將會在模組生效前移除。

如果您想要取代系統的某個目錄，您需要在模組目錄中建立一個相同路徑的目錄，然後為此目錄設定此屬性：`setfattr -n trusted.overlay.opaque -v y <TARGET>`；這樣 overlayfs 系統會自動將對應目錄取代 (`/system` 分割區並未被變更)。

您可以在 `customize.sh` 中宣告一個名為 `REMOVE` 並且包含一系列目錄的變數以執行移除作業，KernelSU 會自動為您在模組對應目錄執行相關作業。例如：

```sh
REPLACE="
/system/app/YouTube
/system/app/Bloatware
"
```

上方的清單將會執行：自動建立目錄 `$MODPATH/system/app/YouTube` 和 `$MODPATH//system/app/Bloatware`，然後執行 `setfattr -n trusted.overlay.opaque -v y $$MODPATH/system/app/YouTube` 和 `setfattr -n trusted.overlay.opaque -v y $$MODPATH/system/app/Bloatware`；並且 `/system/app/YouTube` 和 `/system/app/Bloatware` 將會在模組生效後被取代為空白目錄。

::: tip 與 Magisk 的差異

KernelSU 的 systemless 機制透過核心的 overlayfs 實作，而 Magisk 目前則是透過 magic mount (bind mount)，兩者的實作方式有很大的差別，但最終的目標是一致的：不修改實際的 `/system` 分割區但修改 `/system` 檔案。
:::

如果您對 overlayfs 感興趣，建議閱讀 Linux Kernel 關於 [overlayfs 的文件](https://docs.kernel.org/filesystems/overlayfs.html)

### system.prop

這個檔案的格式與 `build.prop` 完全相同：每一行都是由 `[key]=[value]` 組成。

### sepolicy.rule

如果您的模組需要一些額外 SELinux 原則修補程式，請將這些原則新增至這個檔案中。這個檔案的每一行都將被視為一個原則陳述。

## 模組安裝程式 {#module-installer}

KernelSU 的模組安裝程式就是一個可以透過 KernelSU 管理員應用程式刷新的 Zip 檔案，這個 Zip 檔案的格式如下：

```txt
module.zip
│
├── customize.sh                       <--- (Optional, more details later)
│                                           This script will be sourced by update-binary
├── ...
├── ...  /* 其他模块文件 */
│
```

:::warning
KernelSU 模組不支援在 Recovery 中安裝！！
:::

### 自訂安裝程序 {#customizing-installation}

如果您想要控制模組的安裝程序，可以在模組的目錄下建立一個名為 `customize.sh` 的檔案，這個檔案將會在模組被解壓縮後**匯入**至目前的 shell 中，如果您的模組需要依據裝置的 API 版本或裝置架構執行一些額外的作業，這個腳本將非常有用。

如果您想完全控制腳本的安裝程序，您可以在 `customize.sh` 中宣告 `SKIPUNZIP=1` 以跳過所有的預設安裝步驟；此時，您需要自行處理所有的安裝程序 (例如解壓縮模組、設定權限等)

`customize.sh` 腳本以「獨立模式」執行在 KernelSU 的 BusyBox `ash` shell 中。您可以使用下列變數和函式：

#### 變數 {#variables}

- `KSU` (bool): 標示此腳本執行於 KernelSU 環境中，此變數的值將永遠為 `true`，您可以透過它與 Magisk 進行區分。
- `KSU_VER` (string): KernelSU 目前的版本名稱 (例如 `v0.4.0`)
- `KSU_VER_CODE` (int): KernelSU 使用者空間目前的版本代碼 (例如 `10672`)
- `KSU_KERNEL_VER_CODE` (int): KernelSU 核心空間目前的版本代碼 (例如 `10672`)
- `BOOTMODE` (bool): 此變數在 KernelSU 中永遠為 `true`
- `MODPATH` (path): 目前模組的安裝目錄
- `TMPDIR` (path): 可以存放暫存檔的位置
- `ZIPFILE` (path): 目前模組的安裝程式 Zip
- `ARCH` (string): 裝置的 CPU 架構，有這幾種：`arm`, `arm64`, `x86`, or `x64`
- `IS64BIT` (bool): 是否為 64 位元裝置
- `API` (int): 目前裝置的 Android API 版本 (例如 Android 6.0 上為 `23`)

::: warning
`MAGISK_VER_CODE` 在 KernelSU 永遠為 `25200`，`MAGISK_VER` 則為 `v25.2`，請不要透過這兩個變數來判斷是否為 KernelSU！
:::

#### 函式 {#functions}

```txt
ui_print <msg>
    print <msg> to console
    Avoid using 'echo' as it will not display in custom recovery's console

abort <msg>
    print error message <msg> to console and terminate the installation
    Avoid using 'exit' as it will skip the termination cleanup steps

set_perm <target> <owner> <group> <permission> [context]
    if [context] is not set, the default is "u:object_r:system_file:s0"
    this function is a shorthand for the following commands:
       chown owner.group target
       chmod permission target
       chcon context target

set_perm_recursive <directory> <owner> <group> <dirpermission> <filepermission> [context]
    if [context] is not set, the default is "u:object_r:system_file:s0"
    for all files in <directory>, it will call:
       set_perm file owner group filepermission context
    for all directories in <directory> (including itself), it will call:
       set_perm dir owner group dirpermission context
```

## 開機腳本 {#boot-scripts}

在 KernelSU 中，依據腳本執行模式的不同分為兩種：post-fs-data 模式和 late_start 服務模式。

- post-fs-data 模式
  - 這個階段是 **阻塞** 的。在執行完成之前或 10 秒鐘之後，開機程序會被暫停。
  - 腳本在任何模組被掛接之前執行。這使模組開發人員可以在模組被掛接之前動態調整他們的模組。
  - 這個階段發生在 Zygote 啟動之前，這意味著 Android 中的一切。
  - 使用 `setprop` 會導致開機程序死鎖！請使用 `resetprop -n <prop_name> <prop_value>` 替代。
  - **僅在必要時在此模式中執行腳本**。

- late_start 服務模式
  - 這個階段是 **非阻塞** 的。您的腳本會與其餘的啟動程序**平行**執行。
  - **大多數腳本建議在這種模式下執行**。

在 KernelSU 中，開機腳本依據存放位置的不同還分為兩種：一般腳本和模組腳本。

- 一般腳本
  - 放置於 `/data/adb/post-fs-data.d` 或 `/data/adb/service.d` 中。
  - 僅有腳本被設為可執行 (`chmod +x script.sh`) 時才會被執行。
  - 在 `post-fs-data.d` 中的腳本以 post-fs-data 模式執行，在 `service.d` 中的腳本以 late_start 服務模式執行。
  - 模組**不應**在安裝程序中新增一般腳本。

- 模組腳本
  - 放置於模組自己的資料夾中。
  - 僅有在模組啟用時才會執行。
  - `post-fs-data.sh` 以 post-fs-data 模式運行，`post-mount.sh` 以 post-mount 模式運行，而`service.sh` 則以 late_start 服務模式運行，`boot-completed`在 Android 系統啟動完畢後以服務模式運作。

所有啟動腳本都將在 KernelSU 的 BusyBox ash shell 中執行，並啟用**獨立模式**。