# 模块 WebUI

KernelSU 的模块除了执行启动脚本和修改系统文件之外，还支持显示 UI 界面和与用户交互。

你可以通过任何 Web 技术编写 HTML + CSS + JavaScript 页面，KernelSU 的管理器将通过WebView 显示这些页面。此外，KernelSU 还提供了一些用于与系统交互的 JavaScript API，例如执行shell命令。

## WebUI 根目录

Web 资源文件应放置在模块根目录的 `webroot` 子目录中，并且其中**必须**有一个名为`index.html`的文件，该文件是模块页面入口。包含 Web 界面的最简单的模块结构如下：

````txt
❯ tree .
.
|-- module.prop
`-- webroot
     `--index.html
````

:::warning
安装模块时，KernelSU 会自动设置 `webroot` 目录的权限和 SELinux context，如果您不知道自己在做什么，请不要自行设置该目录的权限！
:::

如果您的页面包含 CSS 和 JavaScript，您也需要将其放入此目录中。

## JavaScript API

如果只是一个显示页面，那它和普通网页没有什么区别。更重要的是，KernelSU 提供了一系列的系统API，可以让您实现模块特有的功能。

KernelSU 提供了一个 JavaScript 库并[在 npm 上发布](https://www.npmjs.com/package/kernelsu)，您可以在网页的 JavaScript 代码中使用它。

例如，您可以执行 shell 命令来获取特定配置或修改属性：

```JavaScript
import { exec } from 'kernelsu';

const { errno, stdout } = await exec("getprop ro.product.model");
````

再比如，你可以让网页全屏显示，或者显示一个Toast。

[API文档](https://www.npmjs.com/package/kernelsu)

如果您发现现有的API不能满足您的需求或者使用不方便，欢迎[在这里](https://github.com/tiann/KernelSU/issues)给我们提出建议！

## 一些技巧

1. 您可以正常使用`localStorage`存储一些数据，但卸载管理器后，这些数据将会丢失。 如果需要持久保存，可以自己将数据写入某个目录。
2. 对于简单的页面，我建议您使用[parceljs](https://parceljs.org/)进行打包。它零配置，使用非常方便。不过，如果你是前端高手或者有自己的喜好，那就选择你喜欢的吧！
