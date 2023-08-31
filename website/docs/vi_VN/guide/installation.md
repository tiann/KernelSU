# Cách cài đặt

## Kiểm tra xem thiết bị của bạn có được hỗ trợ không

Tải xuống APP KernelSU manager từ [GitHub Releases](https://github.com/tiann/KernelSU/releases) hoặc [Coolapk market](https://www.coolapk.com/apk/me.weishu.kernelsu) và cài đặt nó vào thiết bị của bạn:

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

## Install with custom Recovery

Prerequisite: Your device must have a custom Recovery, such as TWRP; if not or only official Recovery is available, use another method.

Step:

1. From the [Release page](https://github.com/tiann/KernelSU/releases) of KernelSU, download the zip package starting with AnyKernel3 that matches your phone version; for example, the phone kernel version is `android12-5.10. 66`, then you should download the file `AnyKernel3-android12-5.10.66_yyyy-MM.zip` (where `yyyy` is the year and `MM` is the month).
2. Reboot the phone into TWRP.
3. Use adb to put AnyKernel3-*.zip into the phone /sdcard and choose to install it in the TWRP GUI; or you can directly `adb sideload AnyKernel-*.zip` to install.

PS. This method is suitable for any installation (not limited to initial installation or subsequent upgrades), as long as you use TWRP.

## Install with Kernel Flasher

Prerequisite: Your device must be rooted. For example, you have installed Magisk to get root, or you have installed an old version of KernelSU and need to upgrade to another version of KernelSU; if your device is not rooted, please try other methods.

Step:

1. Download the AnyKernel3 zip; refer to the section *Installing with Custom Recovery* for downloading instructions.
2. Open the Kernel Flash App and use the provided AnyKernel3 zip to flash.

If you haven't used the Kernel flash App before, the following are the more popular ones.

1. [Kernel Flasher](https://github.com/capntrips/KernelFlasher/releases)
2. [Franco Kernel Manager](https://play.google.com/store/apps/details?id=com.franco.kernel)
3. [Ex Kernel Manager](https://play.google.com/store/apps/details?id=flar2.exkernelmanager)

PS. This method is more convenient when upgrading KernelSU and can be done without a computer (backup first!). .

## Install with boot.img provided by KernelSU

This method does not require you to have TWRP, nor does it require your phone to have root privileges; it is suitable for your first installation of KernelSU.

### Find proper boot.img

KernelSU provides a generic boot.img for GKI devices and you should flush the boot.img to the boot partition of the device.

You can download boot.img from [GitHub Release](https://github.com/tiann/KernelSU/releases), please note that you should use the correct version of boot.img. For example, if your device displays the kernel `android12-5.10.101` , you need to download `android-5.10.101_yyyy-MM.boot-<format>.img`. (Keep KMI consistent!)

Where `<format>` refers to the kernel compression format of your official boot.img, please check the kernel compression format of your original boot.img, you should use the correct format, e.g. `lz4`, `gz`; if you use an incorrect compression format, you may encounter bootloop.

::: info
1. You can use magiskboot to get the compression format of your original boot; of course you can also ask other, more experienced kids with the same model as your device. Also, the compression format of the kernel usually does not change, so if you boot successfully with a certain compression format, you can try that format later.
2. Xiaomi devices usually use `gz` or **uncompressed**.
3. For Pixel devices, follow below instructions.
:::

### flash boot.img to device

Use `adb` to connect your device, then execute `adb reboot bootloader` to enter fastboot mode, then use this command to flash KernelSU:

```sh
fastboot flash boot boot.img
```

::: info
If your device supports `fastboot boot`, you can first use `fastboot boot boot.img` to try to use boot.img to boot the system first. If something unexpected happens, restart it again to boot.
:::

### reboot

After flashing is complete, you should reboot your device:

```sh
fastboot reboot
```

## Patch boot.img manually

For some devices, the boot.img format is not so common, such as not `lz4`, `gz` and uncompressed; the most typical is Pixel, its boot.img format is `lz4_legacy` compressed, ramdisk may be `gz` may also be `lz4_legacy` compression; at this time, if you directly flash the boot.img provided by KernelSU, the phone may not be able to boot; at this time, you can manually patch the boot.img to achieve.

There are generally two patch methods:

1. [Android-Image-Kitchen](https://forum.xda-developers.com/t/tool-android-image-kitchen-unpack-repack-kernel-ramdisk-win-android-linux-mac.2073775/)
2. [magiskboot](https://github.com/topjohnwu/Magisk/releases)

Among them, Android-Image-Kitchen is suitable for operation on PC, and magiskboot needs the cooperation of mobile phone.

### Preparation

1. Get your phone's stock boot.img; you can get it from your device manufacturers, you may need [payload-dumper-go](https://github.com/ssut/payload-dumper-go)
2. Download the AnyKernel3 zip file provided by KernelSU that matches the KMI version of your device (you can refer to the *Install with custom Recovery*).
3. Unpack the AnyKernel3 package and get the `Image` file, which is the kernel file of KernelSU.

### Using Android-Image-Kitchen

1. Download Android-Image-Kitchen to your computer.
2. Put stock boot.img to Android-Image-Kitchen's root folder.
3. Execute `./unpackimg.sh boot.img` at root directory of Android-Image-Kitchen, this command would unpack boot.img and you will get some files.
4. Replace `boot.img-kernel` in the `split_img` directory with the `Image` you extracted from AnyKernel3 (note the name change to boot.img-kernel).
5. Execute `./repackimg.sh` at root directory of 在 Android-Image-Kitchen; And you will get a file named `image-new.img`; Flash this boot.img by fastboot(Refer to the previous section).

### Using magiskboot

1. Download latest Magisk from [Release Page](https://github.com/topjohnwu/Magisk/releases)
2. Rename Magisk-*.apk to Magisk-vesion.zip and unzip it.
3. Push `Magisk-v25.2/lib/arm64-v8a/libmagiskboot.so` to your device by adb: `adb push Magisk-v25.2/lib/arm64-v8a/libmagiskboot.so /data/local/tmp/magiskboot`
4. Push stock boot.img and Image in AnyKernel3 to your device.
5. Enter adb shell and cd `/data/local/tmp/` directory, then `chmod +x magiskboot`
6. Enter adb shell and cd `/data/local/tmp/` directory, execute `./magiskboot unpack boot.img` to unpack `boot.img`, you will get a `kernel` file, this is your stock kernel.
7. Replace `kernel` with `Image`: `mv -f Image kernel`
8. Execute `./magiskboot repack boot.img` to repack boot img, and you will get a `new-boot.img` file, flash this file to device by fastboot.

## Other methods

In fact, all these installation methods have only one main idea, which is to **replace the original kernel for the one provided by KernelSU**; as long as this can be achieved, it can be installed; for example, the following are other possible methods.

1. First install Magisk, get root privileges through Magisk and then use the kernel flasher to flash in the AnyKernel zip from KernelSU.
2. Use some flashing toolkit on PCs to flash in the kernel provided KernelSU.
