# Metamodule

Metamodules are a revolutionary feature in KernelSU that transfers critical module system capabilities from the core to pluggable modules. This architectural shift maintains KernelSU's stability and security while unleashing greater innovation potential for the module ecosystem.

## What is a Metamodule?

A metamodule is a special type of KernelSU module that provides core infrastructure functionality for the module system. Unlike regular modules that modify system files, metamodules control *how* regular modules are installed and mounted.

Metamodules are a plugin-based extension mechanism that allows complete customization of KernelSU's module management infrastructure. By delegating mounting and installation logic to metamodules, KernelSU avoids being a fragile detection point while enabling diverse implementation strategies.

**Key characteristics:**

- **Infrastructure role**: Metamodules provide services that regular modules depend on
- **Single instance**: Only one metamodule can be installed at a time
- **Priority execution**: Metamodule scripts run before regular module scripts
- **Special hooks**: Provides three hook scripts for installation, mounting, and cleanup

## Why Metamodules?

Traditional root solutions bake mounting logic into their core, making them easier to detect and harder to evolve. KernelSU's metamodule architecture solves these problems through separation of concerns.

**Strategic advantages:**

- **Reduced detection surface**: KernelSU itself doesn't perform mounts, reducing detection vectors
- **Stability**: Core remains stable while mounting implementations can evolve
- **Innovation**: Community can develop alternative mounting strategies without forking KernelSU
- **Choice**: Users can select the implementation that best fits their needs

**Mounting flexibility:**

- **No mounting**: For users with mountless-only modules, avoid mounting overhead entirely
- **OverlayFS mounting**: Traditional approach with read-write layer support (via `meta-overlayfs`)
- **Magic mount**: Magisk-compatible mounting for better app compatibility
- **Custom implementations**: FUSE-based overlays, custom VFS mounts, or entirely new approaches

**Beyond mounting:**

- **Extensibility**: Add features like kernel module support without modifying core KernelSU
- **Modularity**: Update implementations independently of KernelSU releases
- **Customization**: Create specialized solutions for specific devices or use cases

::: warning IMPORTANT
Without a metamodule installed, modules will **NOT** be mounted. Fresh KernelSU installations require installing a metamodule (such as `meta-overlayfs`) for modules to function.
:::

## For Users

### Installing a Metamodule

Install a metamodule the same way as regular modules:

1. Download the metamodule ZIP file (e.g., `meta-overlayfs.zip`)
2. Open KernelSU Manager app
3. Tap the floating action button (➕)
4. Select the metamodule ZIP file
5. Reboot your device

The `meta-overlayfs` metamodule is the official reference implementation that provides traditional overlayfs-based module mounting with ext4 image support.

### Checking Active Metamodule

You can check which metamodule is currently active in the KernelSU Manager app's Module page. The active metamodule will be displayed in your module list with its special designation.

### Uninstalling a Metamodule

::: danger WARNING
Uninstalling a metamodule will affect **ALL** modules. After removal, modules will no longer be mounted until you install another metamodule.
:::

To uninstall:

1. Open KernelSU Manager
2. Find the metamodule in your module list
3. Tap uninstall (you'll see a special warning)
4. Confirm the action
5. Reboot your device

After uninstalling, you should install another metamodule if you want modules to continue working.

### Single Metamodule Constraint

Only one metamodule can be installed at a time. If you try to install a second metamodule, KernelSU will prevent the installation to avoid conflicts.

To switch metamodules:

1. Uninstall all regular modules
2. Uninstall the current metamodule
3. Reboot
4. Install the new metamodule
5. Reinstall your regular modules
6. Reboot again

## For Module Developers

If you're developing regular KernelSU modules, you don't need to worry much about metamodules. Your modules will work as long as users have a compatible metamodule installed (like `meta-overlayfs`).

**What you need to know:**

- **Mounting requires a metamodule**: The `system` directory in your module will only be mounted if the user has a metamodule installed that provides mounting functionality
- **No code changes needed**: Existing modules continue to work without modification

::: tip
If you're familiar with Magisk module development, your modules will work the same way in KernelSU when metamodule is installed, as it provides Magisk-compatible mounting.
:::

## For Metamodule Developers

Creating a metamodule allows you to customize how KernelSU handles module installation, mounting, and uninstallation.

### Basic Requirements

A metamodule is identified by a special property in its `module.prop`:

```txt
id=meta-example
name=My Custom Metamodule
version=1.0
versionCode=1
author=Your Name
description=Custom module mounting implementation
metamodule=1
```

**Key requirements:**

- The `metamodule=1` (or `metamodule=true`) property marks this as a metamodule. Without this property, the module will be treated as a regular module.
- **Naming convention**: It is strongly recommended to name your metamodule ID starting with `meta-` (e.g., `meta-overlayfs`, `meta-magicmount`, `meta-custom`). This helps users easily identify metamodules and prevents naming conflicts with regular modules.

### File Structure

A metamodule structure:

```txt
meta-example/
├── module.prop              (must include metamodule=1)
│
│      *** Metamodule-specific hooks ***
├── metamount.sh             (optional: custom mount handler)
├── metainstall.sh           (optional: installation hook for regular modules)
├── metauninstall.sh         (optional: cleanup hook for regular modules)
│
│      *** Standard module files (all optional) ***
├── customize.sh             (installation customization)
├── post-fs-data.sh          (post-fs-data stage script)
├── service.sh               (late_start service script)
├── boot-completed.sh        (boot completed script)
├── uninstall.sh             (metamodule's own uninstallation script)
└── [any additional files]
```

Metamodules can use all standard module features (lifecycle scripts, etc.) in addition to their special metamodule hooks.

### Hook Scripts

Metamodules can provide up to three special hook scripts:

#### 1. metamount.sh - Mount Handler

**Purpose**: Controls how modules are mounted during boot.

**When executed**: [Execution Order](#execution-order) below.

**Environment variables:**

- `MODDIR`: The metamodule's directory path (e.g., `/data/adb/modules/meta-example`)
- All standard KernelSU environment variables

**Responsibilities:**

- Mount all enabled modules systemlessly
- Check for `skip_mount` flags
- Handle module-specific mounting requirements

::: danger CRITICAL REQUIREMENT
When performing mount operations, you **MUST** set the source/device name to `"KSU"`. This identifies mounts as belonging to KernelSU.

**Example (correct):**

```sh
mount -t overlay -o lowerdir=/lower,upperdir=/upper,workdir=/work KSU /target
```

**For modern mount APIs**, set the source string:

```rust
fsconfig_set_string(fs, "source", "KSU")?;
```

This is essential for KernelSU to identify and manage its mounts properly.
:::

**Example script:**

```sh
#!/system/bin/sh
MODDIR="${0%/*}"

# Example: Simple bind mount implementation
for module in /data/adb/modules/*; do
    if [ -f "$module/disable" ] || [ -f "$module/skip_mount" ]; then
        continue
    fi

    if [ -d "$module/system" ]; then
        # Mount with source=KSU (REQUIRED!)
        mount -o bind,dev=KSU "$module/system" /system
    fi
done
```

#### 2. metainstall.sh - Installation Hook

**Purpose**: Customize how regular modules are installed.

**When executed**: During module installation, after files are extracted but before installation completes. This script is **sourced** (not executed) by the built-in installer, similar to how `customize.sh` works.

**Environment variables and functions:**

This script inherits all variables and functions from the built-in `install.sh`:

- **Variables**: `MODPATH`, `TMPDIR`, `ZIPFILE`, `ARCH`, `API`, `IS64BIT`, `KSU`, `KSU_VER`, `KSU_VER_CODE`, `BOOTMODE`, etc.
- **Functions**:
  - `ui_print <msg>` - Print message to console
  - `abort <msg>` - Print error and terminate installation
  - `set_perm <target> <owner> <group> <permission> [context]` - Set file permissions
  - `set_perm_recursive <directory> <owner> <group> <dirpermission> <filepermission> [context]` - Set permissions recursively
  - `install_module` - Call the built-in module installation process

**Use cases:**

- Process module files before or after built-in installation (call `install_module` when ready)
- Move module files
- Validate module compatibility
- Set up special directory structures
- Initialize module-specific resources

**Note**: This script is **NOT** called when installing the metamodule itself.

#### 3. metauninstall.sh - Cleanup Hook

**Purpose**: Clean up resources when regular modules are uninstalled.

**When executed**: During module uninstallation, before the module directory is removed.

**Environment variables:**

- `MODULE_ID`: The ID of the module being uninstalled

**Use cases:**

- Process files
- Clean up symlinks
- Free allocated resources
- Update internal tracking

**Example script:**

```sh
#!/system/bin/sh
# Called when uninstalling regular modules
MODULE_ID="$1"
IMG_MNT="/data/adb/metamodule/mnt"

# Remove module files from image
if [ -d "$IMG_MNT/$MODULE_ID" ]; then
    rm -rf "$IMG_MNT/$MODULE_ID"
fi
```

### Execution Order {#execution-order}

Understanding the boot execution order is crucial for metamodule development:

```txt
post-fs-data stage:
  1. Common post-fs-data.d scripts execute
  2. Prune modules, restorecon, load sepolicy.rule
  3. Metamodule's post-fs-data.sh executes (if exists)
  4. Regular modules' post-fs-data.sh execute
  5. Load system.prop
  6. Metamodule's metamount.sh executes
     └─> Mounts all modules systemlessly
  7. post-mount.d stage runs
     - Common post-mount.d scripts
     - Metamodule's post-mount.sh (if exists)
     - Regular modules' post-mount.sh

service stage:
  1. Common service.d scripts execute
  2. Metamodule's service.sh executes (if exists)
  3. Regular modules' service.sh execute

boot-completed stage:
  1. Common boot-completed.d scripts execute
  2. Metamodule's boot-completed.sh executes (if exists)
  3. Regular modules' boot-completed.sh execute
```

**Key points:**

- `metamount.sh` runs **AFTER** all post-fs-data scripts (both metamodule and regular modules)
- Metamodule lifecycle scripts (`post-fs-data.sh`, `service.sh`, `boot-completed.sh`) always run before regular module scripts
- Common scripts in `.d` directories run before metamodule scripts
- The `post-mount` stage runs after mounting is complete

### Symlink Mechanism

When a metamodule is installed, KernelSU creates a symlink:

```sh
/data/adb/metamodule -> /data/adb/modules/<metamodule_id>
```

This provides a stable path for accessing the active metamodule, regardless of its ID.

**Benefits:**

- Consistent access path
- Easy detection of active metamodule
- Simplifies configuration

### Real-World Example: meta-overlayfs

The `meta-overlayfs` metamodule is the official reference implementation. It demonstrates best practices for metamodule development.

#### Architecture

`meta-overlayfs` uses a **dual-directory architecture**:

1. **Metadata directory**: `/data/adb/modules/`
   - Contains `module.prop`, `disable`, `skip_mount` markers
   - Fast to scan during boot
   - Small storage footprint

2. **Content directory**: `/data/adb/metamodule/mnt/`
   - Contains actual module files (system, vendor, product, etc.)
   - Stored in an ext4 image (`modules.img`)
   - Space-optimized with ext4 features

#### metamount.sh Implementation

Here's how `meta-overlayfs` implements the mount handler:

```sh
#!/system/bin/sh
MODDIR="${0%/*}"
IMG_FILE="$MODDIR/modules.img"
MNT_DIR="$MODDIR/mnt"

# Mount ext4 image if not already mounted
if ! mountpoint -q "$MNT_DIR"; then
    mkdir -p "$MNT_DIR"
    mount -t ext4 -o loop,rw,noatime "$IMG_FILE" "$MNT_DIR"
fi

# Set environment variables for dual-directory support
export MODULE_METADATA_DIR="/data/adb/modules"
export MODULE_CONTENT_DIR="$MNT_DIR"

# Execute the mount binary
# (The actual mounting logic is in a Rust binary)
"$MODDIR/meta-overlayfs"
```

#### Key Features

**Overlayfs mounting:**

- Uses kernel overlayfs for true systemless modifications
- Supports multiple partitions (system, vendor, product, system_ext, odm, oem)
- Read-write layer support via `/data/adb/modules/.rw/`

**Source identification:**

```rust
// From meta-overlayfs/src/mount.rs
fsconfig_set_string(fs, "source", "KSU")?;  // REQUIRED!
```

This sets `dev=KSU` for all overlay mounts, enabling proper identification.

### Best Practices

When developing metamodules:

1. **Always set source to "KSU"** for mount operations - kernel umount and zygisksu umount need this to umount correctly
2. **Handle errors gracefully** - boot processes are time-sensitive
3. **Respect standard flags** - support `skip_mount` and `disable`
4. **Log operations** - use `echo` or logging for debugging
5. **Test thoroughly** - mounting errors can cause boot loops
6. **Document behavior** - clearly explain what your metamodule does
7. **Provide migration paths** - help users switch from other solutions

### Testing Your Metamodule

Before releasing:

1. **Test installation** on a clean KernelSU setup
2. **Verify mounting** with various module types
3. **Check compatibility** with common modules
4. **Test uninstallation** and cleanup
5. **Validate boot performance** (metamount.sh is blocking!)
6. **Ensure proper error handling** to avoid boot loops

## Frequently Asked Questions

### Do I need a metamodule?

**For users**: Only if you want to use modules that require mounting. If you only use modules that run scripts without modifying system files, you don't need a metamodule.

**For module developers**: No, you develop modules normally. Users need a metamodule only if your module requires mounting.

**For advanced users**: Only if you want to customize mounting behavior or create alternative mounting implementations.

### Can I have multiple metamodules?

No. Only one metamodule can be installed at a time. This prevents conflicts and ensures predictable behavior.

### What happens if I uninstall my only metamodule?

Modules will no longer be mounted. Your device will boot normally, but module modifications won't apply until you install another metamodule.

### Is meta-overlayfs required?

No. It provides standard overlayfs mounting compatible with most modules. You can create your own metamodule if you need different behavior.

## See Also

- [Module Guide](module.md) - General module development
- [Difference with Magisk](difference-with-magisk.md) - Comparing KernelSU and Magisk
- [How to Build](how-to-build.md) - Building KernelSU from source
