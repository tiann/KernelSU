#!/system/bin/sh
# meta-overlayfs Module Mount Handler
# This script is the entry point for dual-directory module mounting

MODDIR="${0%/*}"
IMG_FILE="$MODDIR/modules.img"
MNT_DIR="$MODDIR/mnt"

# Log function
log() {
    echo "[meta-overlayfs] $1"
}

log "Starting module mount process"

# Ensure ext4 image is mounted
if ! mountpoint -q "$MNT_DIR" 2>/dev/null; then
    log "Image not mounted, mounting now..."

    # Check if image file exists
    if [ ! -f "$IMG_FILE" ]; then
        log "ERROR: Image file not found at $IMG_FILE"
        exit 1
    fi

    # Create mount point
    mkdir -p "$MNT_DIR"

    # Mount the ext4 image
    mount -t ext4 -o loop,rw,noatime "$IMG_FILE" "$MNT_DIR" || {
        log "ERROR: Failed to mount image"
        exit 1
    }
    log "Image mounted successfully at $MNT_DIR"
else
    log "Image already mounted at $MNT_DIR"
fi

# Binary path (architecture-specific binary selected during installation)
BINARY="$MODDIR/meta-overlayfs"

if [ ! -f "$BINARY" ]; then
    log "ERROR: Binary not found: $BINARY"
    exit 1
fi

# Set dual-directory environment variables
export MODULE_METADATA_DIR="/data/adb/modules"
export MODULE_CONTENT_DIR="$MNT_DIR"

log "Metadata directory: $MODULE_METADATA_DIR"
log "Content directory: $MODULE_CONTENT_DIR"
log "Executing $BINARY"

# Execute the mount binary
"$BINARY"
EXIT_CODE=$?

if [ $EXIT_CODE -ne 0 ]; then
    log "Mount failed with exit code $EXIT_CODE"
    exit $EXIT_CODE
fi

log "Mount completed successfully"
exit 0
