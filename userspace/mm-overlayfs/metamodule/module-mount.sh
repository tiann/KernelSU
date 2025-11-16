#!/system/bin/sh
# KernelSU Official Mount Handler
# This script is the entry point for module mounting

MODDIR="${0%/*}"
MODULE_DIR="${MODULE_DIR:-/data/adb/modules}"

# Log function
log() {
    echo "[ksu-metamodule] $1"
}

log "Starting module mount process"
log "Module directory: $MODULE_DIR"

# Binary path (architecture-specific binary selected during installation)
BINARY="$MODDIR/mm-overlayfs"

if [ ! -f "$BINARY" ]; then
    log "Binary not found: $BINARY"
    exit 1
fi

# Execute the mount binary
log "Executing $BINARY"
export MODULE_DIR
"$BINARY"
EXIT_CODE=$?

if [ $EXIT_CODE -ne 0 ]; then
    log "Mount failed with exit code $EXIT_CODE"
    exit $EXIT_CODE
fi

log "Mount completed successfully"
exit 0
