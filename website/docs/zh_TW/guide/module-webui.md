# 模組 WebUI {#module-webui}

KernelSU 的模組除了執行啟動腳本和修改系統檔案之外，還支援顯示 UI 介面和與使用者互動。

該模組可以透過任何 Web 技術編寫HTML + CSS + JavaScript頁面。 KernelSU的管理器將透過 WebView 顯示這些頁面。它還提供了一些用於與系統互動的JavaScript API，例如執行shell命令。

## WebUI 根目錄 {#webroot-directory}

Web資源應放置在模組根目錄的`webroot`子目錄中，並且其中**必須**有一個名為`index.html`的文件，該檔案是模組頁面入口。

包含Web介面的最簡單的模組結構如下：

```txt
❯ tree .
.
|-- module.prop
`-- webroot
    `-- index.html
```

:::warning
安裝模組時，KernelSU 將自動設定`webroot`的權限和 SELinux context。如果您不知道自己在做什麼，請不要自行設定該目錄的權限！
:::

如果您的頁面包含 CSS 和 JavaScript，您也需要將其放入此目錄中。

## JavaScript API

如果只是一個顯示頁面，那和一般網頁沒有什麼不同。更重要的是，KernelSU 提供了一系列的系統 API，讓您可以實現模組獨特的功能。

KernelSU 提供了一個 JavaScript 庫並[在 npm 上發布](https://www.npmjs.com/package/kernelsu)，您可以在網頁的 JavaScript 程式碼中使用它。

例如，您可以執行 shell 命令來取得特定配置或修改屬性：

```JavaScript
import { exec } from 'kernelsu';

const { errno, stdout } = exec("getprop ro.product.model");
```

再例如，你可以讓網頁全螢幕顯示，或是顯示一個 Toast。

[API 文檔](https://www.npmjs.com/package/kernelsu)

如果您發現現有的API無法滿足您的需求或使用不方便，歡迎您在[這裡](https://github.com/tiann/KernelSU/issues)給我們建議！
## 一些技巧

1. 您可以正常使用`localStorage`來儲存一些數據，但卸載管理器後，這些數據將會遺失。如果需要持久保存，可以自行將資料寫入某個目錄。
2. 對於簡單的頁面，我建議您使用[parceljs](https://parceljs.org/)進行打包。它無須設定，使用非常方便。不過，如果你是前端高手或有自己的喜好，那就選擇你喜歡的吧！
