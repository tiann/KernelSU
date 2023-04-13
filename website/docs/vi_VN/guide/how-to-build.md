# Làm cách nào để build KernelSU ?

Trước tiên, bạn nên đọc tài liệu Chính thức của Android để xây dựng kernel:

1. [Building Kernels](https://source.android.com/docs/setup/build/building-kernels)
2. [GKI Release Builds](https://source.android.com/docs/core/architecture/kernel/gki-release-builds)

> Trang này dành cho thiết bị GKI, nếu bạn dùng kernel cũ, vui lòng tham khảo [Làm thế nào để tích hợp KernelSU vào thiết bị không sử dụng GKI ?](how-to-integrate-for-non-gki)

## Build Kernel

### Đồng bộ mã nguồn

```sh
repo init -u https://android.googlesource.com/kernel/manifest
mv <kernel_manifest.xml> .repo/manifests
repo init -m manifest.xml
repo sync
```

The `<kernel_manifest.xml>` is a manifest file which can determine a build uniquely, you can use the manifest to do a re-preducable build. You should download the manifest file from [Google GKI release builds](https://source.android.com/docs/core/architecture/kernel/gki-release-builds)

`<kernel_manifest.xml>` là một tệp kê khai có thể xác định duy nhất một bản dựng, bạn có thể sử dụng tệp kê khai để thực hiện một bản dựng có thể tái sản xuất. Bạn nên tải xuống tệp kê khai từ [Google GKI release builds](https://source.android.com/docs/core/architecture/kernel/gki-release-builds)

### Build

Vui lòng kiểm tra [tài liệu chính thức](https://source.android.com/docs/setup/build/building-kernels) trước.

Ví dụ: Đầu tiên chúng ta cần build một image cho aarch64:

```sh
LTO=thin BUILD_CONFIG=common/build.config.gki.aarch64 build/build.sh
```

Đừng quên thêm `LTO=thin`, nếu không quá trình xây dựng có thể thất bại trong trường hợp bộ nhớ máy tính của bạn nhỏ hơn 24Gb.

Bắt đầu từ Android 13, kernel được xây dựng bởi `bazel`:

```sh
tools/bazel build --config=fast //common:kernel_aarch64_dist
```

## Build kernel cùng với KernelSU

Nếu bạn có thể build được kernel hoàn chỉnh, thì việc tích hợp KernelSU rất dễ dàng, chạy lệnh sau tại thư mục chứa mã nguồn kernel:

- Latest tag(stable)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -
```

- main branch(dev)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s main
```

- Select tag(Such as v0.5.2)

```sh
curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -s v0.5.2
```

Và rồi build lại, bạn sẽ có được một image chứa KernelSU