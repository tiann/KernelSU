# Cấu hình module

KernelSU cung cấp hệ thống cấu hình tích hợp cho phép các module lưu trữ các cài đặt key-value liên tục hoặc tạm thời. Cấu hình được lưu trữ ở định dạng nhị phân tại `/data/adb/ksu/module_configs/<module_id>/` với các đặc điểm sau:

## Các loại cấu hình

- **Cấu hình liên tục** (`persist.config`): tồn tại sau khi khởi động lại cho đến khi bị xóa rõ ràng hoặc gỡ cài đặt module
- **Cấu hình tạm thời** (`tmp.config`): tự động bị xóa trong giai đoạn post-fs-data mỗi khi khởi động

Khi đọc cấu hình, giá trị tạm thời được ưu tiên hơn giá trị liên tục cho cùng một key.

## Sử dụng cấu hình trong script module

Tất cả các script module (`post-fs-data.sh`, `service.sh`, `boot-completed.sh`, v.v.) chạy với biến môi trường `KSU_MODULE` được đặt thành ID module. Bạn có thể sử dụng các lệnh `ksud module config` để quản lý cấu hình module của mình:

```bash
# Lấy giá trị cấu hình
value=$(ksud module config get my_setting)

# Đặt giá trị cấu hình liên tục
ksud module config set my_setting "some value"

# Đặt giá trị cấu hình tạm thời (xóa sau khi khởi động lại)
ksud module config set --temp runtime_state "active"

# Đặt giá trị từ stdin (hữu ích cho văn bản nhiều dòng hoặc dữ liệu phức tạp)
ksud module config set my_key <<EOF
văn bản nhiều dòng
giá trị
EOF

# Hoặc truyền từ lệnh
echo "value" | ksud module config set my_key

# Cờ stdin rõ ràng
cat file.json | ksud module config set json_data --stdin

# Liệt kê tất cả các mục cấu hình (hợp nhất liên tục và tạm thời)
ksud module config list

# Xóa một mục cấu hình
ksud module config delete my_setting

# Xóa một mục cấu hình tạm thời
ksud module config delete --temp runtime_state

# Xóa tất cả cấu hình liên tục
ksud module config clear

# Xóa tất cả cấu hình tạm thời
ksud module config clear --temp
```

## Giới hạn xác thực

Hệ thống cấu hình áp dụng các giới hạn sau:

- **Độ dài key tối đa**: 256 byte
- **Độ dài giá trị tối đa**: 1MB (1048576 byte)
- **Số lượng mục cấu hình tối đa**: 32 mỗi module
- **Định dạng key**: Phải khớp với `^[a-zA-Z][a-zA-Z0-9._-]+$` (như ID module)
  - Phải bắt đầu bằng một chữ cái
  - Có thể chứa chữ cái, số, dấu chấm, dấu gạch dưới hoặc dấu gạch ngang
  - Độ dài tối thiểu: 2 ký tự
- **Định dạng giá trị**: Không có hạn chế - có thể chứa bất kỳ ký tự UTF-8 nào, bao gồm ngắt dòng và ký tự điều khiển
  - Được lưu trữ ở định dạng nhị phân có tiền tố độ dài để xử lý dữ liệu an toàn

## Vòng đời

- **Khi khởi động**: Tất cả cấu hình tạm thời được xóa trong giai đoạn post-fs-data
- **Khi gỡ cài đặt module**: Tất cả cấu hình (liên tục và tạm thời) tự động bị xóa
- Cấu hình được lưu trữ ở định dạng nhị phân với số ma thuật `0x4b53554d` ("KSUM") và xác thực phiên bản

## Trường hợp sử dụng

Hệ thống cấu hình lý tưởng cho:

- **Tùy chọn người dùng**: Lưu trữ cài đặt module mà người dùng cấu hình thông qua WebUI hoặc action script
- **Cờ tính năng**: Bật/tắt tính năng module mà không cần cài đặt lại
- **Trạng thái runtime**: Theo dõi trạng thái tạm thời nên được đặt lại khi khởi động lại (sử dụng cấu hình tạm thời)
- **Cài đặt cài đặt**: Ghi nhớ các lựa chọn được thực hiện trong quá trình cài đặt module
- **Dữ liệu phức tạp**: Lưu trữ JSON, văn bản nhiều dòng, dữ liệu được mã hóa Base64 hoặc bất kỳ nội dung có cấu trúc nào (lên đến 1MB)

::: tip THỰC HÀNH TỐT NHẤT
- Sử dụng cấu hình liên tục cho tùy chọn người dùng nên tồn tại sau khi khởi động lại
- Sử dụng cấu hình tạm thời cho trạng thái runtime hoặc cờ tính năng nên được đặt lại khi khởi động
- Xác thực giá trị cấu hình trong script của bạn trước khi sử dụng chúng
- Sử dụng lệnh `ksud module config list` để gỡ lỗi các vấn đề cấu hình
:::

## Tính năng Nâng cao

Hệ thống cấu hình mô-đun cung cấp các khóa cấu hình đặc biệt cho các trường hợp sử dụng nâng cao:

### Ghi đè Mô tả Mô-đun

Bạn có thể ghi đè động trường `description` từ `module.prop` bằng cách đặt khóa cấu hình `override.description`:

```bash
# Ghi đè mô tả mô-đun
ksud module config set override.description "Mô tả tùy chỉnh được hiển thị trong trình quản lý"
```

Khi lấy danh sách mô-đun, nếu cấu hình `override.description` tồn tại, nó sẽ thay thế mô tả gốc từ `module.prop`. Điều này hữu ích cho:
- Hiển thị thông tin trạng thái động trong mô tả mô-đun
- Hiển thị chi tiết cấu hình runtime cho người dùng
- Cập nhật mô tả dựa trên trạng thái mô-đun mà không cần cài đặt lại

### Khai báo Tính năng được Quản lý

Các mô-đun có thể khai báo tính năng KernelSU nào mà chúng quản lý bằng cách sử dụng mẫu cấu hình `manage.<feature>`. Các tính năng được hỗ trợ tương ứng với enum nội bộ `FeatureId` của KernelSU:

**Tính năng được Hỗ trợ:**
- `su_compat` - Chế độ tương thích SU
- `kernel_umount` - Tự động unmount kernel
- `enhanced_security` - Chế độ bảo mật nâng cao

```bash
# Khai báo rằng mô-đun này quản lý khả năng tương thích SU và bật nó
ksud module config set manage.su_compat true

# Khai báo rằng mô-đun này quản lý unmount kernel và tắt nó
ksud module config set manage.kernel_umount false

# Xóa quản lý tính năng (mô-đun không còn kiểm soát tính năng này)
ksud module config delete manage.su_compat
```

**Cách hoạt động:**
- Sự hiện diện của khóa `manage.<feature>` cho biết mô-đun đang quản lý tính năng đó
- Giá trị cho biết trạng thái mong muốn: `true`/`1` để bật, `false`/`0` (hoặc bất kỳ giá trị nào khác) để tắt
- Để ngừng quản lý một tính năng, xóa hoàn toàn khóa cấu hình

Các tính năng được quản lý được hiển thị thông qua API danh sách mô-đun dưới dạng trường `managedFeatures` (chuỗi phân tách bằng dấu phẩy). Điều này cho phép:
- Trình quản lý KernelSU phát hiện mô-đun nào quản lý tính năng KernelSU nào
- Ngăn chặn xung đột khi nhiều mô-đun cố gắng quản lý cùng một tính năng
- Phối hợp tốt hơn giữa các mô-đun và chức năng cốt lõi của KernelSU

::: warning CHỈ CÁC TÍNH NĂNG ĐƯỢC HỖ TRỢ
Chỉ sử dụng các tên tính năng được xác định trước được liệt kê ở trên (`su_compat`, `kernel_umount`, `enhanced_security`). Chúng tương ứng với các tính năng nội bộ thực tế của KernelSU. Sử dụng các tên tính năng khác sẽ không gây lỗi nhưng không có mục đích chức năng nào.
:::
