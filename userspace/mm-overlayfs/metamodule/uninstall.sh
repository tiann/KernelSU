#!/system/bin/sh
############################################
# mm-overlayfs uninstall.sh
# Cleanup script for metamodule removal
############################################

MODDIR="${0%/*}"
IMG_FILE="$MODDIR/modules.img"
MNT_DIR="$MODDIR/mnt"

echo "[mm-overlayfs] Uninstalling metamodule"

# Unmount the ext4 image if mounted
if mountpoint -q "$MNT_DIR" 2>/dev/null; then
    echo "[mm-overlayfs] Unmounting image..."
    umount "$MNT_DIR" 2>/dev/null || {
        echo "[mm-overlayfs] Warning: Failed to unmount cleanly"
        umount -l "$MNT_DIR" 2>/dev/null
    }
    echo "[mm-overlayfs] Image unmounted"
fi

# Preserve image file (contains user data)
echo "[mm-overlayfs] Image file preserved at: $IMG_FILE"
echo "[mm-overlayfs] "
echo "[mm-overlayfs] To manually remove all module data:"
echo "[mm-overlayfs]   rm -f $IMG_FILE"
echo "[mm-overlayfs]   rm -rf $MNT_DIR"
echo "[mm-overlayfs] "
echo "[mm-overlayfs] WARNING: This will delete ALL module content!"
echo "[mm-overlayfs] "
echo "[mm-overlayfs] Uninstall complete"

exit 0
