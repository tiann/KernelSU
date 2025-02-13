# Module WebUI

In addition to executing boot scripts and modifying system files, KernelSU's modules also support displaying UI interfaces and interacting directly with users.

The module can write HTML + CSS + JavaScript pages through any web technology. KernelSU's manager will display these pages through WebView. It also provides some APIs to interact with the system, such as executing shell commands.

## `webroot` directory

Web resource files should be placed in the `webroot` subdirectory of the module root directory, and there **MUST** be a file named `index.html`, which is the module page entry. The simplest module structure containing a web interface is as follows:

```txt
❯ tree .
.
|-- module.prop
`-- webroot
    `-- index.html
```

::: warning
When installing the module, KernelSU will automatically set the permissions and SELinux context for this directory. If you don't know what you're doing, do not set the permissions for this directory yourself!
:::

If your page contains CSS and JavaScript, you need to place it in this directory as well.

## JavaScript API

If it's just a display page, it will function like a regular web page. However, the most important thing is that KernelSU provides a series of system APIs, allowing the implementation of module-specific functions.

KernelSU provides a JavaScript library, which is published on [npm](https://www.npmjs.com/package/kernelsu) and can be used in the JavaScript code of your web pages.

For example, you can execute a shell command to obtain a specific configuration or modify a property:

```JavaScript
import { exec } from 'kernelsu';

const { errno, stdout } = exec("getprop ro.product.model");
```

For another example, you can make the web page display full screen, or display a toast.

[API documentation](https://www.npmjs.com/package/kernelsu)

If you find that the existing API doesn't meet your needs or is inconvenient to use, you're welcome to give us suggestions [here](https://github.com/tiann/KernelSU/issues)!

## Some tips

1. You can use `localStorage` as usual to store some data, but keep in mind that it will be lost if the manager app is uninstalled. If you need persistent storage, you will need to manually save the data in a specific directory.
2. For simple pages, we recommend using [parceljs](https://parceljs.org/) for packaging. It requires no initial configuration and is extremely easy to use. However, if you're a front-end expert or have your own preferences, feel free to use the tool of your choice!
