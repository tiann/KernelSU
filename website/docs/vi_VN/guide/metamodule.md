# Metamodule

Metamodule là một tính năng đột phá trong KernelSU cho phép chuyển các khả năng quan trọng của hệ thống module từ lõi sang các module có thể cắm thêm. Sự thay đổi kiến trúc này duy trì tính ổn định và bảo mật của KernelSU đồng thời giải phóng tiềm năng đổi mới lớn hơn cho hệ sinh thái module.

## Metamodule là gì?

Metamodule là một loại module đặc biệt của KernelSU cung cấp chức năng cơ sở hạ tầng cốt lõi cho hệ thống module. Không giống như các module thông thường sửa đổi tệp hệ thống, metamodule kiểm soát *cách thức* các module thông thường được cài đặt và mount.

Metamodule là cơ chế mở rộng dựa trên plugin cho phép tùy chỉnh hoàn toàn cơ sở hạ tầng quản lý module của KernelSU. Bằng cách ủy thác logic mount và cài đặt cho metamodule, KernelSU tránh trở thành điểm phát hiện dễ vỡ trong khi cho phép các chiến lược triển khai đa dạng.

**Đặc điểm chính:**

- **Vai trò cơ sở hạ tầng**: Metamodule cung cấp các dịch vụ mà các module thông thường phụ thuộc vào
- **Chỉ một instance**: Chỉ có thể cài đặt một metamodule tại một thời điểm
- **Thực thi ưu tiên**: Các script của metamodule chạy trước các script của module thông thường
- **Hook đặc biệt**: Cung cấp ba script hook cho cài đặt, mount và dọn dẹp

## Tại sao cần Metamodule?

Các giải pháp root truyền thống tích hợp logic mount vào lõi của chúng, khiến chúng dễ bị phát hiện và khó phát triển hơn. Kiến trúc metamodule của KernelSU giải quyết những vấn đề này thông qua việc tách biệt các mối quan tâm.

**Lợi thế chiến lược:**

- **Giảm bề mặt phát hiện**: Bản thân KernelSU không thực hiện mount, giảm các vector phát hiện
- **Tính ổn định**: Lõi vẫn ổn định trong khi các triển khai mount có thể phát triển
- **Đổi mới**: Cộng đồng có thể phát triển các chiến lược mount thay thế mà không cần fork KernelSU
- **Lựa chọn**: Người dùng có thể chọn triển khai phù hợp nhất với nhu cầu của họ

**Tính linh hoạt của mount:**

- **Không mount**: Đối với người dùng chỉ sử dụng module không cần mount, tránh hoàn toàn chi phí mount
- **Mount OverlayFS**: Cách tiếp cận truyền thống với hỗ trợ lớp đọc-ghi (thông qua `meta-overlayfs`)
- **Magic mount**: Mount tương thích với Magisk để có khả năng tương thích ứng dụng tốt hơn
- **Triển khai tùy chỉnh**: Overlay dựa trên FUSE, mount VFS tùy chỉnh hoặc các phương pháp hoàn toàn mới

**Vượt xa mount:**

- **Khả năng mở rộng**: Thêm các tính năng như hỗ trợ kernel module mà không cần sửa đổi lõi KernelSU
- **Tính module hóa**: Cập nhật các triển khai độc lập với các bản phát hành KernelSU
- **Tùy chỉnh**: Tạo các giải pháp chuyên biệt cho các thiết bị hoặc trường hợp sử dụng cụ thể

::: warning QUAN TRỌNG
Nếu không cài đặt metamodule, các module sẽ **KHÔNG** được mount. Các cài đặt KernelSU mới yêu cầu cài đặt một metamodule (như `meta-overlayfs`) để các module hoạt động.
:::

## Dành cho Người dùng

### Cài đặt Metamodule

Cài đặt metamodule giống như cài đặt các module thông thường:

1. Tải xuống tệp ZIP metamodule (ví dụ: `meta-overlayfs.zip`)
2. Mở ứng dụng KernelSU Manager
3. Nhấn nút hành động nổi (➕)
4. Chọn tệp ZIP metamodule
5. Khởi động lại thiết bị của bạn

Metamodule `meta-overlayfs` là triển khai tham chiếu chính thức cung cấp mount module dựa trên overlayfs truyền thống với hỗ trợ ext4 image.

### Kiểm tra Metamodule đang hoạt động

Bạn có thể kiểm tra metamodule nào đang hoạt động trong trang Module của ứng dụng KernelSU Manager. Metamodule đang hoạt động sẽ được hiển thị trong danh sách module của bạn với chỉ định đặc biệt.

### Gỡ cài đặt Metamodule

::: danger CẢNH BÁO
Gỡ cài đặt metamodule sẽ ảnh hưởng đến **TẤT CẢ** các module. Sau khi gỡ bỏ, các module sẽ không còn được mount cho đến khi bạn cài đặt một metamodule khác.
:::

Để gỡ cài đặt:

1. Mở KernelSU Manager
2. Tìm metamodule trong danh sách module của bạn
3. Nhấn gỡ cài đặt (bạn sẽ thấy cảnh báo đặc biệt)
4. Xác nhận hành động
5. Khởi động lại thiết bị của bạn

Sau khi gỡ cài đặt, bạn nên cài đặt một metamodule khác nếu muốn các module tiếp tục hoạt động.

### Ràng buộc chỉ một Metamodule

Chỉ có thể cài đặt một metamodule tại một thời điểm. Nếu bạn cố gắng cài đặt metamodule thứ hai, KernelSU sẽ ngăn chặn việc cài đặt để tránh xung đột.

Để chuyển đổi metamodule:

1. Gỡ cài đặt tất cả các module thông thường
2. Gỡ cài đặt metamodule hiện tại
3. Khởi động lại
4. Cài đặt metamodule mới
5. Cài đặt lại các module thông thường của bạn
6. Khởi động lại một lần nữa

## Dành cho Nhà phát triển Module

Nếu bạn đang phát triển các module KernelSU thông thường, bạn không cần lo lắng nhiều về metamodule. Các module của bạn sẽ hoạt động miễn là người dùng có cài đặt một metamodule tương thích (như `meta-overlayfs`).

**Những điều bạn cần biết:**

- **Mount yêu cầu metamodule**: Thư mục `system` trong module của bạn sẽ chỉ được mount nếu người dùng có cài đặt metamodule cung cấp chức năng mount
- **Không cần thay đổi code**: Các module hiện có tiếp tục hoạt động mà không cần sửa đổi

::: tip
Nếu bạn quen thuộc với phát triển module Magisk, các module của bạn sẽ hoạt động tương tự trong KernelSU khi cài đặt metamodule, vì nó cung cấp mount tương thích với Magisk.
:::

## Dành cho Nhà phát triển Metamodule

Tạo một metamodule cho phép bạn tùy chỉnh cách KernelSU xử lý cài đặt, mount và gỡ cài đặt module.

### Yêu cầu Cơ bản

Một metamodule được xác định bởi một thuộc tính đặc biệt trong `module.prop`:

```txt
id=my_metamodule
name=My Custom Metamodule
version=1.0
versionCode=1
author=Your Name
description=Custom module mounting implementation
metamodule=1
```

Thuộc tính `metamodule=1` (hoặc `metamodule=true`) đánh dấu đây là một metamodule. Nếu không có thuộc tính này, module sẽ được coi là module thông thường.

### Cấu trúc Tệp

Cấu trúc một metamodule:

```txt
my_metamodule/
├── module.prop              (phải bao gồm metamodule=1)
│
│      *** Hook đặc biệt của metamodule ***
├── metamount.sh             (tùy chọn: xử lý mount tùy chỉnh)
├── metainstall.sh           (tùy chọn: hook cài đặt cho module thông thường)
├── metauninstall.sh         (tùy chọn: hook dọn dẹp cho module thông thường)
│
│      *** Tệp module tiêu chuẩn (tất cả đều tùy chọn) ***
├── customize.sh             (tùy chỉnh cài đặt)
├── post-fs-data.sh          (script giai đoạn post-fs-data)
├── service.sh               (script dịch vụ late_start)
├── boot-completed.sh        (script hoàn thành khởi động)
├── uninstall.sh             (script gỡ cài đặt của chính metamodule)
├── system/                  (sửa đổi systemless, nếu cần)
└── [bất kỳ tệp bổ sung nào]
```

Metamodule có thể sử dụng tất cả các tính năng module tiêu chuẩn (script vòng đời, v.v.) ngoài các hook metamodule đặc biệt của chúng.

### Script Hook

Metamodule có thể cung cấp tối đa ba script hook đặc biệt:

#### 1. metamount.sh - Xử lý Mount

**Mục đích**: Kiểm soát cách các module được mount trong quá trình khởi động.

**Khi thực thi**: Trong giai đoạn `post-fs-data`, trước khi bất kỳ script module nào chạy.

**Biến môi trường:**

- `MODDIR`: Đường dẫn thư mục của metamodule (ví dụ: `/data/adb/modules/my_metamodule`)
- Tất cả các biến môi trường KernelSU tiêu chuẩn

**Trách nhiệm:**

- Mount tất cả các module đã kích hoạt một cách systemless
- Kiểm tra cờ `skip_mount`
- Xử lý các yêu cầu mount cụ thể của module

::: danger YÊU CẦU QUAN TRỌNG
Khi thực hiện các thao tác mount, bạn **PHẢI** đặt tên nguồn/thiết bị thành `"KSU"`. Điều này xác định các mount thuộc về KernelSU.

**Ví dụ (đúng):**

```sh
mount -t overlay -o lowerdir=/lower,upperdir=/upper,workdir=/work KSU /target
```

**Đối với API mount hiện đại**, đặt chuỗi nguồn:

```rust
fsconfig_set_string(fs, "source", "KSU")?;
```

Điều này rất cần thiết để KernelSU xác định và quản lý các mount của nó đúng cách.
:::

**Script ví dụ:**

```sh
#!/system/bin/sh
MODDIR="${0%/*}"

# Ví dụ: Triển khai bind mount đơn giản
for module in /data/adb/modules/*; do
    if [ -f "$module/disable" ] || [ -f "$module/skip_mount" ]; then
        continue
    fi

    if [ -d "$module/system" ]; then
        # Mount với source=KSU (BẮT BUỘC!)
        mount -o bind,dev=KSU "$module/system" /system
    fi
done
```

#### 2. metainstall.sh - Hook Cài đặt

**Mục đích**: Tùy chỉnh cách các module thông thường được cài đặt.

**Khi thực thi**: Trong quá trình cài đặt module, sau khi các tệp được giải nén nhưng trước khi cài đặt hoàn tất. Script này được **sourced** (không thực thi) bởi trình cài đặt tích hợp, tương tự như cách `customize.sh` hoạt động.

**Biến môi trường và hàm:**

Script này kế thừa tất cả các biến và hàm từ `install.sh` tích hợp:

- **Biến**: `MODPATH`, `TMPDIR`, `ZIPFILE`, `ARCH`, `API`, `IS64BIT`, `KSU`, `KSU_VER`, `KSU_VER_CODE`, `BOOTMODE`, v.v.
- **Hàm**:
  - `ui_print <msg>` - In thông báo ra console
  - `abort <msg>` - In lỗi và kết thúc cài đặt
  - `set_perm <target> <owner> <group> <permission> [context]` - Đặt quyền tệp
  - `set_perm_recursive <directory> <owner> <group> <dirpermission> <filepermission> [context]` - Đặt quyền đệ quy
  - `install_module` - Gọi quy trình cài đặt module tích hợp

**Trường hợp sử dụng:**

- Xử lý tệp module trước hoặc sau khi cài đặt tích hợp (gọi `install_module` khi sẵn sàng)
- Di chuyển tệp module
- Xác thực khả năng tương thích của module
- Thiết lập cấu trúc thư mục đặc biệt
- Khởi tạo tài nguyên cụ thể của module

**Lưu ý**: Script này **KHÔNG** được gọi khi cài đặt chính metamodule.

#### 3. metauninstall.sh - Hook Dọn dẹp

**Mục đích**: Dọn dẹp tài nguyên khi các module thông thường được gỡ cài đặt.

**Khi thực thi**: Trong quá trình gỡ cài đặt module, trước khi thư mục module bị xóa.

**Biến môi trường:**

- `MODULE_ID`: ID của module đang được gỡ cài đặt

**Trường hợp sử dụng:**

- Xử lý tệp
- Dọn dẹp symlink
- Giải phóng tài nguyên đã phân bổ
- Cập nhật theo dõi nội bộ

**Script ví dụ:**

```sh
#!/system/bin/sh
# Được gọi khi gỡ cài đặt module thông thường
MODULE_ID="$1"
IMG_MNT="/data/adb/metamodule/mnt"

# Xóa tệp module khỏi image
if [ -d "$IMG_MNT/$MODULE_ID" ]; then
    rm -rf "$IMG_MNT/$MODULE_ID"
fi
```

### Thứ tự Thực thi

Hiểu thứ tự thực thi khởi động rất quan trọng cho phát triển metamodule:

```txt
Giai đoạn post-fs-data:
  1. Các script post-fs-data.d chung thực thi
  2. Prune module, restorecon, tải sepolicy.rule
  3. post-fs-data.sh của metamodule thực thi (nếu có)
  4. post-fs-data.sh của các module thông thường thực thi
  5. Tải system.prop
  6. metamount.sh của metamodule thực thi
     └─> Mount tất cả các module một cách systemless
  7. Giai đoạn post-mount.d chạy
     - Các script post-mount.d chung
     - post-mount.sh của metamodule (nếu có)
     - post-mount.sh của các module thông thường

Giai đoạn service:
  1. Các script service.d chung thực thi
  2. service.sh của metamodule thực thi (nếu có)
  3. service.sh của các module thông thường thực thi

Giai đoạn boot-completed:
  1. Các script boot-completed.d chung thực thi
  2. boot-completed.sh của metamodule thực thi (nếu có)
  3. boot-completed.sh của các module thông thường thực thi
```

**Điểm chính:**

- `metamount.sh` chạy **SAU** tất cả các script post-fs-data (cả metamodule và module thông thường)
- Các script vòng đời của metamodule (`post-fs-data.sh`, `service.sh`, `boot-completed.sh`) luôn chạy trước các script module thông thường
- Các script chung trong thư mục `.d` chạy trước các script metamodule
- Giai đoạn `post-mount` chạy sau khi mount hoàn tất

### Cơ chế Symlink

Khi một metamodule được cài đặt, KernelSU tạo một symlink:

```sh
/data/adb/metamodule -> /data/adb/modules/<metamodule_id>
```

Điều này cung cấp một đường dẫn ổn định để truy cập metamodule đang hoạt động, bất kể ID của nó.

**Lợi ích:**

- Đường dẫn truy cập nhất quán
- Dễ dàng phát hiện metamodule đang hoạt động
- Đơn giản hóa cấu hình

### Ví dụ Thực tế: meta-overlayfs

Metamodule `meta-overlayfs` là triển khai tham chiếu chính thức. Nó thể hiện các thực hành tốt nhất cho phát triển metamodule.

#### Kiến trúc

`meta-overlayfs` sử dụng **kiến trúc thư mục kép**:

1. **Thư mục metadata**: `/data/adb/modules/`
   - Chứa `module.prop`, các marker `disable`, `skip_mount`
   - Nhanh để quét trong quá trình khởi động
   - Dung lượng lưu trữ nhỏ

2. **Thư mục nội dung**: `/data/adb/metamodule/mnt/`
   - Chứa các tệp module thực tế (system, vendor, product, v.v.)
   - Được lưu trữ trong một ext4 image (`modules.img`)
   - Tối ưu hóa không gian với các tính năng ext4

#### Triển khai metamount.sh

Đây là cách `meta-overlayfs` triển khai xử lý mount:

```sh
#!/system/bin/sh
MODDIR="${0%/*}"
IMG_FILE="$MODDIR/modules.img"
MNT_DIR="$MODDIR/mnt"

# Mount ext4 image nếu chưa được mount
if ! mountpoint -q "$MNT_DIR"; then
    mkdir -p "$MNT_DIR"
    mount -t ext4 -o loop,rw,noatime "$IMG_FILE" "$MNT_DIR"
fi

# Đặt biến môi trường cho hỗ trợ thư mục kép
export MODULE_METADATA_DIR="/data/adb/modules"
export MODULE_CONTENT_DIR="$MNT_DIR"

# Thực thi binary mount
# (Logic mount thực tế nằm trong binary Rust)
"$MODDIR/meta-overlayfs"
```

#### Tính năng Chính

**Mount Overlayfs:**

- Sử dụng kernel overlayfs cho các sửa đổi systemless thực sự
- Hỗ trợ nhiều phân vùng (system, vendor, product, system_ext, odm, oem)
- Hỗ trợ lớp đọc-ghi thông qua `/data/adb/modules/.rw/`

**Xác định nguồn:**

```rust
// Từ meta-overlayfs/src/mount.rs
fsconfig_set_string(fs, "source", "KSU")?;  // BẮT BUỘC!
```

Điều này đặt `dev=KSU` cho tất cả các overlay mount, cho phép xác định đúng.

### Thực hành Tốt nhất

Khi phát triển metamodule:

1. **Luôn đặt source thành "KSU"** cho các thao tác mount - kernel umount và zygisksu umount cần điều này để umount đúng cách
2. **Xử lý lỗi một cách khéo léo** - các quy trình khởi động nhạy cảm về thời gian
3. **Tôn trọng các cờ tiêu chuẩn** - hỗ trợ `skip_mount` và `disable`
4. **Log các thao tác** - sử dụng `echo` hoặc logging để debug
5. **Kiểm tra kỹ lưỡng** - lỗi mount có thể gây ra vòng lặp khởi động
6. **Ghi chú hành vi** - giải thích rõ ràng metamodule của bạn làm gì
7. **Cung cấp đường dẫn di chuyển** - giúp người dùng chuyển đổi từ các giải pháp khác

### Kiểm tra Metamodule của bạn

Trước khi phát hành:

1. **Kiểm tra cài đặt** trên thiết lập KernelSU sạch
2. **Xác minh mount** với nhiều loại module khác nhau
3. **Kiểm tra khả năng tương thích** với các module phổ biến
4. **Kiểm tra gỡ cài đặt** và dọn dẹp
5. **Xác thực hiệu suất khởi động** (metamount.sh đang chặn!)
6. **Đảm bảo xử lý lỗi đúng cách** để tránh vòng lặp khởi động

## Câu hỏi Thường gặp

### Tôi có cần metamodule không?

**Đối với người dùng**: Chỉ cần nếu bạn muốn sử dụng các module yêu cầu mount. Nếu bạn chỉ sử dụng các module chạy script mà không sửa đổi tệp hệ thống, bạn không cần metamodule.

**Đối với nhà phát triển module**: Không, bạn phát triển module bình thường. Người dùng chỉ cần metamodule nếu module của bạn yêu cầu mount.

**Đối với người dùng nâng cao**: Chỉ cần nếu bạn muốn tùy chỉnh hành vi mount hoặc tạo các triển khai mount thay thế.

### Tôi có thể có nhiều metamodule không?

Không. Chỉ có thể cài đặt một metamodule tại một thời điểm. Điều này ngăn chặn xung đột và đảm bảo hành vi có thể dự đoán được.

### Điều gì xảy ra nếu tôi gỡ cài đặt metamodule duy nhất của mình?

Các module sẽ không còn được mount. Thiết bị của bạn sẽ khởi động bình thường, nhưng các sửa đổi của module sẽ không áp dụng cho đến khi bạn cài đặt một metamodule khác.

### meta-overlayfs có bắt buộc không?

Không. Nó cung cấp mount overlayfs tiêu chuẩn tương thích với hầu hết các module. Bạn có thể tạo metamodule của riêng mình nếu cần hành vi khác.

## Xem thêm

- [Module Guide](module.md) - Phát triển module chung
- [Difference with Magisk](difference-with-magisk.md) - So sánh KernelSU và Magisk
- [How to Build](how-to-build.md) - Xây dựng KernelSU từ nguồn
