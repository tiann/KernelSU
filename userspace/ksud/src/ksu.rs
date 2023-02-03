use anyhow::Result;

#[cfg(unix)]
use anyhow::ensure;
#[cfg(unix)]
use std::os::unix::process::CommandExt;

pub const KERNEL_SU_OPTION: u32 = 0xDEAD_BEEF;

const CMD_GRANT_ROOT: u64 = 100;
const CMD_GET_VERSION: u64 = 101;
const CMD_REPORT_EVENT: u64 = 102;
pub const CMD_SET_SEPOLICY: u64 = 103;

const CMD_BECOME_MANAGER: u64 = 200;
const CMD_SET_UID_DATA: u64 = 201;
const CMD_GET_UID_DATA: u64 = 202;
const CMD_COUNT_UID_DATA: u64 = 203;
const CMD_LIST_UID_DATA: u64 = 204;

const EVENT_POST_FS_DATA: u64 = 1;
const EVENT_BOOT_COMPLETED: u64 = 2;

#[cfg(unix)]
pub fn grant_root() -> Result<()> {
    let mut result: u32 = 0;
    unsafe {
        #[allow(clippy::cast_possible_wrap)]
        libc::prctl(
            KERNEL_SU_OPTION as i32, // supposed to overflow
            CMD_GRANT_ROOT,
            0,
            0,
            std::ptr::addr_of_mut!(result).cast::<libc::c_void>(),
        );
    }

    ensure!(result == KERNEL_SU_OPTION, "grant root failed");
    Err(std::process::Command::new("sh").exec().into())
}

#[cfg(not(unix))]
pub fn grant_root() -> Result<()> {
    unimplemented!("grant_root is only available on android");
}

pub fn get_version() -> i32 {
    let mut result: i32 = 0;
    #[cfg(unix)]
    unsafe {
        #[allow(clippy::cast_possible_wrap)]
        libc::prctl(
            KERNEL_SU_OPTION as i32, // supposed to overflow
            CMD_GET_VERSION,
            std::ptr::addr_of_mut!(result).cast::<libc::c_void>(),
        );
    }
    result
}

fn report_event(event: u64) {
    #[cfg(unix)]
    unsafe {
        #[allow(clippy::cast_possible_wrap)]
        libc::prctl(
            KERNEL_SU_OPTION as i32, // supposed to overflow
            CMD_REPORT_EVENT,
            event,
        );
    }
}

pub fn report_post_fs_data() {
    report_event(EVENT_POST_FS_DATA);
}

pub fn report_boot_complete() {
    report_event(EVENT_BOOT_COMPLETED);
}
