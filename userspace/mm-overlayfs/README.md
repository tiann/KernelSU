# mm-overlayfs

Official overlayfs mount handler for KernelSU metamodules.

## Installation

```bash
adb push mm-overlayfs-v1.0.0.zip /sdcard/
adb shell su -c 'ksud metamodule install /sdcard/mm-overlayfs-v1.0.0.zip'
adb reboot
```

Or install via KernelSU Manager → Metamodules.

## How It Works

Uses dual-directory architecture for ext4 image support:

- **Metadata**: `/data/adb/modules/` - Contains `module.prop`, `disable`, `skip_mount` markers
- **Content**: `/data/adb/metamodule/mnt/` - Contains `system/`, `vendor/` etc. directories from ext4 images

Scans metadata directory for enabled modules, then mounts their content directories as overlayfs layers.

### Supported Partitions

system, vendor, product, system_ext, odm, oem

### Read-Write Layer

Optional upperdir/workdir support via `/data/adb/modules/.rw/`:

```bash
mkdir -p /data/adb/modules/.rw/system/{upperdir,workdir}
```

## Environment Variables

- `MODULE_METADATA_DIR` - Metadata location (default: `/data/adb/modules/`)
- `MODULE_CONTENT_DIR` - Content location (default: `/data/adb/metamodule/mnt/`)
- `RUST_LOG` - Log level (debug, info, warn, error)

## Architecture

Automatically selects aarch64 or x86_64 binary during installation (~500KB).

## Building

```bash
./build.sh
```

Output: `target/mm-overlayfs-v1.0.0.zip`

## License

GPL-3.0
