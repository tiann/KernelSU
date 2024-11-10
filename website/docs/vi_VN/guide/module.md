# Hướng dẫn mô-đun

KernelSU cung cấp một cơ chế mô-đun giúp đạt được hiệu quả sửa đổi thư mục hệ thống trong khi vẫn duy trì tính toàn vẹn của phân vùng system. Cơ chế này thường được gọi là "systemless".

Cơ chế mô-đun của KernelSU gần giống với Magisk. Nếu bạn đã quen với việc phát triển mô-đun Magisk thì việc phát triển các mô-đun KernelSU cũng rất tương tự. Bạn có thể bỏ qua phần giới thiệu các mô-đun bên dưới và chỉ cần đọc [difference-with-magisk](difference-with-magisk.md).

## Busybox

KernelSU cung cấp tính năng nhị phân BusyBox hoàn chỉnh (bao gồm hỗ trợ SELinux đầy đủ). Tệp thực thi được đặt tại `/data/adb/ksu/bin/busybox`. BusyBox của KernelSU hỗ trợ "ASH Standalone Shell Mode" có thể chuyển đổi thời gian chạy. Standalone mode này có nghĩa là khi chạy trong shell `ash` của BusyBox, mọi lệnh sẽ trực tiếp sử dụng applet trong BusyBox, bất kể cái gì được đặt là `PATH`. Ví dụ: các lệnh như `ls`, `rm`, `chmod` sẽ **KHÔNG** sử dụng những gì có trong `PATH` (trong trường hợp Android theo mặc định, nó sẽ là `/system/bin/ls`, ` /system/bin/rm` và `/system/bin/chmod` tương ứng), nhưng thay vào đó sẽ gọi trực tiếp các ứng dụng BusyBox nội bộ. Điều này đảm bảo rằng các tập lệnh luôn chạy trong môi trường có thể dự đoán được và luôn có bộ lệnh đầy đủ cho dù nó đang chạy trên phiên bản Android nào. Để buộc lệnh _not_ sử dụng BusyBox, bạn phải gọi tệp thực thi có đường dẫn đầy đủ.

Mỗi tập lệnh shell đơn lẻ chạy trong ngữ cảnh của KernelSU sẽ được thực thi trong shell `ash` của BusyBox với standalone mode được bật. Đối với những gì liên quan đến nhà phát triển bên thứ 3, điều này bao gồm tất cả các tập lệnh khởi động và tập lệnh cài đặt mô-đun.

Đối với những người muốn sử dụng tính năng "Standalone mode" này bên ngoài KernelSU, có 2 cách để kích hoạt tính năng này:

1. Đặt biến môi trường `ASH_STANDALONE` thành `1`<br>Ví dụ: `ASH_STANDALONE=1 /data/adb/ksu/bin/busybox sh <script>`
2. Chuyển đổi bằng các tùy chọn dòng lệnh:<br>`/data/adb/ksu/bin/busybox sh -o độc lập <script>`

Để đảm bảo tất cả shell `sh` tiếp theo được thực thi cũng chạy ở standalone mode, tùy chọn 1 là phương thức ưu tiên (và đây là những gì KernelSU và KernelSU manager sử dụng nội bộ) vì các biến môi trường được kế thừa xuống các tiến trình con.

::: tip sự khác biệt với Magisk

BusyBox của KernelSU hiện đang sử dụng tệp nhị phân được biên dịch trực tiếp từ dự án Magisk. **Cảm ơn Magisk!** Vì vậy, bạn không phải lo lắng về vấn đề tương thích giữa các tập lệnh BusyBox trong Magisk và KernelSU vì chúng hoàn toàn giống nhau!
:::

## Mô-đun hạt nhânSU

Mô-đun KernelSU là một thư mục được đặt trong `/data/adb/modules` với cấu trúc bên dưới:

```txt
/data/adb/modules
├── .
├── .
|
├── $MODID                  <--- Thư mục được đặt tên bằng ID của mô-đun
│   │
│   │      *** Nhận Dạng Mô-đun ***
│   │
│   ├── module.prop         <--- Tệp này lưu trữ metadata của mô-đun
│   │
│   │      *** Nội Dung Chính ***
│   │
│   ├── system              <--- Thư mục này sẽ được gắn kết nếu skip_mount không tồn tại
│   │   ├── ...
│   │   ├── ...
│   │   └── ...
│   │
│   │      *** Cờ Trạng Thái ***
│   │
│   ├── skip_mount          <--- Nếu tồn tại, KernelSU sẽ KHÔNG gắn kết thư mục hệ thống của bạn
│   ├── disable             <--- Nếu tồn tại, mô-đun sẽ bị vô hiệu hóa
│   ├── remove              <--- Nếu tồn tại, mô-đun sẽ bị xóa trong lần khởi động lại tiếp theo
│   │
│   │      *** Tệp Tùy Chọn ***
│   │
│   ├── post-fs-data.sh     <--- Tập lệnh này sẽ được thực thi trong post-fs-data
│   ├── post-mount.sh       <--- Tập lệnh này sẽ được thực thi trong post-mount
│   ├── service.sh          <--- Tập lệnh này sẽ được thực thi trong dịch vụ late_start
│   ├── boot-completed.sh   <--- Tập lệnh này sẽ được thực thi khi khởi động xong
|   ├── uninstall.sh        <--- Tập lệnh này sẽ được thực thi khi KernelSU xóa mô-đun của bạn
│   ├── system.prop         <--- Các thuộc tính trong tệp này sẽ được tải dưới dạng thuộc tính hệ thống bằng resetprop
│   ├── sepolicy.rule       <--- Quy tắc riêng biệt tùy chỉnh bổ sung
│   │
│   │      *** Được Tạo Tự Động, KHÔNG TẠO HOẶC SỬA ĐỔI THỦ CÔNG ***
│   │
│   ├── vendor              <--- Một liên kết tượng trưng đến $MODID/system/vendor
│   ├── product             <--- Một liên kết tượng trưng đến $MODID/system/product
│   ├── system_ext          <--- Một liên kết tượng trưng đến $MODID/system/system_ext
│   │
│   │      *** Mọi tập tin / thư mục bổ sung đều được phép ***
│   │
│   ├── ...
│   └── ...
|
├── another_module
│   ├── .
│   └── .
├── .
├── .
```

::: tip sự khác biệt với Magisk
KernelSU không có hỗ trợ tích hợp cho Zygisk nên không có nội dung liên quan đến Zygisk trong mô-đun. Tuy nhiên, bạn có thể sử dụng [ZygiskNext](https://github.com/Dr-TSNG/ZygiskNext) để hỗ trợ các mô-đun Zygisk. Trong trường hợp này, nội dung của mô-đun Zygisk giống hệt với nội dung được Magisk hỗ trợ.
:::

### module.prop

module.prop là tệp cấu hình cho mô-đun. Trong KernelSU, nếu một mô-đun không chứa tệp này, nó sẽ không được nhận dạng là mô-đun. Định dạng của tập tin này như sau:

```txt
id=<string>
name=<string>
version=<string>
versionCode=<int>
author=<string>
description=<string>
```

- `id` phải khớp với biểu thức chính quy này: `^[a-zA-Z][a-zA-Z0-9._-]+$`<br>
   ví dụ: ✓ `a_module`, ✓ `a.module`, ✓ `module-101`, ✗ `a module`, ✗ `1_module`, ✗ `-a-module`<br>
   Đây là **mã định danh duy nhất** của mô-đun của bạn. Bạn không nên thay đổi nó sau khi được xuất bản.
- `versionCode` phải là **số nguyên**. Điều này được sử dụng để so sánh các phiên bản
- Các chuỗi khác không được đề cập ở trên có thể là chuỗi **một dòng** bất kỳ.
- Đảm bảo sử dụng kiểu ngắt dòng `UNIX (LF)` chứ không phải `Windows (CR+LF)` hoặc `Macintosh (CR)`.

### Tập lệnh Shell

Vui lòng đọc phần [Boot Scripts](#boot-scripts) để hiểu sự khác biệt giữa `post-fs-data.sh` và `service.sh`. Đối với hầu hết các nhà phát triển mô-đun, `service.sh` sẽ đủ tốt nếu bạn chỉ cần chạy tập lệnh khởi động, nếu bạn cần chạy tập lệnh sau khi khởi động xong, vui lòng sử dụng `boot-completed.sh`. Nếu bạn muốn làm gì đó sau khi gắn các lớp phủ, vui lòng sử dụng `post-mount.sh`.

Trong tất cả các tập lệnh của mô-đun của bạn, vui lòng sử dụng `MODDIR=${0%/*}` để lấy đường dẫn thư mục cơ sở của mô-đun của bạn; **KHÔNG** mã hóa cứng đường dẫn mô-đun của bạn trong tập lệnh.

::: tip sự khác biệt với Magisk
Bạn có thể sử dụng biến môi trường KSU để xác định xem tập lệnh đang chạy trong KernelSU hay Magisk. Nếu chạy trong KernelSU, giá trị này sẽ được đặt thành true.
:::

### thư mục `system`

Nội dung của thư mục này sẽ được phủ lên trên phân vùng /system của hệ thống bằng cách sử dụng overlayfs sau khi hệ thống được khởi động. Điều này có nghĩa rằng:

1. Các file có cùng tên với các file trong thư mục tương ứng trong hệ thống sẽ bị ghi đè bởi các file trong thư mục này.
2. Các thư mục có cùng tên với thư mục tương ứng trong hệ thống sẽ được gộp với các thư mục trong thư mục này.

Nếu bạn muốn xóa một tập tin hoặc thư mục trong thư mục hệ thống gốc, bạn cần tạo một tập tin có cùng tên với tập tin/thư mục trong thư mục mô-đun bằng cách sử dụng `mknod filename c 0 0`. Bằng cách này, hệ thống lớp phủ sẽ tự động "whiteout" (Xóa trắng) tệp này như thể nó đã bị xóa (phân vùng /system không thực sự bị thay đổi).

Bạn cũng có thể khai báo một biến có tên `REMOVE` chứa danh sách các thư mục trong `customize.sh` để thực hiện các thao tác xóa và KernelSU sẽ tự động thực thi `mknod <TARGET> c 0 0` trong các thư mục tương ứng của mô-đun. Ví dụ:

```sh
REMOVE="
/system/app/YouTube
/system/app/Bloatware
"
```

Danh sách trên sẽ thực thi `mknod $MODPATH/system/app/YouTuBe c 0 0` và `mknod $MODPATH/system/app/Bloatware c 0 0`; và `/system/app/YouTube` và `/system/app/Bloatware` sẽ bị xóa sau khi mô-đun này có hiệu lực.

Nếu bạn muốn thay thế một thư mục trong hệ thống, bạn cần tạo một thư mục có cùng đường dẫn trong thư mục mô-đun của mình, sau đó đặt thuộc tính `setfattr -ntrust.overlay.opaque -v y <TARGET>` cho thư mục này. Bằng cách này, hệ thống Overlayfs sẽ tự động thay thế thư mục tương ứng trong hệ thống (mà không thay đổi phân vùng /system).

Bạn có thể khai báo một biến có tên `REPLACE` trong tệp `customize.sh` của mình, bao gồm danh sách các thư mục sẽ được thay thế và KernelSU sẽ tự động thực hiện các thao tác tương ứng trong thư mục mô-đun của bạn. Ví dụ:

REPLACE="
/system/app/YouTube
/system/app/Bloatware
"

Danh sách này sẽ tự động tạo các thư mục `$MODPATH/system/app/YouTube` và `$MODPATH/system/app/Bloatware`, sau đó thực thi `setfattr -ntrusted.overlay.opaque -v y $MODPATH/system/app/ YouTube` và `setfattr -n Trust.overlay.opaque -v y $MODPATH/system/app/Bloatware`. Sau khi mô-đun có hiệu lực, `/system/app/YouTube` và `/system/app/Bloatware` sẽ được thay thế bằng các thư mục trống.

::: tip sự khác biệt với Magisk

Cơ chế không hệ thống của KernelSU được triển khai thông qua các overlayfs của kernel, trong khi Magisk hiện sử dụng magic mount (bind mount). Hai phương pháp triển khai có những khác biệt đáng kể, nhưng mục tiêu cuối cùng đều giống nhau: sửa đổi các tệp /system mà không sửa đổi vật lý phân vùng /system.
:::

Nếu bạn quan tâm đến overlayfs, bạn nên đọc [documentation on overlayfs](https://docs.kernel.org/filesystems/overlayfs.html) của Kernel Linux.

### system.prop

Tệp này có cùng định dạng với `build.prop`. Mỗi dòng bao gồm `[key]=[value]`.

### sepolicy.rule

Nếu mô-đun của bạn yêu cầu một số bản vá lỗi chính sách bổ sung, vui lòng thêm các quy tắc đó vào tệp này. Mỗi dòng trong tệp này sẽ được coi là một tuyên bố chính sách.

## Trình cài đặt mô-đun

Trình cài đặt mô-đun KernelSU là mô-đun KernelSU được đóng gói trong tệp zip có thể được flash trong APP KernelSU manager. Trình cài đặt mô-đun KernelSU đơn giản chỉ là mô-đun KernelSU được đóng gói dưới dạng tệp zip.

```txt
module.zip
│
├── customize.sh                       <--- (Tùy chọn, biết thêm chi tiết sau)
│                                           Tập lệnh này sẽ có nguồn gốc từ update-binary
├── ...
├── ...  /* Các tập tin còn lại của mô-đun */
│
```

:::warning
Mô-đun KernelSU KHÔNG được hỗ trợ để cài đặt trong khôi phục tùy chỉnh!!
:::

### Tùy chỉnh

Nếu bạn cần tùy chỉnh quá trình cài đặt mô-đun, bạn có thể tùy ý tạo một tập lệnh trong trình cài đặt có tên `customize.sh`. Tập lệnh này sẽ được _sourced_ (không được thực thi!) bởi tập lệnh cài đặt mô-đun sau khi tất cả các tệp được trích xuất và các quyền mặc định cũng như văn bản thứ hai được áp dụng. Điều này rất hữu ích nếu mô-đun của bạn yêu cầu thiết lập bổ sung dựa trên ABI của thiết bị hoặc bạn cần đặt các quyền/văn bản thứ hai đặc biệt cho một số tệp mô-đun của mình.

Nếu bạn muốn kiểm soát và tùy chỉnh hoàn toàn quá trình cài đặt, hãy khai báo `SKIPUNZIP=1` trong `customize.sh` để bỏ qua tất cả các bước cài đặt mặc định. Bằng cách đó, `customize.sh` của bạn sẽ chịu trách nhiệm cài đặt mọi thứ.

Tập lệnh `customize.sh` chạy trong shell `ash` BusyBox của KernelSU với "Chế độ độc lập" được bật. Có sẵn các biến và hàm sau:

#### Biến

- `KSU` (bool): biến để đánh dấu script đang chạy trong môi trường KernelSU, và giá trị của biến này sẽ luôn đúng. Bạn có thể sử dụng nó để phân biệt giữa KernelSU và Magisk.
- `KSU_VER` (chuỗi): chuỗi phiên bản của KernelSU được cài đặt hiện tại (ví dụ: `v0.4.0`)
- `KSU_VER_CODE` (int): mã phiên bản của KernelSU được cài đặt hiện tại trong không gian người dùng (ví dụ: `10672`)
- `KSU_KERNEL_VER_CODE` (int): mã phiên bản của KernelSU được cài đặt hiện tại trong không gian kernel (ví dụ: `10672`)
- `BOOTMODE` (bool): luôn là `true` trong KernelSU
- `MODPATH` (đường dẫn): đường dẫn nơi các tập tin mô-đun của bạn sẽ được cài đặt
- `TMPDIR` (đường dẫn): nơi bạn có thể lưu trữ tạm thời các tập tin
- `ZIPFILE` (đường dẫn): zip cài đặt mô-đun của bạn
- `ARCH` (chuỗi): kiến trúc CPU của thiết bị. Giá trị là `arm`, `arm64`, `x86` hoặc `x64`
- `IS64BIT` (bool): `true` nếu `$ARCH` là `arm64` hoặc `x64`
- `API` (int): cấp độ API (phiên bản Android) của thiết bị (ví dụ: `23` cho Android 6.0)

::: warning
Trong KernelSU, MAGISK_VER_CODE luôn là 25200 và MAGISK_VER luôn là v25.2. Vui lòng không sử dụng hai biến này để xác định xem nó có chạy trên KernelSU hay không.
:::

#### Hàm

```txt
ui_print <msg>
    in <msg> ra console
    Tránh sử dụng 'echo' vì nó sẽ không hiển thị trong console của recovery tùy chỉnh

abort <msg>
    in thông báo lỗi <msg> ra bàn điều khiển và chấm dứt cài đặt
    Tránh sử dụng 'exit' vì nó sẽ bỏ qua các bước dọn dẹp chấm dứt

set_perm <target> <owner> <group> <permission> [context]
    nếu [context] không được đặt, mặc định là "u:object_r:system_file:s0"
    chức năng này là một shorthand cho các lệnh sau:
       chown owner.group target
       chmod permission target
       chcon context target

set_perm_recursive <directory> <owner> <group> <dirpermission> <filepermission> [context]
    nếu [context] không được đặt, mặc định là "u:object_r:system_file:s0"
    đối với tất cả các tệp trong <directory>, nó sẽ gọi:
       bối cảnh cấp phép tệp của nhóm chủ sở hữu tệp set_perm
    đối với tất cả các thư mục trong <directory> (bao gồm cả chính nó), nó sẽ gọi:
       set_perm bối cảnh phân quyền của nhóm chủ sở hữu thư mục
```

## Tập lệnh khởi động

Trong KernelSU, tập lệnh được chia thành hai loại dựa trên chế độ chạy của chúng: chế độ post-fs-data và chế độ dịch vụ late_start:

- chế độ post-fs-data
   - Giai đoạn này là BLOCKING. Quá trình khởi động bị tạm dừng trước khi thực thi xong hoặc đã trôi qua 10 giây.
   - Các tập lệnh chạy trước khi bất kỳ mô-đun nào được gắn kết. Điều này cho phép nhà phát triển mô-đun tự động điều chỉnh các mô-đun của họ trước khi nó được gắn kết.
   - Giai đoạn này xảy ra trước khi Zygote được khởi động, điều này gần như có ý nghĩa đối với mọi thứ trong Android
   - **CẢNH BÁO:** sử dụng `setprop` sẽ làm quá trình khởi động bị nghẽn! Thay vào đó, vui lòng sử dụng `resetprop -n <prop_name> <prop_value>`.
   - **Chỉ chạy tập lệnh ở chế độ này nếu cần thiết.**
- chế độ dịch vụ late_start
   - Giai đoạn này là NON-BLOCKING. Tập lệnh của bạn chạy song song với phần còn lại của quá trình khởi động.
   - **Đây là giai đoạn được khuyến nghị để chạy hầu hết các tập lệnh.**

Trong KernelSU, tập lệnh khởi động được chia thành hai loại dựa trên vị trí lưu trữ của chúng: tập lệnh chung và tập lệnh mô-đun:

- Kịch Bản Chung
   - Được đặt trong `/data/adb/post-fs-data.d`, `/data/adb/service.d`, `/data/adb/post-mount.d` hoặc `/data/adb/boot- đã hoàn thành.d`
   - Chỉ được thực thi nếu tập lệnh được đặt là có thể thực thi được (`chmod +x script.sh`)
   - Các tập lệnh trong `post-fs-data.d` chạy ở chế độ post-fs-data và các tập lệnh trong `service.d` chạy ở chế độ dịch vụ late_start.
   - Các mô-đun **KHÔNG** thêm các tập lệnh chung trong quá trình cài đặt
- Tập Lệnh Mô-đun
   - Được đặt trong thư mục riêng của mô-đun
   - Chỉ thực hiện nếu mô-đun được kích hoạt
   - `post-fs-data.sh` chạy ở chế độ post-fs-data, `service.sh` chạy ở chế độ dịch vụ late_start, `boot-completed.sh` chạy khi khởi động xong, `post-mount.sh` chạy trên overlayfs được gắn kết.

Tất cả các tập lệnh khởi động sẽ chạy trong shell `ash` BusyBox của KernelSU với "Standalone Mode" được bật.
