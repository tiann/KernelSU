# Làm thế nào để xây dựng KernelSU?

Trước tiên, bạn nên đọc tài liệu chính thức của Android để xây dựng kernel:

1. [Building Kernels](https://source.android.com/docs/setup/build/building-kernels?hl=vi)
2. [GKI Release Builds](https://source.android.com/docs/core/architecture/kernel/gki-release-builds?hl=vi)

::: warning
Trang này dành cho thiết bị GKI, nếu bạn sử dụng kernel cũ, vui lòng tham khảo [cách tích hợp KernelSU cho kernel cũ](how-to-integrate-for-non-gki)
:::

## Build Kernel

### Đồng bộ hóa mã nguồn kernel

```sh
repo init -u https://android.googlesource.com/kernel/manifest
mv <kernel_manifest.xml> .repo/manifests
repo init -m manifest.xml
repo sync
```

`<kernel_manifest.xml>` là một tệp kê khai có thể xác định duy nhất một bản dựng, bạn có thể sử dụng tệp kê khai đó để thực hiện một bản dựng có thể dự đoán lại. Bạn nên tải xuống tệp kê khai từ [Google GKI release builds](https://source.android.com/docs/core/architecture/kernel/gki-release-builds?hl=vi)

### Build

Trước tiên, vui lòng kiểm tra [tài liệu chính thức](https://source.android.com/docs/setup/build/building-kernels?hl=vi).

Ví dụ: chúng ta cần xây dựng kernel image aarch64:

```sh
LTO=thin BUILD_CONFIG=common/build.config.gki.aarch64 build/build.sh
```

Đừng quên thêm cờ `LTO=thin`, nếu không quá trình xây dựng có thể thất bại nếu bộ nhớ máy tính của bạn nhỏ hơn 24Gb.

Bắt đầu từ Android 13, kernel được xây dựng bởi `bazel`:

```sh
tools/bazel build --config=fast //common:kernel_aarch64_dist
```

## Build Kernel với KernelSU

Nếu bạn có thể build kernel thành công thì việc xây dựng KernelSU thật dễ dàng, Chọn bất kỳ một lần chạy trong thư mục gốc nguồn Kernel:

- Thẻ mới nhất (ổn định)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -
```

- nhánh chính (dev)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s main
```

- Chọn thẻ (chẳng hạn như v0.5.2)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s v0.5.2
```

Và sau đó build lại kernel và bạn sẽ có được image kernel với KernelSU!
