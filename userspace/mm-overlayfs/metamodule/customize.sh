#!/system/bin/sh

ui_print "- Detecting device architecture..."

# Detect architecture using ro.product.cpu.abi
ABI=$(grep_get_prop ro.product.cpu.abi)
ui_print "- Detected ABI: $ABI"

# Select the correct binary based on architecture
case "$ABI" in
    arm64-v8a)
        ARCH_BINARY="mm-overlayfs-aarch64"
        REMOVE_BINARY="mm-overlayfs-x86_64"
        ui_print "- Selected architecture: ARM64"
        ;;
    x86_64)
        ARCH_BINARY="mm-overlayfs-x86_64"
        REMOVE_BINARY="mm-overlayfs-aarch64"
        ui_print "- Selected architecture: x86_64"
        ;;
    *)
        abort "! Unsupported architecture: $ABI"
        ;;
esac

# Verify the selected binary exists
if [ ! -f "$MODPATH/$ARCH_BINARY" ]; then
    abort "! Binary not found: $ARCH_BINARY"
fi

ui_print "- Installing $ARCH_BINARY as mm-overlayfs"

# Rename the selected binary to the generic name
mv "$MODPATH/$ARCH_BINARY" "$MODPATH/mm-overlayfs" || abort "! Failed to rename binary"

# Remove the unused binary
rm -f "$MODPATH/$REMOVE_BINARY"

# Ensure the binary is executable
chmod 755 "$MODPATH/mm-overlayfs" || abort "! Failed to set permissions"

ui_print "- Architecture-specific binary installed successfully"

# Create ext4 image for module content storage
IMG_FILE="$MODPATH/modules.img"
MNT_DIR="$MODPATH/mnt"
IMG_SIZE_MB=2048

if [ ! -f "$IMG_FILE" ]; then
    ui_print "- Creating 2GB ext4 image for module storage"

    # Create sparse file (2GB logical size, ~1MB actual)
    dd if=/dev/zero of="$IMG_FILE" bs=1M count=1 seek=$((IMG_SIZE_MB - 1)) 2>/dev/null || \
        abort "! Failed to create image file"

    # Format as ext4 (disable journal for better performance)
    mke2fs -t ext4 -O ^has_journal -F "$IMG_FILE" >/dev/null 2>&1 || \
        abort "! Failed to format ext4 image"

    ui_print "- Image created successfully (sparse file)"
else
    ui_print "- Existing image found, keeping it"
fi

# Mount image immediately for use
ui_print "- Mounting image for immediate use..."
mkdir -p "$MNT_DIR"
if ! mountpoint -q "$MNT_DIR" 2>/dev/null; then
    mount -t ext4 -o loop,rw,noatime "$IMG_FILE" "$MNT_DIR" || \
        abort "! Failed to mount image"
    ui_print "- Image mounted successfully"
else
    ui_print "- Image already mounted"
fi

ui_print "- Installation complete"
ui_print "- Image is ready for module installations"
