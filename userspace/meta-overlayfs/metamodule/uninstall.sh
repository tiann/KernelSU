#!/system/bin/sh
############################################
# mm-overlayfs uninstall.sh
# Cleanup script for metamodule removal
############################################

MODDIR="${0%/*}"
MNT_DIR="$MODDIR/mnt"

echo "- Uninstalling metamodule..."

# Unmount the ext4 image if mounted
if mountpoint -q "$MNT_DIR" 2>/dev/null; then
    echo "- Unmounting image..."
    umount "$MNT_DIR" 2>/dev/null || {
        echo "- Warning: Failed to unmount cleanly"
        umount -l "$MNT_DIR" 2>/dev/null
    }
    echo "- Image unmounted"
fi

echo "- Uninstall complete"

exit 0
