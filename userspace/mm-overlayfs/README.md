# KernelSU Official Mount Handler

Official overlayfs mount implementation for KernelSU modules.

## Overview

This metamodule provides the core systemless mounting functionality for KernelSU, using overlayfs to mount module modifications. It replaces the built-in mounting logic that was previously part of ksud, allowing for faster iteration and easier customization.

## Features

- ✅ Complete overlayfs mounting support
- ✅ Multi-partition support (system, vendor, product, system_ext, odm, oem)
- ✅ Child mount point handling
- ✅ System read-write layer support
- ✅ Optimized for size and performance
- ✅ Multi-architecture support (aarch64, x86_64)

## Installation

### Via KernelSU Manager

1. Download `ksu-official-mount-v1.0.0.zip` from releases
2. Open KernelSU Manager
3. Go to Metamodules section
4. Install the downloaded ZIP
5. Reboot your device

### Via Command Line

```bash
adb push ksu-official-mount-v1.0.0.zip /sdcard/
adb shell su -c 'ksud metamodule install /sdcard/ksu-official-mount-v1.0.0.zip'
adb reboot
```

## Architecture Support

- ✅ aarch64 (arm64-v8a) - Most modern Android devices
- ✅ x86_64 - Emulators and x86 devices

**Note**: The correct architecture binary is automatically selected during installation. Only the required binary (~500KB) is kept on your device, saving storage space.

## How It Works

This metamodule performs the following operations during boot:

1. **Module Discovery**: Traverses `/data/adb/modules/` to find enabled modules
2. **Filter Modules**: Skips modules marked as `disable` or `skip_mount`
3. **Construct Layers**: Builds overlayfs lower directories from module contents
4. **Mount Partitions**: Uses overlayfs to mount modifications systemlessly
5. **Handle Submounts**: Processes child mount points correctly

### Mount Process

```
/data/adb/modules/
├── module1/
│   └── system/         → Lower layer 1
├── module2/
│   └── system/         → Lower layer 2
└── ...

                ↓ mount_overlayfs

/system (overlayfs)
├── lowerdir=module2:module1:/system
└── upperdir=.rw/system/upperdir (if exists)
```

## Environment Variables

- `MODULE_DIR`: Module directory path (default: `/data/adb/modules/`)
- `RUST_LOG`: Log level for debugging (debug, info, warn, error)

### Example

```bash
# Run with debug logging
RUST_LOG=debug MODULE_DIR=/custom/path /data/adb/metamodule/mm-overlayfs
```

## Configuration

### Disable a Module

Create a `disable` file in the module directory:

```bash
touch /data/adb/modules/my-module/disable
```

### Skip Mount for a Module

Create a `skip_mount` file:

```bash
touch /data/adb/modules/my-module/skip_mount
```

### Enable Read-Write Mode

Create the `.rw` directory for system modifications:

```bash
mkdir -p /data/adb/modules/.rw/system/{upperdir,workdir}
```

## Building

See [BUILD.md](BUILD.md) for build instructions.

## Troubleshooting

### Modules not mounting

1. Check if the metamodule is installed:
   ```bash
   ksud metamodule list
   ```

2. Check logs:
   ```bash
   adb logcat | grep ksu-metamodule
   ```

3. Verify module structure:
   ```bash
   ls -la /data/adb/modules/your-module/system/
   ```

### Architecture mismatch

Ensure the correct binary is being used:

```bash
uname -m  # Check your device architecture
```

## Technical Details

### File Structure

**Before installation (in ZIP)**:
```
ksu-official-mount/
├── module.prop                  # Metamodule metadata
├── module-mount.sh              # Entry point script
├── customize.sh                 # Installation script (selects architecture)
├── mm-overlayfs-aarch64        # ARM64 binary
└── mm-overlayfs-x86_64         # x86_64 binary
```

**After installation (on device)**:
```
/data/adb/metamodule/
├── module.prop                  # Metamodule metadata
├── module-mount.sh              # Entry point script
├── customize.sh                 # Installation script
└── mm-overlayfs                # Selected architecture binary
```

### Code Organization

- `src/main.rs` - CLI entry point
- `src/mount.rs` - Core mounting logic
- `src/defs.rs` - Constants and definitions

## Comparison with Built-in Implementation

| Feature | Built-in (Old) | Metamodule (New) |
|---------|---------------|------------------|
| Update method | Rebuild ksud | Install new ZIP |
| Customization | Modify ksud source | Fork this project |
| Size | Part of ksud | ~500KB standalone |
| Performance | Direct call | Shell → Binary (negligible overhead) |

## Development

### Project Structure

```
userspace/ksu-metamodule/
├── src/
│   ├── main.rs          # CLI implementation
│   ├── mount.rs         # Mount logic
│   └── defs.rs          # Constants
├── metamodule/
│   ├── module.prop      # Metadata
│   └── module-mount.sh  # Entry script
├── Cargo.toml           # Rust configuration
├── build.sh             # Build script
└── README.md            # This file
```

### Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly on real devices
5. Submit a pull request

## License

GPL-3.0 - Same as KernelSU

## Credits

- Based on the original implementation in KernelSU ksud
- Maintained by the KernelSU Team

## Related Documentation

- [Metamodule Architecture](../../docs/design/metamodule-architecture.md)
- [Implementation Design](../../docs/design/official-metamodule-implementation.md)
- [KernelSU Documentation](https://kernelsu.org)
