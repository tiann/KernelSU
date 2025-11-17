// Constants for KernelSU module mounting

// Dual-directory support for ext4 image
pub const MODULE_METADATA_DIR: &str = "/data/adb/modules/";
pub const MODULE_CONTENT_DIR: &str = "/data/adb/metamodule/mnt/";

// Legacy constant (for backwards compatibility)
pub const _MODULE_DIR: &str = "/data/adb/modules/";

// Status marker files
pub const DISABLE_FILE_NAME: &str = "disable";
pub const _REMOVE_FILE_NAME: &str = "remove";
pub const SKIP_MOUNT_FILE_NAME: &str = "skip_mount";

// System directories
pub const SYSTEM_RW_DIR: &str = "/data/adb/modules/.rw/";
pub const KSU_OVERLAY_SOURCE: &str = "KSU";
