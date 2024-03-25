const EVENT_POST_FS_DATA: u64 = 1;
const EVENT_BOOT_COMPLETED: u64 = 2;
const EVENT_MODULE_MOUNTED: u64 = 3;

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn get_version() -> i32 {
    rustix::process::ksu_get_version()
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
pub fn get_version() -> i32 {
    0
}

#[cfg(any(target_os = "linux", target_os = "android"))]
fn report_event(event: u64) {
    rustix::process::ksu_report_event(event)
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
fn report_event(_event: u64) {}

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn check_kernel_safemode() -> bool {
    rustix::process::ksu_check_kernel_safemode()
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
pub fn check_kernel_safemode() -> bool {
    false
}

pub fn report_post_fs_data() {
    report_event(EVENT_POST_FS_DATA);
}

pub fn report_boot_complete() {
    report_event(EVENT_BOOT_COMPLETED);
}

pub fn report_module_mounted() {
    report_event(EVENT_MODULE_MOUNTED);
}
