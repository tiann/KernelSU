use const_format::concatcp;

pub const DAEMON_PATH: &str = "/data/adb/ksud";
pub const WORKING_DIR: &str = "/data/adb/ksu/";

pub const MODULE_DIR: &str = concatcp!(WORKING_DIR, "modules/");
pub const MODULE_IMG: &str = concatcp!(WORKING_DIR, "modules.img");
pub const MODULE_UPDATE_IMG: &str = concatcp!(WORKING_DIR, "modules_update.img");

pub const MODULE_UPDATE_TMP_IMG: &str = concatcp!(WORKING_DIR, "update_tmp.img");

// warning: this directory should not change, or you need to change the code in module_installer.sh!!!
pub const MODULE_UPDATE_TMP_DIR: &str = concatcp!(WORKING_DIR, "modules_update/");

pub const DISABLE_FILE_NAME: &str = "disable";
pub const UPDATE_FILE_NAME: &str = "update";
pub const REMOVE_FILE_NAME: &str = "remove";
