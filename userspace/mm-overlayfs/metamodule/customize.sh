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
