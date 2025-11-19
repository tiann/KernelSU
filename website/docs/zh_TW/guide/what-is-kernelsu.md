# 什麼是 KernelSU？ {#what-is-kernelsu}

KernelSU 是 Android GKI 裝置的 Root 解決方案，它以核心模式運作，並直接在核心空間中為使用者空間應用程式授予 Root 權限。

## 功能 {#features}

KernelSU 的主要功能是它是**基於核心的**。 KernelSU 在核心空間中執行，所以它可以向我們提供從未有過的核心介面。例如，我們可以在核心模式中為任何處理程序新增硬體中斷點；我們可以在任何處理程序的實體記憶體中存取，而無人知曉；我們可以在核心空間攔截任何系統呼叫；等等。

此外，KernelSU 提供了 [metamodule 系統](metamodule.md)，這是一個用於模組管理的可插拔架構。與傳統 root 解決方案將掛載邏輯內建於核心的做法不同，KernelSU 將此工作委託給 metamodule。這允許您安裝 metamodule(如 [meta-overlayfs](https://github.com/tiann/KernelSU/tree/main/userspace/meta-overlayfs))來提供對 `/system` 分區和其他分區的 systemless 修改。

## 如何使用 {#how-to-use}

請參閱：[安裝](installation)

## 如何建置 {#how-to-build}

请參閱：[如何建置](how-to-build)

## 討論 {#discussion}

- Telegram: [@KernelSU](https://t.me/KernelSU)
