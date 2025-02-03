use const_format::concatcp;

pub const ADB_DIR: &str = "/data/adb/";
pub const WORKING_DIR: &str = concatcp!(ADB_DIR, "ksu/");
pub const BINARY_DIR: &str = concatcp!(WORKING_DIR, "bin/");
pub const LOG_DIR: &str = concatcp!(WORKING_DIR, "log/");

pub const PROFILE_DIR: &str = concatcp!(WORKING_DIR, "profile/");
pub const PROFILE_SELINUX_DIR: &str = concatcp!(PROFILE_DIR, "selinux/");
pub const PROFILE_TEMPLATE_DIR: &str = concatcp!(PROFILE_DIR, "templates/");

pub const KSURC_PATH: &str = concatcp!(WORKING_DIR, ".ksurc");
pub const KSU_OVERLAY_SOURCE: &str = "KSU";
pub const DAEMON_PATH: &str = concatcp!(ADB_DIR, "ksud");
pub const MAGISKBOOT_PATH: &str = concatcp!(BINARY_DIR, "magiskboot");

#[cfg(target_os = "android")]
pub const DAEMON_LINK_PATH: &str = concatcp!(BINARY_DIR, "ksud");

pub const MODULE_DIR: &str = concatcp!(ADB_DIR, "modules/");
pub const MODULE_IMG: &str = concatcp!(WORKING_DIR, "modules.img");
pub const MODULE_UPDATE_IMG: &str = concatcp!(WORKING_DIR, "modules_update.img");

pub const MODULE_UPDATE_TMP_IMG: &str = concatcp!(WORKING_DIR, "update_tmp.img");

// warning: this directory should not change, or you need to change the code in module_installer.sh!!!
pub const MODULE_UPDATE_TMP_DIR: &str = concatcp!(ADB_DIR, "modules_update/");

pub const SYSTEM_RW_DIR: &str = concatcp!(MODULE_DIR, ".rw/");

pub const TEMP_DIR: &str = "/debug_ramdisk";
pub const MODULE_WEB_DIR: &str = "webroot";
pub const MODULE_ACTION_SH: &str = "action.sh";
pub const DISABLE_FILE_NAME: &str = "disable";
pub const UPDATE_FILE_NAME: &str = "update";
pub const REMOVE_FILE_NAME: &str = "remove";
pub const SKIP_MOUNT_FILE_NAME: &str = "skip_mount";

pub const VERSION_CODE: &str = include_str!(concat!(env!("OUT_DIR"), "/VERSION_CODE"));
pub const VERSION_NAME: &str = include_str!(concat!(env!("OUT_DIR"), "/VERSION_NAME"));

pub const KSU_BACKUP_DIR: &str = WORKING_DIR;
pub const KSU_BACKUP_FILE_PREFIX: &str = "ksu_backup_";
pub const BACKUP_FILENAME: &str = "stock_image.sha1";
