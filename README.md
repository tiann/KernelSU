# KernelSU

A Kernel based root solution for Android GKI.

## Usage

1. Flash a custom kernel with KernelSU, you can build it yourself or use prebuilt boot.img.
2. Install Manager App and enjoy :)

## Build

### Build GKI Kernel

1. Put the `kernel` directory to GKI source's `common/drivers`
2. Edit `common/drivers/Makefile` and add the driver to target
3. Follow the [GKI build instruction](https://source.android.com/docs/core/architecture/kernel/generic-kernel-image) and build the kernel.

### Build App

Just open Android Studio and import the project.

## License

[GPL-3](http://www.gnu.org/copyleft/gpl.html)

## Credits

- [kernel-assisted-superuser](https://git.zx2c4.com/kernel-assisted-superuser/about/)
- [genuine](https://github.com/brevent/genuine/)
- [Diamorphine](https://github.com/m0nad/Diamorphine)