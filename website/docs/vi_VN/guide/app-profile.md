# App Profile

App Profile là một cơ chế do KernelSU cung cấp để tùy chỉnh cấu hình của các ứng dụng khác nhau.

Đối với các ứng dụng được cấp quyền root (tức là có thể sử dụng `su`), App Profile cũng có thể được gọi là Root Profile. Nó cho phép tùy chỉnh các quy tắc `uid`, `gid`, `groups`, `capabilities` và `SELinux` của lệnh `su`, do đó hạn chế các đặc quyền của người dùng root. Ví dụ: nó có thể chỉ cấp quyền mạng cho các ứng dụng tường lửa trong khi từ chối quyền truy cập tệp hoặc có thể cấp quyền shell thay vì quyền truy cập root đầy đủ cho các ứng dụng đóng băng: **giữ nguồn điện theo nguyên tắc đặc quyền tối thiểu.**

Đối với các ứng dụng thông thường không có quyền root, App Profile có thể kiểm soát hành vi của hệ thống kernel và mô-đun đối với các ứng dụng này. Ví dụ, nó có thể xác định liệu các sửa đổi do mô-đun tạo ra có nên được giải quyết hay không. Hệ thống kernel và mô-đun có thể đưa ra quyết định dựa trên cấu hình này, chẳng hạn như thực hiện các hoạt động tương tự như "hiding"

## Root Profile

### UID, GID, và Groups

Hệ thống Linux có hai khái niệm: người dùng (user) và nhóm (group). Mỗi người dùng có một user ID (UID) và một người dùng có thể thuộc nhiều nhóm, mỗi nhóm có group ID (GID) riêng. Những ID này được sử dụng để xác định người dùng trong hệ thống và xác định tài nguyên hệ thống nào họ có thể truy cập.

Người dùng có UID bằng 0 được gọi là người dùng root và các nhóm có GID bằng 0 được gọi là nhóm gốc. Nhóm người dùng root thường giữ các đặc quyền hệ thống cao nhất.

Trong trường hợp hệ thống Android, mỗi ứng dụng là một người dùng riêng biệt (không bao gồm các trường hợp UID dùng chung) với một UID duy nhất. Ví dụ: `0` đại diện cho người dùng root, `1000` đại diện cho `system`, `2000` đại diện cho ADB shell và các UID từ 10000 đến 19999 đại diện cho các ứng dụng thông thường.

:::info
Ở đây, UID được đề cập không giống với khái niệm nhiều người dùng hoặc hồ sơ công việc trong hệ thống Android. Hồ sơ công việc thực sự được triển khai bằng cách phân vùng phạm vi UID. Ví dụ: 10000-19999 đại diện cho người dùng chính, trong khi 110000-119999 đại diện cho hồ sơ công việc. Mỗi ứng dụng thông thường trong số đó đều có UID riêng.
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

### Khả năng

Khả năng là một cơ chế phân tách đặc quyền trong Linux.

Với mục đích thực hiện kiểm tra quyền, việc triển khai UNIX truyền thống phân biệt hai loại quy trình: quy trình đặc quyền (có ID người dùng hiệu quả là 0, được gọi là siêu người dùng hoặc root) và quy trình không có đặc quyền (có UID hiệu dụng khác 0). Các quy trình đặc quyền bỏ qua tất cả các bước kiểm tra quyền của kernel, trong khi các quy trình không có đặc quyền phải chịu sự kiểm tra quyền đầy đủ dựa trên thông tin xác thực của quy trình (thường là: UID hiệu quả, GID hiệu quả và danh sách nhóm bổ sung).

Bắt đầu với Linux 2.2, Linux chia các đặc quyền truyền thống được liên kết với siêu người dùng thành các đơn vị riêng biệt, được gọi là các khả năng, có thể được bật và tắt một cách độc lập.

Mỗi Khả năng đại diện cho một hoặc nhiều đặc quyền. Ví dụ: `CAP_DAC_READ_SEARCH` thể hiện khả năng bỏ qua việc kiểm tra quyền để đọc tệp cũng như quyền đọc và thực thi thư mục. Nếu người dùng có UID hiệu dụng là `0` (người dùng root) thiếu khả năng `CAP_DAC_READ_SEARCH` hoặc cao hơn, điều này có nghĩa là ngay cả khi họ là root, họ không thể tùy ý đọc tệp.

Cấu hình gốc của KernelSU cho phép tùy chỉnh các Khả năng của tiến trình gốc sau khi thực thi `su`, nhờ đó đạt được việc cấp một phần "quyền root". Không giống như UID và GID đã nói ở trên, một số ứng dụng gốc nhất định yêu cầu UID là `0` sau khi sử dụng `su`. Trong những trường hợp như vậy, việc giới hạn Khả năng của người dùng root này bằng UID `0` có thể hạn chế các hoạt động được phép của họ.

:::tip Strong Recommendation
Linux's Capability [official documentation](https://man7.org/linux/man-pages/man7/capabilities.7.html) provides detailed explanations of the abilities represented by each Capability. If you intend to customize Capabilities, it is strongly recommended that you read this document first.
:::

### SELinux

SELinux is a powerful Mandatory Access Control (MAC) mechanism. It operates on the principle of **default deny**: any action not explicitly allowed is denied.

SELinux can run in two global modes:

1. Permissive mode: Denial events are logged but not enforced.
2. Enforcing mode: Denial events are logged and enforced.

:::warning Warning
Modern Android systems heavily rely on SELinux to ensure overall system security. It is highly recommended not to use any custom systems running in "permissive mode" since it provides no significant advantages over a completely open system.
:::

Explaining the full concept of SELinux is complex and beyond the scope of this document. It is recommended to first understand its workings through the following resources:

1. [Wikipedia](https://en.wikipedia.org/wiki/Security-Enhanced_Linux)
2. [Red Hat: What Is SELinux?](https://www.redhat.com/en/topics/linux/what-is-selinux)
3. [ArchLinux: SELinux](https://wiki.archlinux.org/title/SELinux)

KernelSU's Root Profile allows customization of the SELinux context of the root process after executing `su`. Specific access control rules can be set for this context to enable fine-grained control over root permissions.

In typical scenarios, when an app executes `su`, it switches the process to a SELinux domain with **unrestricted access**, such as `u:r:su:s0`. Through the Root Profile, this domain can be switched to a custom domain, such as `u:r:app1:s0`, and a series of rules can be defined for this domain:

```sh
type app1
enforce app1
typeattribute app1 mlstrustedsubject
allow app1 * * *
```

Note that the `allow app1 * * *` rule is used for demonstration purposes only. In practice, this rule should not be used extensively since it doesn't differ much from permissive mode.

### Escalation

If the configuration of the Root Profile is not set properly, an escalation scenario may occur: the restrictions imposed by the Root Profile can unintentionally fail.

For example, if you grant root permission to an ADB shell user (which is a common case), and then you grant root permission to a regular application but configure its root profile with UID 2000 (which is the UID of the ADB shell user), the application can obtain full root access by executing the `su` command twice:

1. The first `su` execution is subject to the enforcement of the App Profile and will switch to UID `2000` (adb shell) instead of `0` (root).
2. The second `su` execution, since the UID is `2000`, and you have granted root access to the UID `2000` (adb shell) in the configuration, the application will gain full root privileges.

:::warning Note
This behavior is entirely expected and not a bug. Therefore, we recommend the following:

If you genuinely need to grant root permissions to ADB (e.g., as a developer), it is not advisable to change the UID to `2000` when configuring the Root Profile. Using `1000` (system) would be a better choice.
:::

## Non-Root Profile

### Umount Modules

KernelSU provides a systemless mechanism for modifying system partitions, achieved through overlayfs mounting. However, some apps may be sensitive to such behavior. Thus, we can unload modules mounted on these apps by setting the "umount modules" option.

Additionally, the settings interface of the KernelSU manager provides a switch for "umount modules by default". By default, this switch is **enabled**, which means that KernelSU or some modules will unload modules for this app unless additional settings are applied. If you do not prefer this setting or if it affects certain apps, you have the following options:

1. Keep the switch for "umount modules by default" and individually disable the "umount modules" option in the App Profile for apps requiring module loading (acting as a "whitelist").
2. Disable the switch for "umount modules by default" and individually enable the "umount modules" option in the App Profile for apps requiring module unloading (acting as a "blacklist").

:::info
In devices using kernel version 5.10 and above, the kernel performs the unloading of modules. However, for devices running kernel versions below 5.10, this switch is merely a configuration option, and KernelSU itself does not take any action. Some modules, such as Zygisksu, may use this switch to determine whether module unloading is necessary.
:::
