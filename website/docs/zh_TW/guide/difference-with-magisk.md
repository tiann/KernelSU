# KernelSU 模組與 Magisk 的差異 {#title}

儘管 KernelSU 模組和 Magisk 模組之間有許多相似之處，但由於它們完全不同的實作機制，不可避免地存在一些差異；如果您想讓您的模組同時在 Magisk 和 KernelSU 上運作，那麼您必須瞭解這些差異。

## 相同之處 {#similarities}

- 模組檔案格式：都以 Zip 的格式組織模組，並且模組的格式幾乎相同
- 模組安裝目錄：都位於 `/data/adb/modules`
- Systemless：都支援透過模組的形式以 systemless 修改 /system
- `post-fs-data.sh`：執行時間和語義完全相同
- `service.sh`：執行時間和語義完全相同
- `system.prop`：完全相同
- `sepolicy.rule`：完全相同
- BusyBox：指令碼在 BusyBox 中以「獨立模式」執行

## 不同之處 {#differences}

在瞭解不同之處之前，您需要知道如何區分您的模組是在 KernelSU 還是 Magisk 中執行；在所有可以執行模組指令碼的位置 (`customize.sh`, `post-fs-data.sh`, `service.sh`)，您都可以使用環境變數 `KSU` 來區分，在 KernelSU 中，這個環境變數將被設定為 `true`。

以下是一些不同之處：

1. KernelSU 的模組不支援在 Recovery 中安裝。
2. KernelSU 的模組沒有內建的 Zygisk 支援 (但您可以透過 [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext) 來使用 Zygisk 模組)。
3. KernelSU 模組取代或刪除檔案與 Magisk 完全不同。KernelSU 不支援 `.replace` 方法，相反，您需要透過 `mknod filename c 0 0` 建立相同名稱的資料夾以刪除對應檔案。
4. BusyBox 的目錄不同；KernelSU 內建的 BusyBox 在 `/data/adb/ksu/bin/busybox` 而 Magisk 在 `/data/adb/magisk/busybox`；**注意此為 KernelSU 內部行為，未來可能會變更！**
5. KernelSU 不支援 `.replace` 檔案；但 KernelSU 支援 `REPLACE` 和 `REMOVE` 變數以移除或取代檔案 (資料夾)。
