# 常見問題

## KernelSU 是否支援我的裝置？

首先，您的裝置應該能解鎖 Bootloader。如果不能，則不支援。

然後在您的裝置上安裝 KernelSU 管理員並開啟它，如果它顯示 `不支援`，那麼您的裝置沒有官方支援的開箱即用的 Boot 映像；但您可以自行建置核心來源並整合 KernelSU 以繼續使用。

## KernelSU 是否需要解鎖 Bootloader？

當然需要。

## KernelSU 是否支援模組？

支援，但它是早期版本，可能存在問題。請等候它逐漸穩定 :)

## KernelSU 是否支援 Xposed ？

支援。[Dreamland](https://github.com/canyie/Dreamland) 和 [TaiChi](https://taichi.cool) 可以正常運作。LSPosed 可以在 [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext) 的支援下正常運作。

## KernelSU 支援 Zygisk 嗎？

KernelSU 沒有內建 Zygisk 支援，但是您可以用 [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext) 來使用 Zygisk 模組。

## KernelSU 與 Magisk 相容嗎？

KernelSU 的模組系統與 Magisk 的 magic mount 存在衝突，如果在 KernelSU 中啟用了任何模組，那麼整個 Magisk 將無法正常運作。

但是如果您只使用 KernelSU 的 `su`，那么它會和 Magisk 一同運作：KernelSU 修改 `kernel` 、 Magisk 修改 `ramdisk`，它們可以搭配使用。

## KernelSU 会取代 Magisk 嗎？

我們不這樣認為，這也不是我們的目標。Magisk 對於使用者空間 Root 解決方案來說已經足夠優秀了，它會存在很長一段時間。KernelSU 的目標是為使用者提供核心介面，而非取代 Magisk。

## KernelSU 可以支援非 GKI 裝置嗎？

可以。但是您應該下載核心來源並整合 KernelSU 至來源樹狀結構並自行編譯核心。

## KernelSU 支援 Android 12 以下的裝置嗎？

影響 KernelSU 相容性的是裝置的核心版本，它與 Android 版本並無直接關係。唯一有關聯的是：**原廠** Android 12 的裝置，一定是 5.10 或更高的核心 (GKI 裝置)；因此結論如下：

1. 原廠 Android 12 的裝置必定支援 (GKI 裝置)
2. 舊版核心的裝置 (即使是 Android 12，也可能是舊版核心) 是相容的 (您需要自行建置核心)

## KernelSU 可以支援舊版核心嗎？

可以，目前最低支援到 4.14；更低的版本您需要手動移植它，歡迎 PR！

## 如何為舊版核心整合 KernelSU？

請參閱[指南](how-to-integrate-for-non-gki)

## 為何我的 Android 版本為 13，但核心版本卻是 "android12-5.10"？

核心版本與 Android 版本無關，如果您要刷新 KernelSU，請一律使用**核心版本**而非 Android 版本，如果你為 "android12-5.10" 的裝置刷新 Android 13 的核心，等候您的將會是開機迴圈。

## KernelSU 支援 --mount-master/全域掛接命名空間嗎？

目前沒有 (未來可能會支援)，但實際上有很多種方法手動進入全域命名空間，無需 Su 內建支援，比如：

1. `nsenter -t 1 -m sh` 可以取得一個全域 mount namespace 的 shell.
2. 在您要執行的命令前新增 `nsenter --mount=/proc/1/ns/mnt` 即可使此命令在全域 mount namespace 下執行。KernelSU 本身也使用了 [這種方法](https://github.com/tiann/KernelSU/blob/77056a710073d7a5f7ee38f9e77c9fd0b3256576/manager/app/src/main/java/me/weishu/kernelsu/ui/util/KsuCli.kt#L115)

## 我是 GKI1.0，能用 KernelSU 嗎？

GKI1 與 GKI2 完全不同，所以您需要自行編譯核心。
