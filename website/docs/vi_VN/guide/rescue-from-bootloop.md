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

Nếu bạn đã flash một mô-đun đã được chứng minh là an toàn nhưng khiến thiết bị của bạn không khởi động được thì tình huống này có thể dễ dàng phục hồi trong KernelSU mà không phải lo lắng gì. KernelSU có các cơ chế tích hợp sẵn để giải cứu thiết bị của bạn, bao gồm:

1. Cập nhật AB
2. Cứu bằng cách nhấn Giảm âm lượng

#### Cập nhật AB

Các bản cập nhật mô-đun của KernelSU lấy cảm hứng từ cơ chế cập nhật AB của hệ thống Android được sử dụng trong các bản cập nhật OTA. Nếu bạn cài đặt một mô-đun mới hoặc cập nhật mô-đun hiện có, nó sẽ không trực tiếp sửa đổi tệp mô-đun hiện đang sử dụng. Thay vào đó, tất cả các mô-đun sẽ được tích hợp vào một hình ảnh cập nhật khác. Sau khi hệ thống được khởi động lại, nó sẽ cố gắng bắt đầu sử dụng hình ảnh cập nhật này. Nếu hệ thống Android khởi động thành công, các mô-đun sẽ được cập nhật thực sự.

Vì vậy, phương pháp đơn giản và được sử dụng phổ biến nhất để cứu thiết bị của bạn là **buộc khởi động lại**. Nếu bạn không thể khởi động hệ thống của mình sau khi flash một mô-đun, bạn có thể nhấn và giữ nút nguồn trong hơn 10 giây và hệ thống sẽ tự động khởi động lại; sau khi khởi động lại, nó sẽ quay trở lại trạng thái trước khi cập nhật mô-đun và các mô-đun được cập nhật trước đó sẽ tự động bị tắt.

#### Cứu bằng cách nhấn Giảm âm lượng

Nếu bản cập nhật AB vẫn không giải quyết được vấn đề, bạn có thể thử sử dụng **Chế độ an toàn**. Ở Chế độ an toàn, tất cả các mô-đun đều bị tắt.

Có hai cách để vào Chế độ an toàn:

1. Chế Độ An Toàn tích hợp (built-in Safe Mode) của một số hệ thống; một số hệ thống có Chế độ an toàn tích hợp có thể được truy cập bằng cách nhấn và giữ nút giảm âm lượng, trong khi những hệ thống khác (chẳng hạn như MIUI) có thể bật Chế Độ An Toàn trong Recovery. Khi vào Chế Độ An Toàn của hệ thống, KernelSU cũng sẽ vào Chế Độ An Toàn và tự động tắt các mô-đun.
2. Chế Độ An Toàn tích hợp (built-in Safe Mode) của KernelSU; phương pháp thao tác là **nhấn phím giảm âm lượng liên tục hơn ba lần** sau màn hình khởi động đầu tiên. Lưu ý là nhấn-thả, nhấn-thả, nhấn-thả chứ không phải nhấn giữ.

Sau khi vào chế độ an toàn, tất cả các mô-đun trên trang mô-đun của KernelSU Manager đều bị tắt nhưng bạn có thể thực hiện thao tác "gỡ cài đặt" để gỡ cài đặt bất kỳ mô-đun nào có thể gây ra sự cố.

Chế độ an toàn tích hợp được triển khai trong kernel, do đó không có khả năng thiếu các sự kiện chính do bị chặn. Tuy nhiên, đối với các hạt nhân không phải GKI, có thể cần phải tích hợp mã thủ công và bạn có thể tham khảo tài liệu chính thức để được hướng dẫn.

### Mô-đun độc hại

Nếu các phương pháp trên không thể cứu được thiết bị của bạn thì rất có thể mô-đun bạn cài đặt có hoạt động độc hại hoặc đã làm hỏng thiết bị của bạn thông qua các phương tiện khác. Trong trường hợp này, chỉ có hai gợi ý:

1. Xóa sạch dữ liệu và flash hệ thống chính thức.
2. Tham khảo dịch vụ hậu mãi.
