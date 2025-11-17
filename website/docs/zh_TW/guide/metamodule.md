# 元模組

元模組是 KernelSU 的一項革命性功能,它將關鍵的模組系統能力從核心守護程序轉移到可插拔模組中。這種架構轉變在保持 KernelSU 穩定性和安全性的同時,為模組生態系統釋放了更大的創新潛力。

## 什麼是元模組?

元模組是一種特殊類型的 KernelSU 模組,為模組系統提供核心基礎設施功能。與修改系統檔案的常規模組不同,元模組控制常規模組的*安裝和掛載方式*。

元模組是一種基於外掛的擴充機制,允許完全自訂 KernelSU 的模組管理基礎設施。透過將掛載和安裝邏輯委託給元模組,KernelSU 避免成為脆弱的檢測點,同時支援多樣化的實作策略。

**主要特徵:**

- **基礎設施角色**: 元模組提供常規模組依賴的服務
- **單實例**: 一次只能安裝一個元模組
- **優先執行**: 元模組腳本在常規模組腳本之前執行
- **特殊鉤子**: 提供三個用於安裝、掛載和清理的鉤子腳本

## 為什麼需要元模組?

傳統的 Root 解決方案將掛載邏輯內建在核心中,這使得它們更容易被檢測且難以演進。KernelSU 的元模組架構透過關注點分離解決了這些問題。

**策略優勢:**

- **減少檢測面**: KernelSU 本身不執行掛載,減少了檢測向量
- **穩定性**: 核心守護程序保持穩定,而掛載實作可以不斷演進
- **創新性**: 社群可以開發替代掛載策略,而無需分叉 KernelSU
- **選擇性**: 使用者可以選擇最適合其需求的實作

**掛載靈活性:**

- **無掛載**: 對於僅使用無掛載模組的使用者,完全避免掛載開銷
- **OverlayFS 掛載**: 傳統方法,支援讀寫層(透過 `meta-overlayfs`)
- **魔術掛載**: Magisk 相容掛載,以獲得更好的應用程式相容性
- **自訂實作**: 基於 FUSE 的覆蓋層、自訂 VFS 掛載或全新方法

**超越掛載:**

- **可擴充性**: 新增核心模組支援等功能,無需修改核心 KernelSU
- **模組化**: 獨立於 KernelSU 版本更新實作
- **客製化**: 為特定裝置或用例建立專門的解決方案

::: warning 重要
如果沒有安裝元模組,模組將**不會**被掛載。新安裝的 KernelSU 需要安裝元模組(如 `meta-overlayfs`)才能使模組正常運作。
:::

## 對於使用者

### 安裝元模組

像安裝常規模組一樣安裝元模組:

1. 下載元模組 ZIP 檔案(例如 `meta-overlayfs.zip`)
2. 開啟 KernelSU Manager 應用程式
3. 點擊浮動操作按鈕(➕)
4. 選擇元模組 ZIP 檔案
5. 重新啟動裝置

`meta-overlayfs` 元模組是官方參考實作,提供傳統的基於 overlayfs 的模組掛載,支援 ext4 映像。

### 檢查活動的元模組

您可以在 KernelSU Manager 應用程式的模組頁面中檢視目前活動的元模組。活動的元模組將顯示在模組清單中,並帶有特殊標識。

### 解除安裝元模組

::: danger 警告
解除安裝元模組會影響**所有**模組。移除後,模組將不再被掛載,直到您安裝另一個元模組。
:::

解除安裝步驟:

1. 開啟 KernelSU Manager
2. 在模組清單中找到元模組
3. 點擊解除安裝(您會看到特殊警告)
4. 確認操作
5. 重新啟動裝置

解除安裝後,如果您希望模組繼續運作,應該安裝另一個元模組。

### 單元模組約束

一次只能安裝一個元模組。如果您嘗試安裝第二個元模組,KernelSU 將阻止安裝以避免衝突。

切換元模組的步驟:

1. 解除安裝所有常規模組
2. 解除安裝目前元模組
3. 重新啟動
4. 安裝新元模組
5. 重新安裝常規模組
6. 再次重新啟動

## 對於模組開發者

如果您正在開發常規 KernelSU 模組,您不需要太擔心元模組。只要使用者安裝了相容的元模組(如 `meta-overlayfs`),您的模組就能正常運作。

**您需要知道的:**

- **掛載需要元模組**: 模組中的 `system` 目錄只有在使用者安裝了提供掛載功能的元模組時才會被掛載
- **無需變更程式碼**: 現有模組無需修改即可繼續運作

::: tip
如果您熟悉 Magisk 模組開發,您的模組在安裝元模組後將在 KernelSU 中以相同方式運作,因為它提供了 Magisk 相容的掛載。
:::

## 對於元模組開發者

建立元模組允許您自訂 KernelSU 處理模組安裝、掛載和解除安裝的方式。

### 基本要求

元模組透過 `module.prop` 中的特殊屬性來識別:

```txt
id=my_metamodule
name=My Custom Metamodule
version=1.0
versionCode=1
author=Your Name
description=Custom module mounting implementation
metamodule=1
```

`metamodule=1`(或 `metamodule=true`)屬性將此模組標記為元模組。沒有此屬性,模組將被視為常規模組。

### 檔案結構

元模組結構:

```txt
my_metamodule/
├── module.prop              (必須包含 metamodule=1)
│
│      *** 元模組特定鉤子 ***
├── metamount.sh             (選用: 自訂掛載處理程式)
├── metainstall.sh           (選用: 常規模組的安裝鉤子)
├── metauninstall.sh         (選用: 常規模組的清理鉤子)
│
│      *** 標準模組檔案(全部選用) ***
├── customize.sh             (安裝自訂)
├── post-fs-data.sh          (post-fs-data 階段腳本)
├── service.sh               (late_start service 腳本)
├── boot-completed.sh        (啟動完成腳本)
├── uninstall.sh             (元模組自己的解除安裝腳本)
├── system/                  (無系統修改,如果需要)
└── [任何其他檔案]
```

除了特殊的元模組鉤子外,元模組可以使用所有標準模組功能(生命週期腳本等)。

### 鉤子腳本

元模組可以提供最多三個特殊鉤子腳本:

#### 1. metamount.sh - 掛載處理程式

**目的**: 控制啟動期間模組的掛載方式。

**執行時機**: 在 `post-fs-data` 階段,在任何模組腳本執行之前。

**環境變數:**

- `MODDIR`: 元模組的目錄路徑(例如 `/data/adb/modules/my_metamodule`)
- 所有標準 KernelSU 環境變數

**職責:**

- 以無系統方式掛載所有已啟用的模組
- 檢查 `skip_mount` 標誌
- 處理特定模組的掛載要求

::: danger 關鍵要求
執行掛載操作時,**必須**將來源/裝置名稱設定為 `"KSU"`。這將掛載標識為屬於 KernelSU。

**範例(正確):**

```sh
mount -t overlay -o lowerdir=/lower,upperdir=/upper,workdir=/work KSU /target
```

**對於現代掛載 API**,設定來源字串:

```rust
fsconfig_set_string(fs, "source", "KSU")?;
```

這對於 KernelSU 正確識別和管理其掛載至關重要。
:::

**範例腳本:**

```sh
#!/system/bin/sh
MODDIR="${0%/*}"

# 範例: 簡單的繫結掛載實作
for module in /data/adb/modules/*; do
    if [ -f "$module/disable" ] || [ -f "$module/skip_mount" ]; then
        continue
    fi

    if [ -d "$module/system" ]; then
        # 使用 source=KSU 掛載(必需!)
        mount -o bind,dev=KSU "$module/system" /system
    fi
done
```

#### 2. metainstall.sh - 安裝鉤子

**目的**: 自訂常規模組的安裝方式。

**執行時機**: 在模組安裝期間,檔案提取後但安裝完成前。此腳本被內建安裝程式**引用**(而非執行),類似於 `customize.sh` 的運作方式。

**環境變數和函式:**

此腳本繼承內建 `install.sh` 的所有變數和函式:

- **變數**: `MODPATH`、`TMPDIR`、`ZIPFILE`、`ARCH`、`API`、`IS64BIT`、`KSU`、`KSU_VER`、`KSU_VER_CODE`、`BOOTMODE` 等
- **函式**:
  - `ui_print <msg>` - 向主控台列印訊息
  - `abort <msg>` - 列印錯誤並終止安裝
  - `set_perm <target> <owner> <group> <permission> [context]` - 設定檔案權限
  - `set_perm_recursive <directory> <owner> <group> <dirpermission> <filepermission> [context]` - 遞迴設定權限
  - `install_module` - 呼叫內建模組安裝程序

**用例:**

- 在內建安裝之前或之後處理模組檔案(準備好後呼叫 `install_module`)
- 移動模組檔案
- 驗證模組相容性
- 設定特殊目錄結構
- 初始化模組特定資源

**注意**: 安裝元模組本身時**不會**呼叫此腳本。

#### 3. metauninstall.sh - 清理鉤子

**目的**: 解除安裝常規模組時清理資源。

**執行時機**: 在模組解除安裝期間,在刪除模組目錄之前。

**環境變數:**

- `MODULE_ID`: 正在解除安裝的模組的 ID

**用例:**

- 處理檔案
- 清理符號連結
- 釋放配置的資源
- 更新內部追蹤

**範例腳本:**

```sh
#!/system/bin/sh
# 解除安裝常規模組時呼叫
MODULE_ID="$1"
IMG_MNT="/data/adb/metamodule/mnt"

# 從映像中刪除模組檔案
if [ -d "$IMG_MNT/$MODULE_ID" ]; then
    rm -rf "$IMG_MNT/$MODULE_ID"
fi
```

### 執行順序

了解啟動執行順序對於元模組開發至關重要:

```txt
post-fs-data 階段:
  1. 執行通用 post-fs-data.d 腳本
  2. 修剪模組,restorecon,載入 sepolicy.rule
  3. 執行元模組的 post-fs-data.sh(如果存在)
  4. 執行常規模組的 post-fs-data.sh
  5. 載入 system.prop
  6. 執行元模組的 metamount.sh
     └─> 以無系統方式掛載所有模組
  7. post-mount.d 階段執行
     - 通用 post-mount.d 腳本
     - 元模組的 post-mount.sh(如果存在)
     - 常規模組的 post-mount.sh

service 階段:
  1. 執行通用 service.d 腳本
  2. 執行元模組的 service.sh(如果存在)
  3. 執行常規模組的 service.sh

boot-completed 階段:
  1. 執行通用 boot-completed.d 腳本
  2. 執行元模組的 boot-completed.sh(如果存在)
  3. 執行常規模組的 boot-completed.sh
```

**要點:**

- `metamount.sh` 在所有 post-fs-data 腳本(元模組和常規模組)**之後**執行
- 元模組生命週期腳本(`post-fs-data.sh`、`service.sh`、`boot-completed.sh`)始終在常規模組腳本之前執行
- `.d` 目錄中的通用腳本在元模組腳本之前執行
- `post-mount` 階段在掛載完成後執行

### 符號連結機制

當安裝元模組時,KernelSU 會建立一個符號連結:

```sh
/data/adb/metamodule -> /data/adb/modules/<metamodule_id>
```

這為存取活動元模組提供了穩定的路徑,無論其 ID 如何。

**好處:**

- 一致的存取路徑
- 輕鬆偵測活動元模組
- 簡化設定

### 真實範例: meta-overlayfs

`meta-overlayfs` 元模組是官方參考實作。它展示了元模組開發的最佳實踐。

#### 架構

`meta-overlayfs` 使用**雙目錄架構**:

1. **中繼資料目錄**: `/data/adb/modules/`
   - 包含 `module.prop`、`disable`、`skip_mount` 標記
   - 啟動期間快速掃描
   - 儲存佔用小

2. **內容目錄**: `/data/adb/metamodule/mnt/`
   - 包含實際模組檔案(system、vendor、product 等)
   - 儲存在 ext4 映像(`modules.img`)中
   - 使用 ext4 功能最佳化空間

#### metamount.sh 實作

以下是 `meta-overlayfs` 如何實作掛載處理程式:

```sh
#!/system/bin/sh
MODDIR="${0%/*}"
IMG_FILE="$MODDIR/modules.img"
MNT_DIR="$MODDIR/mnt"

# 如果尚未掛載,則掛載 ext4 映像
if ! mountpoint -q "$MNT_DIR"; then
    mkdir -p "$MNT_DIR"
    mount -t ext4 -o loop,rw,noatime "$IMG_FILE" "$MNT_DIR"
fi

# 為雙目錄支援設定環境變數
export MODULE_METADATA_DIR="/data/adb/modules"
export MODULE_CONTENT_DIR="$MNT_DIR"

# 執行掛載二進位檔案
# (實際掛載邏輯在 Rust 二進位檔案中)
"$MODDIR/meta-overlayfs"
```

#### 主要特性

**Overlayfs 掛載:**

- 使用核心 overlayfs 進行真正的無系統修改
- 支援多個分割區(system、vendor、product、system_ext、odm、oem)
- 透過 `/data/adb/modules/.rw/` 支援讀寫層

**來源識別:**

```rust
// 來自 meta-overlayfs/src/mount.rs
fsconfig_set_string(fs, "source", "KSU")?;  // 必需!
```

這為所有 overlay 掛載設定 `dev=KSU`,實現正確識別。

### 最佳實踐

開發元模組時:

1. **始終將來源設定為"KSU"**以進行掛載操作 - 核心卸載和 zygisksu 卸載需要此設定才能正確卸載
2. **優雅地處理錯誤** - 啟動程序對時間敏感
3. **尊重標準標誌** - 支援 `skip_mount` 和 `disable`
4. **記錄操作** - 使用 `echo` 或日誌記錄進行除錯
5. **徹底測試** - 掛載錯誤可能導致啟動迴圈
6. **記錄行為** - 清楚地解釋您的元模組做什麼
7. **提供遷移路徑** - 協助使用者從其他解決方案切換

### 測試您的元模組

發布前:

1. 在乾淨的 KernelSU 設定上**測試安裝**
2. **驗證掛載**各種模組類型
3. **檢查相容性**與常見模組
4. **測試解除安裝**和清理
5. **驗證啟動效能**(metamount.sh 是阻塞的!)
6. **確保正確的錯誤處理**以避免啟動迴圈

## 常見問題

### 我需要元模組嗎?

**對於使用者**: 僅當您想使用需要掛載的模組時。如果您只使用執行腳本而不修改系統檔案的模組,則不需要元模組。

**對於模組開發者**: 不需要,您正常開發模組。僅當您的模組需要掛載時,使用者才需要元模組。

**對於進階使用者**: 僅當您想自訂掛載行為或建立替代掛載實作時。

### 我可以有多個元模組嗎?

不可以。一次只能安裝一個元模組。這可以防止衝突並確保可預測的行為。

### 如果我解除安裝了唯一的元模組會怎樣?

模組將不再被掛載。您的裝置將正常啟動,但模組修改將不會套用,直到您安裝另一個元模組。

### meta-overlayfs 是必需的嗎?

不是。它提供與大多數模組相容的標準 overlayfs 掛載。如果您需要不同的行為,可以建立自己的元模組。

## 另請參閱

- [模組指南](module.md) - 通用模組開發
- [與 Magisk 的差異](difference-with-magisk.md) - 比較 KernelSU 和 Magisk
- [如何建置](how-to-build.md) - 從原始碼建置 KernelSU
