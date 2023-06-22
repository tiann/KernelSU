use const_format::concatcp;

pub const ADB_DIR: &str = "/data/adb/";
pub const WORKING_DIR: &str = concatcp!(ADB_DIR, "ksu/");
pub const BINARY_DIR: &str = concatcp!(WORKING_DIR, "bin/");
pub const LOG_DIR: &str = concatcp!(WORKING_DIR, "log/");

pub const KSURC_PATH: &str = concatcp!(WORKING_DIR, ".ksurc");
pub const KSU_OVERLAY_SOURCE: &str = "KSU";
pub const DAEMON_PATH: &str = concatcp!(ADB_DIR, "ksud");

#[cfg(target_os = "android")]
pub const DAEMON_LINK_PATH: &str = concatcp!(BINARY_DIR, "ksud");

pub const MODULE_DIR: &str = concatcp!(ADB_DIR, "modules/");
pub const MODULE_IMG: &str = concatcp!(WORKING_DIR, "modules.img");
pub const MODULE_UPDATE_IMG: &str = concatcp!(WORKING_DIR, "modules_update.img");

pub const MODULE_UPDATE_TMP_IMG: &str = concatcp!(WORKING_DIR, "update_tmp.img");

// warning: this directory should not change, or you need to change the code in module_installer.sh!!!
pub const MODULE_UPDATE_TMP_DIR: &str = concatcp!(ADB_DIR, "modules_update/");

pub const DISABLE_FILE_NAME: &str = "disable";
pub const UPDATE_FILE_NAME: &str = "update";
pub const REMOVE_FILE_NAME: &str = "remove";
pub const SKIP_MOUNT_FILE_NAME: &str = "skip_mount";

pub const VERSION_CODE: &str = include_str!(concat!(env!("OUT_DIR"), "/VERSION_CODE"));
pub const VERSION_NAME: &str = include_str!(concat!(env!("OUT_DIR"), "/VERSION_NAME"));
