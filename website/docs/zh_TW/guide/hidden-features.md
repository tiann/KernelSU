# 隱藏功能 {#hidden-features}

## .ksurc

預設狀況下，`/system/bin/sh` 會載入 `/system/etc/mkshrc`。

可以透過建立 `/data/adb/ksu/.ksurc` 檔案來讓 `su` 載入此檔案而非 `/system/etc/mkshrc`。