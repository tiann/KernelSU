// Reuse core defaults to avoid duplication
pub use ksu_core::defs::{
    ADB_DIR,
    WORKING_DIR,
    BINARY_DIR,
    MODULE_DIR,
    MODULE_IMG,
    MODULE_UPDATE_IMG,
    MODULE_UPDATE_TMP_IMG,
    MODULE_UPDATE_TMP_DIR,
    SYSTEM_RW_DIR,
    KSU_OVERLAY_SOURCE,
    BUSYBOX_PATH,
    RESETPROP_PATH,
};

pub const TEMP_DIR: &str = "/debug_ramdisk";
pub const MODULE_WEB_DIR: &str = "webroot";
pub const MODULE_ACTION_SH: &str = "action.sh";
pub const DISABLE_FILE_NAME: &str = "disable";
pub const UPDATE_FILE_NAME: &str = "update";
pub const REMOVE_FILE_NAME: &str = "remove";
pub const SKIP_MOUNT_FILE_NAME: &str = "skip_mount";

// Version strings presented by ksud; keep simple fallbacks for overlay builds
pub const VERSION_CODE: &str = "10940";
pub const VERSION_NAME: &str = "v0.9.5";
