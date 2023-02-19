use anyhow::Result;

#[cfg(unix)]
use anyhow::ensure;
#[cfg(unix)]
use std::os::unix::process::CommandExt;

pub const KERNEL_SU_OPTION: u32 = 0xDEAD_BEEF;

const CMD_GRANT_ROOT: u64 = 0;
// const CMD_BECOME_MANAGER: u64 = 1;
const CMD_GET_VERSION: u64 = 2;
// const CMD_ALLOW_SU: u64 = 3;
// const CMD_DENY_SU: u64 = 4;
// const CMD_GET_ALLOW_LIST: u64 = 5;
// const CMD_GET_DENY_LIST: u64 = 6;
const CMD_REPORT_EVENT: u64 = 7;
pub const CMD_SET_SEPOLICY: u64 = 8;
pub const CMD_CHECK_SAFEMODE: u64 = 9;

const EVENT_POST_FS_DATA: u64 = 1;
const EVENT_BOOT_COMPLETED: u64 = 2;

#[cfg(any(target_os = "linux", target_os = "android"))]
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

#[cfg(not(any(target_os = "linux", target_os = "android")))]
pub fn grant_root() -> Result<()> {
    unimplemented!("grant_root is only available on android");
}

pub fn get_version() -> i32 {
    let mut result: i32 = 0;
    #[cfg(any(target_os = "linux", target_os = "android"))]
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
    #[cfg(any(target_os = "linux", target_os = "android"))]
    unsafe {
        #[allow(clippy::cast_possible_wrap)]
        libc::prctl(
            KERNEL_SU_OPTION as i32, // supposed to overflow
            CMD_REPORT_EVENT,
            event,
        );
    }
}

pub fn check_kernel_safemode() -> bool {
    let mut result: i32 = 0;
    #[cfg(any(target_os = "linux", target_os = "android"))]
    unsafe {
        #[allow(clippy::cast_possible_wrap)]
        libc::prctl(
            KERNEL_SU_OPTION as i32, // supposed to overflow
            CMD_CHECK_SAFEMODE,
            0,
            0,
            std::ptr::addr_of_mut!(result).cast::<libc::c_void>(),
        );
    }
    result == KERNEL_SU_OPTION as i32
}

pub fn report_post_fs_data() {
    report_event(EVENT_POST_FS_DATA);
}

pub fn report_boot_complete() {
    report_event(EVENT_BOOT_COMPLETED);
}
