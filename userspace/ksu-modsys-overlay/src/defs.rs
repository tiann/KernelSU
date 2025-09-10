// Reuse core defaults to avoid duplication
pub use ksu_core::defs::{
    ADB_DIR,
    BINARY_DIR,
    BUSYBOX_PATH,
    DISABLE_FILE_NAME,
    KSU_OVERLAY_SOURCE,
    MODULE_ACTION_SH,
    MODULE_DIR,
    MODULE_IMG,
    MODULE_UPDATE_IMG,
    MODULE_UPDATE_TMP_DIR,
    MODULE_UPDATE_TMP_IMG,
    MODULE_WEB_DIR,
    REMOVE_FILE_NAME,
    RESETPROP_PATH,
    SKIP_MOUNT_FILE_NAME,
    SYSTEM_RW_DIR,
    // also re-export module-related defaults
    TEMP_DIR,
    UPDATE_FILE_NAME,
    WORKING_DIR,
};

// Version strings written by build.rs into OUT_DIR
pub const VERSION_CODE: &str = include_str!(concat!(env!("OUT_DIR"), "/VERSION_CODE"));
pub const VERSION_NAME: &str = include_str!(concat!(env!("OUT_DIR"), "/VERSION_NAME"));
