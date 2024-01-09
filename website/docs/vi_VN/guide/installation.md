# Cách cài đặt

## Kiểm tra xem thiết bị của bạn có được hỗ trợ không

Tải xuống APP KernelSU manager từ [GitHub Releases](https://github.com/tiann/KernelSU/releases) và cài đặt nó vào thiết bị của bạn:

- Nếu ứng dụng hiển thị `Unsupported`, nghĩa là **Bạn nên tự biên dịch kernel**, KernelSU sẽ không và không bao giờ cung cấp boot image để bạn flash.
- Nếu ứng dụng hiển thị `Not installed` thì thiết bị của bạn đã được KernelSU hỗ trợ chính thức.

:::info
Đối với các thiết bị hiển thị `Unsupported`, đây là [Thiết-bị-hỗ-trợ-không-chính-thức](unofficially-support-devices.md), bạn có thể tự biên dịch kernel.
:::

## Sao lưu stock boot.img

Trước khi flash, trước tiên bạn phải sao lưu stock boot.img. Nếu bạn gặp phải bootloop (vòng lặp khởi động), bạn luôn có thể khôi phục hệ thống bằng cách quay lại trạng thái khởi động ban đầu bằng fastboot.

::: warning
Việc flash có thể gây mất dữ liệu, hãy đảm bảo thực hiện tốt bước này trước khi chuyển sang bước tiếp theo!! Bạn cũng có thể sao lưu tất cả dữ liệu trên điện thoại nếu cần.
:::

## Kiến thức cần thiết

### ADB và fastboot

Theo mặc định, bạn sẽ sử dụng các công cụ ADB và fastboot trong hướng dẫn này, vì vậy nếu bạn không biết về chúng, chúng tôi khuyên bạn nên sử dụng công cụ tìm kiếm để tìm hiểu về chúng trước tiên.

### KMI

Kernel Module Interface (KMI), các phiên bản kernel có cùng KMI đều **tương thích** Đây là ý nghĩa của "general" trong GKI; ngược lại, nếu KMI khác thì các kernel này không tương thích với nhau và việc flash kernel image có KMI khác với thiết bị của bạn có thể gây ra bootloop.

Cụ thể, đối với thiết bị GKI, định dạng phiên bản kernel phải như sau:

```txt
KernelRelease :=
Version.PatchLevel.SubLevel-AndroidRelease-KmiGeneration-suffix
w      .x         .y       -zzz           -k            -something
```

`w.x-zzz-k` là phiên bản KMI. Ví dụ: nếu phiên bản kernel của thiết bị là `5.10.101-android12-9-g30979850fc20`, thì KMI của nó là `5.10-android12-9`; về mặt lý thuyết, nó có thể khởi động bình thường với các kernel KMI khác.

::: tip
Lưu ý rằng SubLevel trong phiên bản kernel không phải là một phần của KMI! Điều đó có nghĩa là `5.10.101-android12-9-g30979850fc20` có cùng KMI với `5.10.137-android12-9-g30979850fc20`!
:::

### Phiên bản kernel vs Phiên bản Android

Xin lưu ý: **Phiên bản kernel và phiên bản Android không nhất thiết phải giống nhau!**

Nếu bạn nhận thấy phiên bản kernel của mình là `android12-5.10.101` nhưng phiên bản hệ thống Android của bạn là Android 13 hoặc phiên bản khác; xin đừng ngạc nhiên, vì số phiên bản của hệ thống Android không nhất thiết phải giống với số phiên bản của kernel Linux; Số phiên bản của kernel Linux nhìn chung nhất quán với phiên bản của hệ thống Android đi kèm với **thiết bị khi nó được xuất xưởng**. Nếu hệ thống Android được nâng cấp sau này, phiên bản kernel thường sẽ không thay đổi. Nếu bạn cần flash, **vui lòng tham khảo phiên bản kernel!!**

## Giới thiệu

Có một số phương pháp cài đặt KernelSU, mỗi phương pháp phù hợp với một kịch bản khác nhau, vì vậy vui lòng chọn khi cần.

1. Cài đặt với Recovery tùy chỉnh (ví dụ TWRP)
2. Cài đặt bằng ứng dụng flash kernel, chẳng hạn như Franco Kernel Manager
3. Cài đặt thông qua fastboot bằng boot.img do KernelSU cung cấp
4. Sửa boot.img theo cách thủ công và cài đặt nó

## Cài đặt với Recovery tùy chỉnh

Điều kiện chắc chắn: Thiết bị của bạn phải có Recovery tùy chỉnh, chẳng hạn như TWRP; nếu không hoặc chỉ có Recovery chính thức, hãy sử dụng phương pháp khác.

Các bước:

1. Từ [Release page](https://github.com/tiann/KernelSU/releases) của KernelSU, tải xuống gói zip bắt đầu bằng AnyKernel3 phù hợp với phiên bản điện thoại của bạn; ví dụ: phiên bản kernel của điện thoại là `android12-5.10. 66`, thì bạn nên tải xuống tệp `AnyKernel3-android12-5.10.66_yyyy-MM.zip` (trong đó `yyyy` là năm và `MM` là tháng).
2. Khởi động lại điện thoại vào TWRP.
3. Sử dụng adb để đặt AnyKernel3-*.zip vào điện thoại /sdcard và chọn cài đặt nó trong GUI TWRP; hoặc bạn có thể trực tiếp `adb sideload AnyKernel-*.zip` để cài đặt.

PS. Phương pháp này phù hợp với mọi cài đặt (không giới hạn cài đặt ban đầu hoặc các nâng cấp tiếp theo), miễn là bạn sử dụng TWRP.

## Cài đặt bằng Kernel Flasher

Điều kiện chắc chắn: Thiết bị của bạn phải được root. Ví dụ: bạn đã cài đặt Magisk để root hoặc bạn đã cài đặt phiên bản KernelSU cũ và cần nâng cấp lên phiên bản KernelSU khác; nếu thiết bị của bạn chưa được root, vui lòng thử các phương pháp khác.

Các bước:

1. Tải xuống zip AnyKernel3; hãy tham khảo phần *Cài đặt bằng Custom Recovery* để biết hướng dẫn tải xuống.
2. Mở Ứng dụng Kernel Flash và sử dụng zip AnyKernel3 được cung cấp để flash.

Nếu trước đây bạn chưa từng sử dụng Ứng dụng Kernel flash thì sau đây là những ứng dụng phổ biến hơn.

1. [Kernel Flasher](https://github.com/capntrips/KernelFlasher/releases)
2. [Franco Kernel Manager](https://play.google.com/store/apps/details?id=com.franco.kernel)
3. [Ex Kernel Manager](https://play.google.com/store/apps/details?id=flar2.exkernelmanager)

PS. Phương pháp này thuận tiện hơn khi nâng cấp KernelSU và có thể thực hiện mà không cần máy tính (sao lưu trước!). .

Các bước:

## Cài đặt bằng boot.img do KernelSU cung cấp

Phương pháp này không yêu cầu bạn phải có TWRP, cũng như không yêu cầu điện thoại của bạn phải có quyền root; nó phù hợp cho lần cài đặt KernelSU đầu tiên của bạn.

### Tìm boot.img thích hợp

KernelSU cung cấp boot.img chung cho các thiết bị GKI và bạn nên chuyển boot.img vào phân vùng boot của thiết bị.

Bạn có thể tải xuống boot.img từ [GitHub Release](https://github.com/tiann/KernelSU/releases), xin lưu ý rằng bạn nên sử dụng đúng phiên bản boot.img. Ví dụ: nếu thiết bị của bạn hiển thị kernel `android12-5.10.101` , bạn cần tải xuống `android-5.10.101_yyyy-MM.boot-<format>.img`. (Giữ KMI nhất quán!)

Trong đó `<format>` đề cập đến định dạng nén kernel của boot.img chính thức của bạn, vui lòng kiểm tra định dạng nén kernel của boot.img ban đầu của bạn, bạn nên sử dụng đúng định dạng, ví dụ: `lz4`, `gz`; nếu bạn sử dụng định dạng nén không chính xác, bạn có thể gặp phải bootloop.

::: info
1. Bạn có thể sử dụng magiskboot để lấy định dạng nén của boot ban đầu; Tất nhiên, bạn cũng có thể hỏi những người khác, có kinh nghiệm hơn có cùng kiểu máy với thiết bị của bạn. Ngoài ra, định dạng nén của kernel thường không thay đổi nên nếu bạn khởi động thành công với một định dạng nén nào đó thì bạn có thể thử định dạng đó sau.
2. Các thiết bị Xiaomi thường sử dụng `gz` hoặc **uncompressed** (không nén).
3. Đối với thiết bị Pixel, hãy làm theo hướng dẫn bên dưới.
:::

### flash boot.img vào thiết bị

Sử dụng `adb` để kết nối thiết bị của bạn, sau đó thực thi `adb restart bootloader` để vào chế độ fastboot, sau đó sử dụng lệnh này để flash KernelSU:

```sh
fastboot flash boot boot.img
```

::: info
Nếu thiết bị của bạn hỗ trợ `fastboot boot`, trước tiên bạn có thể sử dụng `fastboot boot boot.img` để thử sử dụng boot.img để khởi động hệ thống trước. Nếu có điều gì bất ngờ xảy ra, hãy khởi động lại để boot.
:::

### khởi động lại

Sau khi flash xong bạn nên khởi động lại máy:

```sh
fastboot reboot
```

## Vá boot.img theo cách thủ công

Đối với một số thiết bị, định dạng boot.img không quá phổ biến, chẳng hạn như không `lz4`, `gz` và không nén; điển hình nhất là Pixel, định dạng boot.img của nó là nén `lz4_legacy`, ramdisk có thể là `gz` cũng có thể là nén `lz4_legacy`; tại thời điểm này, nếu bạn trực tiếp flash boot.img do KernelSU cung cấp, điện thoại có thể không khởi động được; Tại thời điểm này, bạn có thể vá boot.img theo cách thủ công để dùng được.

Nhìn chung có hai phương pháp vá:

1. [Android-Image-Kitchen](https://forum.xda-developers.com/t/tool-android-image-kitchen-unpack-repack-kernel-ramdisk-win-android-linux-mac.2073775/)
2. [magiskboot](https://github.com/topjohnwu/Magisk/releases)

Trong số đó, Android-Image-Kitchen phù hợp để hoạt động trên PC và magiskboot cần sự kết nối của điện thoại di động.

### Chuẩn bị

1. Lấy stock boot.img của điện thoại; bạn có thể lấy nó từ nhà sản xuất thiết bị của mình, bạn có thể cần [payload-dumper-go](https://github.com/ssut/payload-dumper-go)
2. Tải xuống tệp zip AnyKernel3 do KernelSU cung cấp phù hợp với phiên bản KMI của thiết bị của bạn (bạn có thể tham khảo *Cài đặt với Khôi phục tùy chỉnh*).
3. Giải nén gói AnyKernel3 và lấy tệp `Image`, đây là tệp kernel của KernelSU.

### Sử dụng Android-Image-Kitchen

1. Tải Android-Image-Kitchen về máy tính.
2. Đặt stock boot.img vào thư mục gốc của Android-Image-Kitchen.
3. Thực thi `./unpackimg.sh boot.img` tại thư mục gốc của Android-Image-Kitchen, lệnh này sẽ giải nén boot.img và bạn sẽ nhận được một số tệp.
4. Thay thế `boot.img-kernel` trong thư mục `split_img` bằng `Image` bạn đã trích xuất từ AnyKernel3 (lưu ý đổi tên thành boot.img-kernel).
5. Thực thi `./repackimg.sh` tại thư mục gốc của 在 Android-Image-Kitchen; Và bạn sẽ nhận được một file có tên `image-new.img`; Flash boot.img này bằng fastboot(Tham khảo phần trước).

### Sử dụng magiskboot

1. Tải xuống Magisk mới nhất từ [Trang phát hành](https://github.com/topjohnwu/Magisk/releases)
2. Đổi tên `Magisk-*(version).apk` thành `Magisk-*.zip` và giải nén nó.
3. Đẩy `Magisk-*/lib/arm64-v8a/libmagiskboot.so` vào thiết bị của bạn bằng adb: `adb push Magisk-*/lib/arm64-v8a/libmagiskboot.so /data/local/tmp /magiskboot`
4. Đẩy stock boot.img và Image trong AnyKernel3 vào thiết bị của bạn.
5. Nhập thư mục adb shell và cd `/data/local/tmp/`, sau đó `chmod +x magiskboot`
6. Nhập adb shell và cd `/data/local/tmp/`, thực thi `./magiskboot unpack boot.img` để giải nén `boot.img`, bạn sẽ nhận được file `kernel`, đây là kernel gốc của bạn.
7. Thay thế `kernel` bằng `Image`: `mv -f Image kernel`
8. Thực thi `./magiskboot repack boot.img` để đóng gói lại boot img và bạn sẽ nhận được một tệp `new-boot.img`, flash tệp này vào thiết bị bằng fastboot.

## Các phương pháp khác

Trên thực tế, tất cả các phương pháp cài đặt này chỉ có một ý tưởng chính, đó là **thay thế kernel gốc bằng kernel do KernelSU cung cấp**; chỉ cần đạt được điều này là có thể cài đặt được; ví dụ, sau đây là các phương pháp có thể khác.

1. Trước tiên hãy cài đặt Magisk, nhận quyền root thông qua Magisk, sau đó sử dụng flasher kernel để flash trong zip AnyKernel từ KernelSU.
2. Sử dụng một số bộ công cụ flash trên PC để flash trong kernel do KernelSU cung cấp.
