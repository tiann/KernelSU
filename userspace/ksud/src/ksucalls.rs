use std::fs;
#[cfg(any(target_os = "linux", target_os = "android"))]
use std::os::unix::io::RawFd;
use std::sync::OnceLock;

// Event constants
const EVENT_POST_FS_DATA: u32 = 1;
const EVENT_BOOT_COMPLETED: u32 = 2;
const EVENT_MODULE_MOUNTED: u32 = 3;

const KSU_IOCTL_GRANT_ROOT: u32 = 0x4B01; // _IO('K', 1)
const KSU_IOCTL_GET_INFO: u32 = 0x80084b02; // _IOR('K', 2, struct ksu_get_info_cmd)
const KSU_IOCTL_REPORT_EVENT: u32 = 0x40044b03; // _IOW('K', 3, struct ksu_report_event_cmd)
const KSU_IOCTL_SET_SEPOLICY: u32 = 0xc0104b04; // _IOWR('K', 4, struct ksu_set_sepolicy_cmd)
const KSU_IOCTL_CHECK_SAFEMODE: u32 = 0x80014b05; // _IOR('K', 5, struct ksu_check_safemode_cmd)

#[repr(C)]
#[derive(Clone, Copy, Default)]
struct GetInfoCmd {
    version: u32,
    flags: u32,
}

#[repr(C)]
struct ReportEventCmd {
    event: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct SetSepolicyCmd {
    pub cmd: u64,
    pub arg: u64,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
struct CheckSafemodeCmd {
    in_safe_mode: u8,
}

// Global driver fd cache
#[cfg(any(target_os = "linux", target_os = "android"))]
static DRIVER_FD: OnceLock<RawFd> = OnceLock::new();
#[cfg(any(target_os = "linux", target_os = "android"))]
static INFO_CACHE: OnceLock<GetInfoCmd> = OnceLock::new();

const KSU_INSTALL_MAGIC1: u32 = 0xDEADBEEF;
const KSU_INSTALL_MAGIC2: u32 = 0xCAFEBABE;

fn scan_driver_fd() -> Option<RawFd> {
    let fd_dir = fs::read_dir("/proc/self/fd").ok()?;

    for entry in fd_dir.flatten() {
        if let Ok(fd_num) = entry.file_name().to_string_lossy().parse::<i32>() {
            let link_path = format!("/proc/self/fd/{}", fd_num);
            if let Ok(target) = fs::read_link(&link_path) {
                let target_str = target.to_string_lossy();
                if target_str.contains("[ksu_driver]") {
                    return Some(fd_num);
                }
            }
        }
    }

    None
}

// Get cached driver fd
fn init_driver_fd() -> Option<RawFd> {
    let is_root = rustix::process::getuid().is_root();
    if is_root {
        let mut fd = -1;
        unsafe {
            libc::syscall(
                libc::SYS_reboot,
                KSU_INSTALL_MAGIC1,
                KSU_INSTALL_MAGIC2,
                0,
                &mut fd,
            );
        };
        if fd >= 0 { Some(fd) } else { None }
    } else {
        scan_driver_fd()
    }
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
fn init_driver_fd() -> Option<RawFd> {
    None
}

// ioctl wrapper using libc
#[cfg(any(target_os = "linux", target_os = "android"))]
fn ksuctl<T>(request: u32, arg: *mut T) -> std::io::Result<i32> {
    use std::io;

    let fd = *DRIVER_FD.get_or_init(|| init_driver_fd().unwrap_or(-1));
    unsafe {
        #[cfg(target_os = "android")]
        let ret = libc::ioctl(fd as libc::c_int, request as i32, arg);
        #[cfg(not(target_os = "android"))]
        let ret = libc::ioctl(fd as libc::c_int, request as u64, arg);
        if ret < 0 {
            Err(io::Error::last_os_error())
        } else {
            Ok(ret)
        }
    }
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
fn ksuctl<T>(_request: u32, _arg: *mut T) -> std::io::Result<i32> {
    Err(std::io::Error::from_raw_os_error(libc::ENOSYS))
}

// API implementations
fn get_info() -> GetInfoCmd {
    *INFO_CACHE.get_or_init(|| {
        let mut cmd = GetInfoCmd {
            version: 0,
            flags: 0,
        };
        let _ = ksuctl(KSU_IOCTL_GET_INFO, &mut cmd as *mut _);
        cmd
    })
}

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn get_version() -> i32 {
    get_info().version as i32
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
pub fn get_version() -> i32 {
    0
}

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn grant_root() -> std::io::Result<()> {
    ksuctl(KSU_IOCTL_GRANT_ROOT, std::ptr::null_mut::<u8>())?;
    Ok(())
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
pub fn grant_root() -> std::io::Result<()> {
    Err(std::io::Error::from_raw_os_error(libc::ENOSYS))
}

#[cfg(any(target_os = "linux", target_os = "android"))]
fn report_event(event: u32) {
    let mut cmd = ReportEventCmd { event };
    let _ = ksuctl(KSU_IOCTL_REPORT_EVENT, &mut cmd as *mut _);
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
fn report_event(_event: u32) {}

pub fn report_post_fs_data() {
    report_event(EVENT_POST_FS_DATA);
}

pub fn report_boot_complete() {
    report_event(EVENT_BOOT_COMPLETED);
}

pub fn report_module_mounted() {
    report_event(EVENT_MODULE_MOUNTED);
}

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn check_kernel_safemode() -> bool {
    let mut cmd = CheckSafemodeCmd { in_safe_mode: 0 };
    let _ = ksuctl(KSU_IOCTL_CHECK_SAFEMODE, &mut cmd as *mut _);
    cmd.in_safe_mode != 0
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
pub fn check_kernel_safemode() -> bool {
    false
}

pub fn set_sepolicy(cmd: &SetSepolicyCmd) -> std::io::Result<()> {
    let mut ioctl_cmd = *cmd;
    ksuctl(KSU_IOCTL_SET_SEPOLICY, &mut ioctl_cmd as *mut _)?;
    Ok(())
}
