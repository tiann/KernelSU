# Module WebUI

Ngoài việc chạy các script khởi động và chỉnh sửa tệp hệ thống, module KernelSU còn có thể hiển thị giao diện người dùng và tương tác trực tiếp với người dùng.

Module có thể định nghĩa các trang HTML + CSS + JavaScript bằng bất kỳ công nghệ web nào. Trình quản lý của KernelSU hiển thị những trang này thông qua WebView và cung cấp API để tương tác với hệ thống, chẳng hạn như thực thi lệnh shell.

## Thư mục `webroot`

Các tệp tài nguyên web cần được đặt trong thư mục con `webroot` ở thư mục gốc của module và **PHẢI** có tệp `index.html`, đây là điểm vào của trang module. Cấu trúc đơn giản nhất của một module có giao diện web trông như sau:

```txt
❯ tree .
.
|-- module.prop
`-- webroot
    `-- index.html
```

::: warning
Khi cài đặt module, KernelSU sẽ tự động đặt quyền và ngữ cảnh SELinux cho thư mục này. Nếu bạn không chắc mình đang làm gì, đừng tự ý thay đổi quyền của thư mục!
:::

Nếu trang của bạn có CSS hoặc JavaScript thì cũng cần đặt chúng trong thư mục này.

## JavaScript API

Nếu chỉ là trang hiển thị, nó sẽ hoạt động giống một trang web bình thường. Tuy nhiên điều quan trọng nhất là KernelSU cung cấp một loạt API hệ thống cho phép bạn triển khai các chức năng riêng của module.

KernelSU có một thư viện JavaScript được phát hành trên [npm](https://www.npmjs.com/package/kernelsu) để bạn dùng trong mã JavaScript của trang.

Ví dụ, bạn có thể thực thi một lệnh shell để lấy cấu hình hoặc thay đổi một thuộc tính:

```JavaScript
import { exec } from 'kernelsu';

const { errno, stdout } = exec("getprop ro.product.model");
```

Bạn cũng có thể chuyển trang sang chế độ toàn màn hình hoặc hiển thị thông báo toast.

[Tài liệu API](https://www.npmjs.com/package/kernelsu)

Nếu API hiện tại chưa đáp ứng nhu cầu hoặc khó sử dụng, hãy gửi đề xuất cho chúng tôi [tại đây](https://github.com/tiann/KernelSU/issues)!

## Một vài lưu ý

1. Bạn có thể sử dụng `localStorage` như bình thường để lưu dữ liệu, nhưng hãy nhớ rằng dữ liệu sẽ biến mất khi ứng dụng quản lý bị gỡ cài đặt. Nếu cần lưu trữ lâu dài, hãy tự lưu dữ liệu vào một thư mục cụ thể.
2. Với các trang đơn giản, chúng tôi gợi ý dùng [parceljs](https://parceljs.org/) để đóng gói. Công cụ này không cần cấu hình ban đầu và rất dễ dùng. Nếu bạn là chuyên gia front-end hoặc có sở thích khác, cứ thoải mái dùng công cụ bạn muốn!
