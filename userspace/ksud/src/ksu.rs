#![allow(dead_code, unused_mut, unused_variables)]
const KERNEL_SU_OPTION: u32 = 0xDEADBEEF;

// const CMD_GRANT_ROOT: u64 = 0;
// const CMD_BECOME_MANAGER: u64 = 1;
const CMD_GET_VERSION: u64 = 2;
// const CMD_ALLOW_SU: u64 = 3;
// const CMD_DENY_SU: u64 = 4;
// const CMD_GET_ALLOW_LIST: u64 = 5;
// const CMD_GET_DENY_LIST: u64 = 6;
const CMD_REPORT_EVENT: u64 = 7;

const EVENT_POST_FS_DATA: u64 = 1;
const EVENT_BOOT_COMPLETED: u64 = 2;

// pub fn grant_root() -> bool {
//     let mut result: i32 = 0;
//     unsafe {
//         libc::prctl(
//             KERNEL_SU_OPTION as i32,
//             CMD_GRANT_ROOT,
//             0,
//             0,
//             &mut result as *mut _ as *mut libc::c_void,
//         );
//     }
//     return result as u32 == KERNEL_SU_OPTION;
// }

pub fn get_version() -> i32 {
    let mut result: i32 = 0;
    #[cfg(target_os = "android")]
    unsafe {
        libc::prctl(
            KERNEL_SU_OPTION as i32,
            CMD_GET_VERSION,
            &mut result as *mut _ as *mut libc::c_void,
        );
    }
    result
}

fn report_event(event: u64) {
    #[cfg(target_os = "android")]
    unsafe {
        libc::prctl(
            KERNEL_SU_OPTION as i32,
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