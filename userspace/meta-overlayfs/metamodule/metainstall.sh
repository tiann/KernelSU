#!/system/bin/sh
############################################
# meta-overlayfs metainstall.sh
# Module installation hook for ext4 image support
############################################

# Constants
IMG_FILE="/data/adb/metamodule/modules.img"
MNT_DIR="/data/adb/metamodule/mnt"

# Ensure ext4 image is mounted
ensure_image_mounted() {
    if ! mountpoint -q "$MNT_DIR" 2>/dev/null; then
        ui_print "- Mounting modules image"
        mkdir -p "$MNT_DIR"
        mount -t ext4 -o loop,rw,noatime "$IMG_FILE" "$MNT_DIR" || {
            abort "! Failed to mount modules image"
        }
        ui_print "- Image mounted successfully"
    else
        ui_print "- Image already mounted"
    fi
}

# Post-installation: move partition directories to ext4 image
post_install_to_image() {
    ui_print "- Moving module content to image"

    MOD_IMG_DIR="$MNT_DIR/$MODID"
    mkdir -p "$MOD_IMG_DIR"

    # Move all partition directories
    for partition in system vendor product system_ext odm oem; do
        if [ -d "$MODPATH/$partition" ]; then
            ui_print "  Moving $partition/"
            mv "$MODPATH/$partition" "$MOD_IMG_DIR/" || {
                ui_print "! Warning: Failed to move $partition"
            }
        fi
    done

    # Set permissions
    chown -R 0:0 "$MOD_IMG_DIR" 2>/dev/null
    chmod -R 755 "$MOD_IMG_DIR" 2>/dev/null

    ui_print "- Module content moved to image"
}

# Main installation flow
ui_print "- Using meta-overlayfs metainstall"

# 1. Ensure ext4 image is mounted
ensure_image_mounted

# 2. Call standard install_module function (defined in installer.sh)
install_module

# 3. Post-process: move content to image
post_install_to_image

ui_print "- Installation complete"
