# Cứu khỏi bootloop (Vòng lặp khởi động)

Khi flash một thiết bị, chúng ta có thể gặp phải tình trạng máy "bị brick". Về lý thuyết, nếu bạn chỉ sử dụng fastboot để flash phân vùng boot hoặc cài đặt các mô-đun không phù hợp khiến máy không khởi động được thì điều này có thể được khắc phục bằng các thao tác thích hợp. Tài liệu này nhằm mục đích cung cấp một số phương pháp khẩn cấp để giúp bạn khôi phục từ thiết bị "bị brick".

## Brick bởi flash vào phân vùng boot

Trong KernelSU, các tình huống sau có thể gây ra lỗi khởi động khi flash phân vùng khởi động:

1. Bạn flash image boot sai định dạng. Ví dụ: nếu định dạng khởi động điện thoại của bạn là `gz`, nhưng bạn flash image định dạng `lz4` thì điện thoại sẽ không thể khởi động.
2. Điện thoại của bạn cần tắt xác minh AVB để khởi động bình thường (thường yêu cầu xóa tất cả dữ liệu trên điện thoại).
3. Kernel của bạn có một số lỗi hoặc không phù hợp để flash điện thoại của bạn.

Bất kể tình huống thế nào, bạn có thể khôi phục bằng cách **flash boot image gốc**. Do đó, khi bắt đầu hướng dẫn cài đặt, chúng tôi thực sự khuyên bạn nên sao lưu boot image gốc trước khi flash. Nếu chưa sao lưu, bạn có thể lấy boot image gốc từ người dùng khác có cùng thiết bị với bạn hoặc từ chương trình cơ sở chính thức (official firmware).

## Brick bởi mô-đun

Việc cài đặt mô-đun có thể là nguyên nhân phổ biến hơn khiến thiết bị của bạn bị brick, nhưng chúng tôi phải nghiêm túc cảnh báo bạn: **Không cài đặt mô-đun từ các nguồn không xác định**! Vì các mô-đun có đặc quyền root nên chúng có thể gây ra thiệt hại không thể khắc phục cho thiết bị của bạn!

### Mô-đun bình thường

Nếu bạn đã flash một mô-đun đã được chứng minh là an toàn nhưng khiến thiết bị của bạn không khởi động được thì tình huống này có thể dễ dàng phục hồi trong KernelSU mà không phải lo lắng gì. KernelSU tích hợp Chế độ an toàn để giải cứu thiết bị của bạn:

#### Cứu bằng cách nhấn Giảm âm lượng {#volume-down}

Bạn có thể thử sử dụng **Chế độ an toàn** để giải cứu thiết bị của mình. Sau khi vào Chế độ an toàn, tất cả các mô-đun đều bị tắt.

Có hai cách để vào Chế độ an toàn:

1. Chế Độ An Toàn tích hợp (built-in Safe Mode) của một số hệ thống; một số hệ thống có Chế độ an toàn tích hợp có thể được truy cập bằng cách nhấn và giữ nút giảm âm lượng, trong khi những hệ thống khác (chẳng hạn như MIUI/HyperOS) có thể bật Chế Độ An Toàn trong Recovery. Khi vào Chế Độ An Toàn của hệ thống, KernelSU cũng sẽ vào Chế Độ An Toàn và tự động tắt các mô-đun.
2. Chế Độ An Toàn tích hợp (built-in Safe Mode) của KernelSU; phương pháp thao tác là **nhấn phím giảm âm lượng liên tục hơn ba lần** sau màn hình khởi động đầu tiên. Lưu ý là nhấn-thả, nhấn-thả, nhấn-thả chứ không phải nhấn giữ.

Sau khi vào chế độ an toàn, tất cả các mô-đun trên trang mô-đun của KernelSU Manager đều bị tắt nhưng bạn có thể thực hiện thao tác "gỡ cài đặt" để gỡ cài đặt bất kỳ mô-đun nào có thể gây ra sự cố.

Chế độ an toàn tích hợp được triển khai trong kernel, do đó không có khả năng thiếu các sự kiện chính do bị chặn. Tuy nhiên, đối với các hạt nhân không phải GKI, có thể cần phải tích hợp mã thủ công và bạn có thể tham khảo tài liệu chính thức để được hướng dẫn.

::: warning
KernelSU đăng ký bộ lắng nghe phím âm lượng trong quá trình khởi tạo mô-đun hạt nhân (được tải khi hạt nhân thực thi quy trình init ở chế độ LKM), và hủy đăng ký ở giai đoạn `on_post_fs_data` (trước hoạt ảnh khởi động). Bạn cần nắm bắt thời gian và nhanh chóng nhấn phím giảm âm lượng ba lần sau màn hình khởi động đầu tiên. Nếu thiết bị khởi động nhanh hoặc thao tác không kịp thời, chế độ an toàn có thể không được kích hoạt.

Nếu mô-đun viết mã không hợp lý trong initrc khiến thiết bị không thể khởi động, những mã này vẫn sẽ được thực thi ngay cả trong chế độ an toàn.
:::

#### Cứu Hộ Thủ Công {#manual-rescue}

Khi chế độ an toàn không thể giải quyết vấn đề, bạn có thể thử cứu hộ thủ công. Chọn các phương pháp sau tùy theo trạng thái của thiết bị.

**Phương pháp 1: Sử dụng ksud để quản lý các mô-đun thông qua ADB**

Nếu thiết bị có thể lấy root shell thông qua ADB, bạn có thể sử dụng dòng lệnh `ksud` trực tiếp để vô hiệu hóa hoặc gỡ cài đặt mô-đun có vấn đề:

::: tip
Sau khi gắn kết các phân vùng `metadata` và `data`, bạn có thể chạy lệnh `/data/adb/ksud` dưới chế độ Recovery để quản lý các mô-đun.

Vì các thiết bị GKI chia sẻ `init`, mô-đun hạt nhân KernelSU vẫn sẽ được tải dưới chế độ Recovery, bạn sẽ có thể sử dụng hầu hết các tính năng của `ksud` (như cài đặt tính năng) một cách bình thường.
:::

```
adb shell
su
ksud module list          # Liệt kê tất cả các mô-đun
ksud module disable <id>  # Vô hiệu hóa mô-đun có vấn đề
ksud module uninstall <id> # Hoặc gỡ cài đặt trực tiếp
reboot
```

**Phương pháp 2: Dọn dẹp thủ công thông qua Recovery**

Nếu bạn không thể vào hệ thống (ngay cả ADB cũng không thể kết nối), bạn cần một Recovery của bên thứ ba (chẳng hạn như TWRP) trên thiết bị.

Việc tải mô-đun của KernelSU phụ thuộc vào tệp chèn init.rc ở phía hạt nhân và tiến trình ksud trong không gian người dùng. Sau khi xóa các tệp này và khởi động lại, KernelSU sẽ không tải bất kỳ mô-đun nào.

**Các bước thao tác:**

1. Vào Recovery (chẳng hạn như TWRP).
2. Gắn kết phân vùng data:
   ```
   mount /data
   ```
   (Bạn có thể cần giải mã phân vùng data trước. Thao tác cụ thể tùy thuộc vào thiết bị và phương pháp giải mã.)
3. Xóa ksud để ngăn chặn tải mô-đun:
   ```
   rm -f /data/adb/ksud
   ```
4. (Tùy chọn) Gắn kết phân vùng metadata và xóa tệp chèn init.rc do mô-đun tạo ra:
   ```
   mount /metadata
   rm -f /metadata/ksu/modules.rc
   rm -f /metadata/watchdog/ksu/modules.rc
   ```
5. Khởi động lại thiết bị:
   ```
   reboot
   ```

KernelSU sẽ bỏ qua việc tải tất cả các mô-đun sau khi khởi động lại. Sau khi vào hệ thống, bạn có thể mở lại trình quản lý KernelSU để xử lý các vấn đề của mô-đun.

### Định dạng dữ liệu hoặc các mô-đun độc hại khác

Nếu các phương pháp trên không thể cứu được thiết bị của bạn thì rất có thể mô-đun bạn cài đặt có hoạt động độc hại hoặc đã làm hỏng thiết bị của bạn thông qua các phương tiện khác. Trong trường hợp này, chỉ có hai gợi ý:

1. Xóa sạch dữ liệu và flash hệ thống chính thức hoàn toàn.
2. Tham khảo dịch vụ hậu mãi.
