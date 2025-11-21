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

# Determine whether this module should be moved into the ext4 image.
# We only relocate payloads that expose system/ overlays and do not opt out via skip_mount.
module_requires_overlay_move() {
    if [ -f "$MODPATH/skip_mount" ]; then
        ui_print "- skip_mount flag detected; keeping files under /data/adb/modules"
        return 1
    fi

    if [ ! -d "$MODPATH/system" ]; then
        ui_print "- No system/ directory detected; keeping files under /data/adb/modules"
        return 1
    fi

    return 0
}

# Copy SELinux contexts from src tree to destination by mirroring each entry.
copy_selinux_contexts() {
    command -v chcon >/dev/null 2>&1 || return 0

    SRC="$1"
    DST="$2"

    if [ -z "$SRC" ] || [ -z "$DST" ] || [ ! -e "$SRC" ] || [ ! -e "$DST" ]; then
        return 0
    fi

    chcon --reference="$SRC" "$DST" 2>/dev/null || true

    find "$SRC" -print | while IFS= read -r PATH_SRC; do
        if [ "$PATH_SRC" = "$SRC" ]; then
            continue
        fi
        REL_PATH="${PATH_SRC#"${SRC}/"}"
        PATH_DST="$DST/$REL_PATH"
        if [ -e "$PATH_DST" ] || [ -L "$PATH_DST" ]; then
            chcon --reference="$PATH_SRC" "$PATH_DST" 2>/dev/null || true
        fi
    done
}

# Post-installation: move partition directories to ext4 image
post_install_to_image() {
    ui_print "- Copying module content to image"

    set_perm "$MNT_DIR" 0 0 0755 0644

    MOD_IMG_DIR="$MNT_DIR/$MODID"
    mkdir -p "$MOD_IMG_DIR"
    set_perm "$MOD_IMG_DIR" 0 0 0755 0644

    # Move all partition directories
    for partition in system vendor product system_ext odm oem; do
        if [ -d "$MODPATH/$partition" ]; then
            ui_print "- Copying $partition/"
            cp -af "$MODPATH/$partition" "$MOD_IMG_DIR/" || {
                ui_print "! Warning: Failed to move $partition"
                continue
            }
            copy_selinux_contexts "$MODPATH/$partition" "$MOD_IMG_DIR/$partition"
        fi
    done
}

ui_print "- Using meta-overlayfs metainstall"

install_module

if module_requires_overlay_move; then
    ensure_image_mounted
    post_install_to_image
else
    ui_print "- Skipping move to modules image"
fi

ui_print "- Installation complete"
