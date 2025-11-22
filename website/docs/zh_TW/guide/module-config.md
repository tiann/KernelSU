# 模組配置

KernelSU 提供了一個內建的配置系統,允許模組儲存持久化或暫時的鍵值設定。配置以二進位格式儲存在 `/data/adb/ksu/module_configs/<module_id>/`,具有以下特性:

## 配置類型

- **持久配置** (`persist.config`):重新開機後保留,直到明確刪除或解除安裝模組
- **暫時配置** (`tmp.config`):在每次啟動時的 post-fs-data 階段自動清除

讀取配置時,對於同一個鍵,暫時值優先於持久值。

## 在模組腳本中使用配置

所有模組腳本(`post-fs-data.sh`、`service.sh`、`boot-completed.sh` 等)執行時都會設定 `KSU_MODULE` 環境變數為模組 ID。您可以使用 `ksud module config` 命令來管理模組的配置:

```bash
# 獲取配置值
value=$(ksud module config get my_setting)

# 設定持久配置值
ksud module config set my_setting "some value"

# 設定暫時配置值(重新開機後清除)
ksud module config set --temp runtime_state "active"

# 列出所有配置項(合併持久和暫時配置)
ksud module config list

# 刪除配置項
ksud module config delete my_setting

# 刪除暫時配置項
ksud module config delete --temp runtime_state

# 清除所有持久配置
ksud module config clear

# 清除所有暫時配置
ksud module config clear --temp

# 從 stdin 設定值(適用於多行或複雜資料)
ksud module config set my_key <<EOF
多行
文字值
EOF

# 或從命令管道輸入
echo "value" | ksud module config set my_key

# 明確使用 stdin 標誌
cat file.json | ksud module config set json_data --stdin
```

## 驗證限制

配置系統強制執行以下限制:

- **最大鍵長度**:256 位元組
- **最大值長度**:1MB (1048576 位元組)
- **最大配置項數**:每個模組 32 個
- **鍵格式**:必須符合 `^[a-zA-Z][a-zA-Z0-9._-]+$`(與模組 ID 相同)
  - 必須以字母(a-zA-Z)開頭
  - 可包含字母、數字、點(`.`)、底線(`_`)或連字號(`-`)
  - 最小長度:2 個字元
- **值格式**:無限制 - 可包含任何 UTF-8 字元,包括換行符、控制字元等
  - 以二進位格式儲存,帶長度前綴,確保安全處理所有資料

## 生命週期

- **啟動時**:所有暫時配置在 post-fs-data 階段清除
- **模組解除安裝時**:所有配置(持久和暫時)自動刪除
- 配置以二進位格式儲存,使用魔數 `0x4b53554d`("KSUM")和版本驗證

## 使用場景

配置系統適用於:

- **使用者偏好**:儲存使用者透過 WebUI 或 action 腳本配置的模組設定
- **功能開關**:在不重新安裝的情況下啟用/停用模組功能
- **執行時狀態**:追蹤應在重新開機時重置的暫時狀態(使用暫時配置)
- **安裝設定**:記住模組安裝時做出的選擇
- **複雜資料**:儲存 JSON、多行文字、Base64 編碼資料或任何結構化內容(最多 1MB)

::: tip 最佳實踐
- 對於應在重新開機後保留的使用者偏好,使用持久配置
- 對於應在啟動時重置的執行時狀態或功能開關,使用暫時配置
- 在腳本中使用配置值之前驗證它們
- 使用 `ksud module config list` 命令偵錯配置問題
:::

## 進階功能

模組配置系統提供了用於進階用例的特殊配置鍵:

### 覆蓋模組描述

您可以透過設定 `override.description` 配置鍵來動態覆蓋 `module.prop` 中的 `description` 欄位:

```bash
# 覆蓋模組描述
ksud module config set override.description "在管理器中顯示的自訂描述"
```

當取得模組列表時,如果存在 `override.description` 配置,它將取代 `module.prop` 中的原始描述。這對於以下場景很有用:
- 在模組描述中顯示動態狀態資訊
- 向使用者顯示執行時配置詳情
- 基於模組狀態更新描述而無需重新安裝

### 宣告管理的功能

模組可以使用 `manage.<feature>` 配置模式宣告它們管理的 KernelSU 功能。支援的功能對應於 KernelSU 內部的 `FeatureId` 列舉:

**支援的功能:**
- `su_compat` - SU 相容模式
- `kernel_umount` - 核心自動卸載
- `enhanced_security` - 增強安全模式

```bash
# 宣告此模組管理 SU 相容性並將其啟用
ksud module config set manage.su_compat true

# 宣告此模組管理核心卸載並將其停用
ksud module config set manage.kernel_umount false

# 移除功能管理(模組不再控制此功能)
ksud module config delete manage.su_compat
```

**工作原理:**
- `manage.<feature>` 鍵的存在表示模組正在管理該功能
- 值表示期望的狀態:`true`/`1` 代表啟用,`false`/`0`(或任何其他值)代表停用
- 要停止管理某個功能,請完全刪除該配置鍵

管理的功能透過模組列表 API 以 `managedFeatures` 欄位(逗號分隔的字串)公開。這允許:
- KernelSU 管理器偵測哪些模組管理哪些 KernelSU 功能
- 防止多個模組嘗試管理同一功能時發生衝突
- 更好地協調模組與核心 KernelSU 功能之間的關係

::: warning 僅支援預定義功能
僅使用上面列出的預定義功能名稱(`su_compat`、`kernel_umount`、`enhanced_security`)。這些對應於實際的 KernelSU 內部功能。使用其他功能名稱不會導致錯誤,但沒有任何功能作用。
:::
