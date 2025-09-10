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
    // also re-export module-related defaults
    TEMP_DIR,
    MODULE_WEB_DIR,
    MODULE_ACTION_SH,
    DISABLE_FILE_NAME,
    UPDATE_FILE_NAME,
    REMOVE_FILE_NAME,
    SKIP_MOUNT_FILE_NAME,
};

// Version strings written by build.rs into OUT_DIR
pub const VERSION_CODE: &str = include_str!(concat!(env!("OUT_DIR"), "/VERSION_CODE"));
pub const VERSION_NAME: &str = include_str!(concat!(env!("OUT_DIR"), "/VERSION_NAME"));
