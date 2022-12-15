# KernelSU

A Kernel based root solution for Android GKI.

## Usage

1. Flash a custom kernel with KernelSU, you can build it yourself or use prebuilt boot.img.
2. Install Manager App and enjoy :)

## Build

### Build GKI Kernel

1. Download the GKI source first, you can refer the [GKI build instruction](https://source.android.com/docs/setup/build/building-kernels)
2. cd `<GKI kernel source dir>`
3. `curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -`
4. Build the kernel.

### Build the Manager App

Just open Android Studio and import the project.

## License

[GPL-3](http://www.gnu.org/copyleft/gpl.html)

## Credits

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/)
- [genuine](https://github.com/brevent/genuine/)
- [Diamorphine](https://github.com/m0nad/Diamorphine)
