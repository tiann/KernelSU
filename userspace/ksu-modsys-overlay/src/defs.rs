// Re-export constants from ksud for consistency
use const_format::concatcp;

pub const ADB_DIR: &str = "/data/adb/";
pub const WORKING_DIR: &str = concatcp!(ADB_DIR, "ksu/");
pub const BINARY_DIR: &str = concatcp!(WORKING_DIR, "bin/");
pub const LOG_DIR: &str = concatcp!(WORKING_DIR, "log/");

pub const MODULE_DIR: &str = concatcp!(ADB_DIR, "modules/");
pub const MODULE_IMG: &str = concatcp!(WORKING_DIR, "modules.img");
pub const MODULE_UPDATE_IMG: &str = concatcp!(WORKING_DIR, "modules_update.img");
pub const MODULE_UPDATE_TMP_IMG: &str = concatcp!(WORKING_DIR, "update_tmp.img");
pub const MODULE_UPDATE_TMP_DIR: &str = concatcp!(ADB_DIR, "modules_update/");

pub const SYSTEM_RW_DIR: &str = concatcp!(MODULE_DIR, ".rw/");

pub const TEMP_DIR: &str = "/debug_ramdisk";
pub const MODULE_WEB_DIR: &str = "webroot";
pub const MODULE_ACTION_SH: &str = "action.sh";
pub const DISABLE_FILE_NAME: &str = "disable";
pub const UPDATE_FILE_NAME: &str = "update";
pub const REMOVE_FILE_NAME: &str = "remove";
pub const SKIP_MOUNT_FILE_NAME: &str = "skip_mount";

pub const KSU_OVERLAY_SOURCE: &str = "KSU";

// Asset paths - these will be provided by ksud
pub const BUSYBOX_PATH: &str = concatcp!(BINARY_DIR, "busybox");
pub const RESETPROP_PATH: &str = concatcp!(BINARY_DIR, "resetprop");
pub const MAGISKBOOT_PATH: &str = concatcp!(BINARY_DIR, "magiskboot");

// KernelSU call placeholders - will be provided via environment or IPC
pub const VERSION_CODE: &str = "10940";
pub const VERSION_NAME: &str = "v0.9.5";
