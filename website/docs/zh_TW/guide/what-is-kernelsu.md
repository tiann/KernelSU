# 什麼是 KernelSU？ {#what-is-kernelsu}

KernelSU 是 Android GKI 裝置的 Root 解決方案，它以核心模式運作，並直接在核心空間中為使用者空間應用程式授予 Root 權限。

## 功能 {#features}

KernelSU 的主要功能是它是**基於核心的**。 KernelSU 在核心空間中執行，所以它可以向我們提供從未有過的核心介面。例如，我們可以在核心模式中為任何處理程序新增硬體中斷點；我們可以在任何處理程序的實體記憶體中存取，而無人知曉；我們可以在核心空間攔截任何系統呼叫；等等。

KernelSU 還提供了一個以 overlayfs 為基礎的模組系統，允許您將自訂模組載入到系統中。它還提供了一種修改 `/system` 分區中檔案的機制。

## 如何使用 {#how-to-use}

請參閱：[安裝](installation)

## 如何建置 {#how-to-build}

请參閱：[如何建置](how-to-build)

## 討論 {#discussion}

- Telegram: [@KernelSU](https://t.me/KernelSU)
