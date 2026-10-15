# Script chạy một lần

Script chạy một lần (one-shot script) là một tệp ZIP mà trình quản lý KernelSU **chạy một lần** thay vì cài đặt. Nó được đóng gói giống như một module, nhưng không có gì được giữ lại trên thiết bị: trình quản lý giải nén ZIP vào một thư mục riêng, hỏi các tham số mà nó khai báo, hiển thị mã nguồn `action.sh` và chạy nó trong root shell. Hãy dùng nó cho các tác vụ như mở khóa một cài đặt ẩn hoặc xuất log; mọi thứ cần chạy khi khởi động hoặc sửa đổi các phân vùng hệ thống nên là một [module](module.md).

## Cấu trúc gói

```txt
<id>-<version>.zip
├── module.prop      # the usual module.prop fields + oneshot=true
├── action.sh        # required: the script the manager runs
├── params.json      # optional: parameters asked before running
├── customize.sh     # optional: only used if the ZIP is installed as a module
└── other files      # data the script reads via $MODPATH
```

`module.prop` sử dụng các trường giống như một [module](module.md#module-prop) và bổ sung `oneshot`:

```txt
id=example_script
name=Example Script
version=v1
versionCode=1
author=Alice
description=Does a thing once
oneshot=true
```

`oneshot` được đọc giống như `metamodule`: giá trị, bỏ qua khoảng trắng xung quanh, là `1` hoặc `true` (không phân biệt hoa thường). Bất kỳ giá trị nào khác, hoặc không có dòng `oneshot`, đều có nghĩa là một module thông thường.

Không đưa vào các tệp chỉ dành cho module như `post-fs-data.sh`, `service.sh`, `system.prop`, `sepolicy.rule` hoặc thư mục `system/`: chúng không bao giờ được sử dụng trong một lần chạy one-shot. Script chạy một lần không thể là metamodule.

## Chạy script

Có hai cách để chạy một script chạy một lần:

- **Từ kho module**: các mục trong registry [KernelSU Modules](https://modules.kernelsu.org) có `kind: script` hiển thị huy hiệu "Script" và nút **Run** thay vì **Cài đặt**. Tệp tải xuống được kiểm tra theo kích thước và SHA-256 của registry, và `module.prop` phải mang `id` đã đăng ký.
- **Từ tệp ZIP cục bộ**: nếu bạn chọn một tệp ZIP duy nhất từ bộ nhớ (hoặc mở nó từ một ứng dụng khác) và `module.prop` của nó có `oneshot=true`, trang chạy sẽ mở ra thay vì trình cài đặt. Trang này cũng cung cấp tùy chọn **Install as module anyway**.

Trước khi chạy, trình quản lý kiểm tra rằng `module.prop` khai báo `oneshot` với một `id` hợp lệ và `action.sh` tồn tại ở thư mục gốc của ZIP, sau đó giải nén ZIP. Các đường dẫn tuyệt đối, `..`, liên kết tượng trưng và tệp đặc biệt bị từ chối, và nội dung giải nén bị giới hạn ở 64 MiB và 4096 mục.

Trang chạy hiển thị biểu mẫu tham số, mã nguồn `action.sh` và lần chạy gần nhất trên thiết bị này. Sau khi xác nhận, trình quản lý chạy script trong một root shell mới ở mount namespace toàn cục, giống như nút hành động (Action) của module:

```sh
cd <dir> && <env> /data/adb/ksu/bin/busybox sh ./action.sh </dev/null
```

Đầu ra được hiển thị giống như đầu ra của Action và có thể được lưu lại. Thư mục giải nén sẽ bị xóa sau đó. Trình quản lý chỉ ghi nhớ lần chạy gần nhất (phiên bản và mã thoát); không có trạng thái đã cài đặt và không kiểm tra cập nhật.

### Biến môi trường

| Biến | Giá trị |
| --- | --- |
| `ASH_STANDALONE` | `1` |
| `KSU` | `true` |
| `KSU_VER`, `KSU_VER_CODE`, `KSU_KERNEL_VER_CODE` | các phiên bản KernelSU, giống như với module |
| `KSU_LATE_LOAD` | `1` chỉ trong [chế độ late-load](module.md#late-load-mode) |
| `KSU_MODULE` | ID của script |
| `KSU_ONESHOT` | `1`, để phân biệt lần chạy one-shot với nút Action của một module đã cài đặt |
| `MODPATH` | thư mục giải nén; hãy đọc các tệp dữ liệu của bạn từ đây |
| `PATH` | `$PATH:/data/adb/ksu/bin` |
| `KSU_PARAM_<KEY>` | mỗi tham số một biến, xem [`params.json`](#params-json) |

Stdin là `/dev/null`, vì vậy script không thể đọc đầu vào một cách tương tác; hãy khai báo tham số thay thế.

### Mã thoát

| Mã thoát | Ý nghĩa |
| --- | --- |
| `0` | Thành công. |
| `2` | Không áp dụng cho thiết bị này; được hiển thị như một thông báo, không phải lỗi. |
| bất kỳ giá trị nào khác | Thất bại. |

## `params.json` {#params-json}

`params.json` là tùy chọn. Nó nằm ở thư mục gốc của ZIP, là JSON UTF-8 và có kích thước tối đa 64 KiB. Trình quản lý xây dựng biểu mẫu từ nó và truyền mọi giá trị cho `action.sh` dưới dạng `KSU_PARAM_<KEY>`.

```json
{
  "schemaVersion": 1,
  "params": [
    {
      "key": "ENABLE_FASTBOOT",
      "type": "bool",
      "label": "Also enable fastboot",
      "description": "Optional longer help text",
      "default": true
    },
    { "key": "MODE", "type": "enum", "label": "Mode", "default": "a",
      "options": [ { "value": "a", "label": "A" }, { "value": "b" } ] },
    { "key": "NAME", "type": "string", "label": "Name", "default": "", "maxLength": 64,
      "required": false },
    { "key": "COUNT", "type": "int", "label": "Count", "default": 1, "min": 0, "max": 10 }
  ]
}
```

| Trường | Quy tắc |
| --- | --- |
| `schemaVersion` | Phải là `1`. |
| `params` | Tối đa 16 tham số. |
| `key` | `^[A-Z][A-Z0-9_]{0,31}$`, duy nhất. Giá trị được truyền tới script dưới dạng `KSU_PARAM_<key>`. |
| `type` | `bool`, `enum`, `string` hoặc `int`. |
| `label` | Bắt buộc, 1–200 ký tự. Nhãn được hiển thị đúng như khi viết (chỉ một ngôn ngữ). |
| `description` | Tùy chọn, quy tắc giống như `label`. |

Theo từng kiểu:

| Kiểu | Trường | Được truyền dưới dạng |
| --- | --- | --- |
| `bool` | `default`: `true` / `false` (mặc định `false`). | `true` / `false` |
| `enum` | `options`: 1–32 mục `{value, label?}`; `value` khớp với `^[A-Za-z0-9._-]{1,64}$` và là duy nhất. `default` phải là một trong các giá trị (mặc định: tùy chọn đầu tiên). | giá trị của tùy chọn |
| `string` | `default` (mặc định `""`), `maxLength` 1–1024 (mặc định 256), `required` (mặc định `false`; cho phép giá trị rỗng trừ khi bắt buộc). Giá trị không được chứa NUL hoặc ký tự xuống dòng. | như đã nhập |
| `int` | `min` / `max` và `default` tùy chọn (mặc định là `min`, hoặc `0` nếu không có `min`); số nguyên có dấu 32-bit. | số thập phân |

`default` của mọi tham số phải thỏa mãn các ràng buộc của chính nó. Nếu `params.json` không hợp lệ, trình quản lý sẽ từ chối chạy script.

::: warning
Không có kiểm tra định dạng cho các giá trị `string`. Mọi giá trị đều được truyền trong dấu nháy đơn, vì vậy nó không bao giờ bị shell khởi chạy `action.sh` mở rộng, nhưng `action.sh` vẫn phải xác thực giá trị và đặt nó trong dấu nháy khi sử dụng.
:::

## Ví dụ

```sh
#!/system/bin/sh
# action.sh
MODPATH=${MODPATH:-${0%/*}}

if [ "$(getprop ro.miui.ui.version.name)" = "" ]; then
  echo "Not a MIUI device"
  exit 2
fi

echo "Mode: ${KSU_PARAM_MODE:-a}"
if [ "$KSU_PARAM_ENABLE_FASTBOOT" = "true" ]; then
  sh "$MODPATH/fastboot.sh" || exit 1
fi
echo "Done"
```

## Tương thích với trình quản lý cũ

Các trình quản lý không hỗ trợ one-shot sẽ cài đặt ZIP như một module thông thường, có nút Action chạy `action.sh`. Hãy giữ cho `action.sh` cũng hoạt động trong trường hợp đó: khi ấy `KSU_ONESHOT`, `MODPATH` và `KSU_PARAM_*` không được đặt và thư mục làm việc là thư mục module. Hãy dùng giá trị mặc định thay thế, như trong ví dụ ở trên:

```sh
MODPATH=${MODPATH:-${0%/*}}
MODE=${KSU_PARAM_MODE:-a}
```

## Xuất bản

Để xuất bản một script chạy một lần trên [KernelSU Modules](https://modules.kernelsu.org), hãy đặt `kind: script` trong `module.yaml`. Xem [Script entries](https://github.com/KernelSU-Modules/tools/blob/main/docs/developers/module-yaml.md#script-entries-kind-script) để biết các quy tắc của registry.
