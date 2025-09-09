#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn check_kernel_safemode() -> bool {
    rustix::process::ksu_check_kernel_safemode()
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
pub fn check_kernel_safemode() -> bool { false }

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn report_module_mounted() { rustix::process::ksu_report_event(3) }

#[cfg(not(any(target_os = "linux", target_os = "android")))]
pub fn report_module_mounted() {}
