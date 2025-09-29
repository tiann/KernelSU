# KSU ModSys Overlay

KernelSU Module System - Overlay Implementation

This is the default module system implementation for KernelSU, implementing the "metamodule" architecture that allows pluggable module system backends.

## Features

- Full module lifecycle management (install, uninstall, enable, disable)
- A/B image update mechanism for safe module updates
- Systemless overlay mounting using overlayfs
- Stage-based script execution (post-fs-data, service, boot-completed)
- Compatible with existing KernelSU Manager apps

## CLI Commands

### Module Management
- `install <zip>` - Install a module from ZIP file
- `uninstall <id>` - Uninstall a module by ID
- `enable <id>` - Enable a module
- `disable <id>` - Disable a module
- `list` - List all modules
- `action <id>` - Run action script for a module
- `shrink` - Shrink module image files

### Stage Execution
- `stage post-fs-data` - Execute post-fs-data stage
- `stage service` - Execute service stage
- `stage boot-completed` - Execute boot-completed stage

### Mounting
- `mount systemless` - Mount modules systemlessly

### Support Check
- `--supported` - Check if this implementation is supported

## Integration

This binary should be placed at `/data/adb/ksu/msp/ksu-modsys-overlay` and will be automatically used by ksud when no other module system is selected.

The selection can be changed by modifying `/data/adb/ksu/modsys.selected`.

## Building

```bash
cd userspace/ksu-modsys-overlay
cargo build --release
```

## Architecture

This implementation maintains backward compatibility with the original KernelSU module system while providing a clean separation between the core KernelSU functionality and module management.
