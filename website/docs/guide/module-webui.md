# وحدة WebUI

بالإضافة إلى تنفيذ البرامج النصية للتمهيد وتعديل ملفات النظام، يمكن لوحدات KernelSU عرض واجهات المستخدم والتفاعل مباشرة مع المستخدمين.

يمكن للوحدات تحديد صفحات HTML + CSS + JavaScript باستخدام أي تقنية ويب. يعرض مدير KernelSU هذه الصفحات عبر WebView ويكشف واجهات برمجة التطبيقات للتفاعل مع النظام، مثل تنفيذ أوامر القشرة.

## 'webroot` directory

 يجب وضع ملفات موارد الويب في... 'webroot`index.html   الدليل الفرعي لدليل جذر الوحدة، وهناك.   **'** be a file named   , which is the module page entry. The simplest module structure containing a web interface is as follows:

"'
'- Webroot.
|- وحدة.prop
شجرة.
`-- webroot
 '-- index.html `-- index.html
"'

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

You can also make the page full screen or display a toast.

[API documentation](https://www.npmjs.com/package/kernelsu)

If you find that the existing API doesn't meet your needs or is inconvenient to use, you're welcome to give us suggestions [here](https://github.com/tiann/KernelSU/issues)!

## Some tips

1. You can use `localStorage` as usual to store some data, but keep in mind that it will be lost if the manager app is uninstalled. If you need persistent storage, you will need to manually save the data in a specific directory.
2. For simple pages, we recommend using [parceljs](https://parceljs.org/) for packaging. It requires no initial configuration and is extremely easy to use. However, if you're a front-end expert or have your own preferences, feel free to use the tool of your choice!
