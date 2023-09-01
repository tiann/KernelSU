# App Profile

App Profile là một cơ chế do KernelSU cung cấp để tùy chỉnh cấu hình của các ứng dụng khác nhau.

Đối với các ứng dụng được cấp quyền root (tức là có thể sử dụng `su`), App Profile cũng có thể được gọi là Root Profile. Nó cho phép tùy chỉnh các quy tắc `uid`, `gid`, `groups`, `capabilities` và `SELinux` của lệnh `su`, do đó hạn chế các đặc quyền của người dùng root. Ví dụ: nó có thể chỉ cấp quyền mạng cho các ứng dụng tường lửa trong khi từ chối quyền truy cập tệp hoặc có thể cấp quyền shell thay vì quyền truy cập root đầy đủ cho các ứng dụng đóng băng: **giữ nguồn điện theo nguyên tắc đặc quyền tối thiểu.**

Đối với các ứng dụng thông thường không có quyền root, App Profile có thể kiểm soát hành vi của hệ thống kernel và mô-đun đối với các ứng dụng này. Ví dụ, nó có thể xác định liệu các sửa đổi do mô-đun tạo ra có nên được giải quyết hay không. Hệ thống kernel và mô-đun có thể đưa ra quyết định dựa trên cấu hình này, chẳng hạn như thực hiện các hoạt động tương tự như "hiding"

## Root Profile

### UID, GID, và Groups

Hệ thống Linux có hai khái niệm: người dùng (user) và nhóm (group). Mỗi người dùng có một user ID (UID) và một người dùng có thể thuộc nhiều nhóm, mỗi nhóm có group ID (GID) riêng. Những ID này được sử dụng để xác định người dùng trong hệ thống và xác định tài nguyên hệ thống nào họ có thể truy cập.

Người dùng có UID bằng 0 được gọi là người dùng root và các nhóm có GID bằng 0 được gọi là nhóm root. Nhóm người dùng root thường giữ các đặc quyền hệ thống cao nhất.

Trong trường hợp hệ thống Android, mỗi ứng dụng là một người dùng riêng biệt (không bao gồm các trường hợp UID dùng chung) với một UID duy nhất. Ví dụ: `0` đại diện cho người dùng root, `1000` đại diện cho `system`, `2000` đại diện cho ADB shell và các UID từ 10000 đến 19999 đại diện cho các ứng dụng thông thường.

:::info
Ở đây, UID được đề cập không giống với khái niệm nhiều người dùng hoặc hồ sơ công việc (Work profile) trong hệ thống Android. Hồ sơ công việc thực sự được triển khai bằng cách phân vùng phạm vi UID. Ví dụ: 10000-19999 đại diện cho người dùng chính, trong khi 110000-119999 đại diện cho hồ sơ công việc. Mỗi ứng dụng thông thường trong số đó đều có UID riêng.
:::

Mỗi ứng dụng có thể có nhiều nhóm, với GID đại diện cho nhóm chính, thường khớp với UID. Các nhóm khác được gọi là nhóm bổ sung. Một số quyền nhất định được kiểm soát thông qua các nhóm, chẳng hạn như quyền truy cập mạng hoặc truy cập Bluetooth.

Ví dụ: nếu chúng ta thực thi lệnh `id` trong shell ADB, kết quả đầu ra có thể trông như thế này:

```sh
oriole:/ $ id
uid=2000(shell) gid=2000(shell) groups=2000(shell),1004(input),1007(log),1011(adb),1015(sdcard_rw),1028(sdcard_r),1078(ext_data_rw),1079(ext_obb_rw),3001(net_bt_admin),3002(net_bt),3003(inet),3006(net_bw_stats),3009(readproc),3011(uhid),3012(readtracefs) context=u:r:shell:s0
```

Ở đây, UID là `2000` và GID (ID nhóm chính) cũng là `2000`. Ngoài ra, nó thuộc một số nhóm bổ sung, chẳng hạn như `inet` (biểu thị khả năng tạo ổ cắm `AF_INET` và `AF_INET6`) và `sdcard_rw` (biểu thị quyền đọc/ghi đối với thẻ SD).

Root Profile của KernelSU cho phép tùy chỉnh UID, GID và các nhóm cho quy trình gốc sau khi thực thi `su`. Ví dụ: Cấu hình gốc của ứng dụng gốc có thể đặt UID của nó thành `2000`, có nghĩa là khi sử dụng `su`, các quyền thực tế của ứng dụng sẽ ở cấp shell ADB. Nhóm `inet` có thể bị xóa, ngăn lệnh `su` truy cập mạng.

:::tip Ghi chú
Hồ sơ ứng dụng chỉ kiểm soát các quyền của tiến trình gốc sau khi sử dụng `su`; nó không kiểm soát các quyền của ứng dụng. Nếu một ứng dụng đã yêu cầu quyền truy cập mạng, ứng dụng đó vẫn có thể truy cập mạng ngay cả khi không sử dụng `su`. Việc xóa nhóm `inet` khỏi `su` chỉ ngăn `su` truy cập mạng.
:::

Root Profile được thực thi trong kernel và không dựa vào hành vi tự nguyện của các ứng dụng root, không giống như việc chuyển đổi người dùng hoặc nhóm thông qua `su`, việc cấp quyền `su` hoàn toàn phụ thuộc vào người dùng chứ không phải nhà phát triển.

### Capabilities

Capabilities (khả năng) là một cơ chế phân tách đặc quyền trong Linux.

Với mục đích thực hiện kiểm tra quyền, việc triển khai UNIX truyền thống phân biệt hai loại quy trình: quy trình đặc quyền (có ID người dùng hiệu quả là 0, được gọi là siêu người dùng hoặc root) và quy trình không có đặc quyền (có UID hiệu dụng khác 0). Các quy trình đặc quyền bỏ qua tất cả các bước kiểm tra quyền của kernel, trong khi các quy trình không có đặc quyền phải chịu sự kiểm tra quyền đầy đủ dựa trên thông tin xác thực của quy trình (thường là: UID hiệu quả, GID hiệu quả và danh sách nhóm bổ sung).

Bắt đầu với Linux 2.2, Linux chia các đặc quyền truyền thống được liên kết với siêu người dùng thành các đơn vị riêng biệt, được gọi là các khả năng, có thể được bật và tắt một cách độc lập.

Mỗi Khả năng đại diện cho một hoặc nhiều đặc quyền. Ví dụ: `CAP_DAC_READ_SEARCH` thể hiện khả năng bỏ qua việc kiểm tra quyền để đọc tệp cũng như quyền đọc và thực thi thư mục. Nếu người dùng có UID hiệu dụng là `0` (người dùng root) thiếu khả năng `CAP_DAC_READ_SEARCH` hoặc cao hơn, điều này có nghĩa là ngay cả khi họ là root, họ không thể tùy ý đọc tệp.

Cấu hình gốc của KernelSU cho phép tùy chỉnh các Khả năng của tiến trình gốc sau khi thực thi `su`, nhờ đó đạt được việc cấp một phần "quyền root". Không giống như UID và GID đã nói ở trên, một số ứng dụng gốc nhất định yêu cầu UID là `0` sau khi sử dụng `su`. Trong những trường hợp như vậy, việc giới hạn Khả năng của người dùng root này bằng UID `0` có thể hạn chế các hoạt động được phép của họ.

:::tip Rất Khuyến Nghị
Capabilities của Linux [tài liệu chính thức](https://man7.org/linux/man-pages/man7/capabilities.7.html) cung cấp giải thích chi tiết về các khả năng mà mỗi Capabilities thể hiện. Nếu bạn có ý định tùy chỉnh Capabilities, bạn nên đọc tài liệu này trước.
:::

### SELinux

SELinux là một cơ chế Kiểm Soát Truy Cập Bắt Buộc (Mandatory Access Control: MAC) mạnh mẽ. Nó hoạt động theo nguyên tắc **từ chối mặc định**: bất kỳ hành động nào không được cho phép rõ ràng đều bị từ chối.

SELinux có thể chạy ở hai chế độ chung:

1. Chế độ cho phép (Permissive mode): Các sự kiện từ chối được ghi lại nhưng không được thực thi.
2. Chế độ thực thi (Enforcing mode): Các sự kiện từ chối được ghi lại và thực thi.

:::warning Cảnh báo
Các hệ thống Android hiện đại phụ thuộc rất nhiều vào SELinux để đảm bảo an ninh hệ thống tổng thể. Chúng tôi khuyên bạn không nên sử dụng bất kỳ hệ thống tùy chỉnh nào chạy ở "chế độ cho phép" vì nó không mang lại lợi thế đáng kể nào so với hệ thống mở hoàn toàn.
:::

Việc giải thích khái niệm đầy đủ về SELinux rất phức tạp và nằm ngoài phạm vi của tài liệu này. Trước tiên nên hiểu hoạt động của nó thông qua các tài nguyên sau:

1. [Wikipedia](https://en.wikipedia.org/wiki/Security-Enhanced_Linux)
2. [Red Hat: What Is SELinux?](https://www.redhat.com/en/topics/linux/what-is-selinux)
3. [ArchLinux: SELinux](https://wiki.archlinux.org/title/SELinux)

Root Profile của KernelSU cho phép tùy chỉnh ngữ cảnh SELinux của tiến trình gốc sau khi thực thi `su`. Các quy tắc kiểm soát truy cập cụ thể có thể được đặt cho bối cảnh này để cho phép kiểm soát chi tiết hơn các quyền .

Trong các trường hợp điển hình, khi một ứng dụng thực thi `su`, nó sẽ chuyển quy trình sang miền SELinux với **quyền truy cập không hạn chế**, chẳng hạn như `u:r:su:s0`. Thông qua Root Profile, miền này có thể được chuyển sang miền tùy chỉnh, chẳng hạn như `u:r:app1:s0` và một loạt quy tắc có thể được xác định cho miền này:

```sh
type app1
enforce app1
typeattribute app1 mlstrustedsubject
allow app1 * * *
```

Lưu ý rằng quy tắc `allow app1 * * *` chỉ được sử dụng cho mục đích minh họa. Trong thực tế, quy tắc này không nên được sử dụng rộng rãi vì nó không khác nhiều so với chế độ cho phép.

### Escalation

Nếu cấu hình của Root Profile không được đặt đúng cách, một tình huống escalation (leo thang) có thể xảy ra: các hạn chế do Root Profile áp đặt có thể vô tình bị lỗi.

Ví dụ: nếu bạn cấp quyền root cho người dùng shell ADB (đây là trường hợp phổ biến), sau đó bạn cấp quyền root cho một ứng dụng thông thường nhưng định cấu hình cấu hình gốc của nó bằng UID 2000 (là UID của người dùng shell ADB) , ứng dụng có thể có được quyền truy cập root đầy đủ bằng cách thực hiện lệnh `su` hai lần:

1. Lần thực thi `su` đầu tiên phải tuân theo sự thực thi của App Profile và sẽ chuyển sang UID `2000` (adb shell) thay vì `0` (root).
2. Lần thực thi `su` thứ hai, vì UID là `2000` và bạn đã cấp quyền truy cập root cho UID `2000` (adb shell) trong cấu hình, ứng dụng sẽ có toàn quyền root.

:::warning Ghi chú
Hành vi này hoàn toàn được mong đợi và không phải là lỗi. Vì vậy, chúng tôi khuyến nghị như sau:

Nếu bạn thực sự cần cấp quyền root cho ADB (ví dụ: với tư cách là nhà phát triển), bạn không nên thay đổi UID thành `2000` khi định cấu hình Root Profile. Sử dụng `1000` (hệ thống) sẽ là lựa chọn tốt hơn.
:::

## Non-Root Profile

### Umount Modules

KernelSU cung cấp một cơ chế systemless để sửa đổi các phân vùng hệ thống, đạt được thông qua việc gắn overlayfs. Tuy nhiên, một số ứng dụng có thể nhạy cảm với hành vi đó. Do đó, chúng ta có thể dỡ bỏ các mô-đun được gắn trên các ứng dụng này bằng cách đặt tùy chọn "umount modules".

Ngoài ra, giao diện cài đặt của trình quản lý KernelSU cung cấp một công tắc cho "umount modules by default". Theo mặc định, công tắc này được **bật**, có nghĩa là KernelSU hoặc một số mô-đun sẽ hủy tải các mô-đun cho ứng dụng này trừ khi áp dụng cài đặt bổ sung. Nếu bạn không thích cài đặt này hoặc nếu nó ảnh hưởng đến một số ứng dụng nhất định, bạn có các tùy chọn sau:

1. Giữ nút chuyển cho "umount modules by default" và tắt riêng tùy chọn "umount modules" trong App Profile đối với các ứng dụng yêu cầu tải mô-đun (hoạt động như "whitelist").
2. Tắt khóa chuyển cho "umount modules by default" và bật riêng tùy chọn "umount modules" trong App Profile cho các ứng dụng yêu cầu dỡ bỏ mô-đun (hoạt động như "blacklist").

:::info
Trong các thiết bị sử dụng kernel phiên bản 5.10 trở lên, kernel thực hiện việc dỡ tải các mô-đun. Tuy nhiên, đối với các thiết bị chạy phiên bản kernel dưới 5.10, công tắc này chỉ đơn thuần là một tùy chọn cấu hình và bản thân KernelSU không thực hiện bất kỳ hành động nào. Một số mô-đun, chẳng hạn như Zygisksu, có thể sử dụng công tắc này để xác định xem có cần thiết phải dỡ bỏ mô-đun hay không.
:::
