use std::fs;
#[cfg(any(target_os = "linux", target_os = "android"))]
use std::os::fd::RawFd;
use std::sync::OnceLock;

// Event constants
const EVENT_POST_FS_DATA: u32 = 1;
const EVENT_BOOT_COMPLETED: u32 = 2;

const KSU_IOCTL_GRANT_ROOT: u32 = 0x00004b01; // _IOC(_IOC_NONE, 'K', 1, 0)
const KSU_IOCTL_GET_INFO: u32 = 0x80004b02; // _IOC(_IOC_READ, 'K', 2, 0)
const KSU_IOCTL_REPORT_EVENT: u32 = 0x40004b03; // _IOC(_IOC_WRITE, 'K', 3, 0)
const KSU_IOCTL_SET_SEPOLICY: u32 = 0xc0004b04; // _IOC(_IOC_READ|_IOC_WRITE, 'K', 4, 0)
const KSU_IOCTL_CHECK_SAFEMODE: u32 = 0x80004b05; // _IOC(_IOC_READ, 'K', 5, 0)
const KSU_IOCTL_GET_FEATURE: u32 = 0xc0004b0d; // _IOC(_IOC_READ|_IOC_WRITE, 'K', 13, 0)
const KSU_IOCTL_SET_FEATURE: u32 = 0x40004b0e; // _IOC(_IOC_WRITE, 'K', 14, 0)
const KSU_IOCTL_GET_WRAPPER_FD: u32 = 0x40004b0f; // _IOC(_IOC_WRITE, 'K', 15, 0)
const KSU_IOCTL_MANAGE_MARK: u32 = 0xc0004b10; // _IOC(_IOC_READ|_IOC_WRITE, 'K', 16, 0)

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

#[repr(C)]
#[derive(Clone, Copy, Default)]
struct GetFeatureCmd {
    feature_id: u32,
    value: u64,
    supported: u8,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
struct SetFeatureCmd {
    feature_id: u32,
    value: u64,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
struct GetWrapperFdCmd {
    fd: i32,
    flags: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
struct ManageMarkCmd {
    operation: u32,
    pid: i32,
    result: u32,
}

// Mark operation constants
const KSU_MARK_GET: u32 = 1;
const KSU_MARK_MARK: u32 = 2;
const KSU_MARK_UNMARK: u32 = 3;
const KSU_MARK_REFRESH: u32 = 4;

// Global driver fd cache
#[cfg(any(target_os = "linux", target_os = "android"))]
static DRIVER_FD: OnceLock<RawFd> = OnceLock::new();
#[cfg(any(target_os = "linux", target_os = "android"))]
static INFO_CACHE: OnceLock<GetInfoCmd> = OnceLock::new();

const KSU_INSTALL_MAGIC1: u32 = 0xDEADBEEF;
const KSU_INSTALL_MAGIC2: u32 = 0xCAFEBABE;

#[cfg(any(target_os = "linux", target_os = "android"))]
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
#[cfg(any(target_os = "linux", target_os = "android"))]
fn init_driver_fd() -> Option<RawFd> {
    let fd = scan_driver_fd();
    if fd.is_none() {
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
        fd
    }
}

// ioctl wrapper using libc
#[cfg(any(target_os = "linux", target_os = "android"))]
fn ksuctl<T>(request: u32, arg: *mut T) -> std::io::Result<i32> {
    use std::io;

    let fd = *DRIVER_FD.get_or_init(|| init_driver_fd().unwrap_or(-1));
    unsafe {
        #[cfg(not(target_env = "gnu"))]
        let ret = libc::ioctl(fd as libc::c_int, request as i32, arg);
        #[cfg(target_env = "gnu")]
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
#[cfg(any(target_os = "linux", target_os = "android"))]
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

/// Get feature value and support status from kernel
/// Returns (value, supported)
pub fn get_feature(feature_id: u32) -> std::io::Result<(u64, bool)> {
    let mut cmd = GetFeatureCmd {
        feature_id,
        value: 0,
        supported: 0,
    };
    ksuctl(KSU_IOCTL_GET_FEATURE, &mut cmd as *mut _)?;
    Ok((cmd.value, cmd.supported != 0))
}

/// Set feature value in kernel
pub fn set_feature(feature_id: u32, value: u64) -> std::io::Result<()> {
    let mut cmd = SetFeatureCmd { feature_id, value };
    ksuctl(KSU_IOCTL_SET_FEATURE, &mut cmd as *mut _)?;
    Ok(())
}

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn get_wrapped_fd(fd: RawFd) -> std::io::Result<RawFd> {
    let mut cmd = GetWrapperFdCmd { fd, flags: 0 };
    let result = ksuctl(KSU_IOCTL_GET_WRAPPER_FD, &mut cmd as *mut _)?;
    Ok(result)
}

/// Get mark status for a process (pid=0 returns total marked count)
pub fn mark_get(pid: i32) -> std::io::Result<u32> {
    let mut cmd = ManageMarkCmd {
        operation: KSU_MARK_GET,
        pid,
        result: 0,
    };
    ksuctl(KSU_IOCTL_MANAGE_MARK, &mut cmd as *mut _)?;
    Ok(cmd.result)
}

/// Mark a process (pid=0 marks all processes)
pub fn mark_set(pid: i32) -> std::io::Result<()> {
    let mut cmd = ManageMarkCmd {
        operation: KSU_MARK_MARK,
        pid,
        result: 0,
    };
    ksuctl(KSU_IOCTL_MANAGE_MARK, &mut cmd as *mut _)?;
    Ok(())
}

/// Unmark a process (pid=0 unmarks all processes)
pub fn mark_unset(pid: i32) -> std::io::Result<()> {
    let mut cmd = ManageMarkCmd {
        operation: KSU_MARK_UNMARK,
        pid,
        result: 0,
    };
    ksuctl(KSU_IOCTL_MANAGE_MARK, &mut cmd as *mut _)?;
    Ok(())
}

/// Refresh mark for all running processes
pub fn mark_refresh() -> std::io::Result<()> {
    let mut cmd = ManageMarkCmd {
        operation: KSU_MARK_REFRESH,
        pid: 0,
        result: 0,
    };
    ksuctl(KSU_IOCTL_MANAGE_MARK, &mut cmd as *mut _)?;
    Ok(())
}
