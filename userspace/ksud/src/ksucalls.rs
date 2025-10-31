use std::os::unix::io::RawFd;
use std::sync::OnceLock;

// Event constants
const EVENT_POST_FS_DATA: u32 = 1;
const EVENT_BOOT_COMPLETED: u32 = 2;
const EVENT_MODULE_MOUNTED: u32 = 3;

// IOCTL command definitions
const KSU_IOCTL_GRANT_ROOT: u64 = 0x4B01; // _IO('K', 1)
const KSU_IOCTL_GET_INFO: u64 = 0x804B02; // _IOR('K', 2, struct ksu_get_info_cmd)
const KSU_IOCTL_REPORT_EVENT: u64 = 0x404B03; // _IOW('K', 3, struct ksu_report_event_cmd)
const KSU_IOCTL_SET_SEPOLICY: u64 = 0xC04B04; // _IOWR('K', 4, struct ksu_set_sepolicy_cmd)
const KSU_IOCTL_CHECK_SAFEMODE: u64 = 0x804B05; // _IOR('K', 5, struct ksu_check_safemode_cmd)
const KSU_IOCTL_GET_ALLOW_LIST: u64 = 0xC04B06; // _IOWR('K', 6, struct ksu_get_allow_list_cmd)
const KSU_IOCTL_GET_DENY_LIST: u64 = 0xC04B07; // _IOWR('K', 7, struct ksu_get_allow_list_cmd)
const KSU_IOCTL_UID_GRANTED_ROOT: u64 = 0xC04B08; // _IOWR('K', 8, struct ksu_uid_granted_root_cmd)
const KSU_IOCTL_UID_SHOULD_UMOUNT: u64 = 0xC04B09; // _IOWR('K', 9, struct ksu_uid_should_umount_cmd)
const KSU_IOCTL_GET_MANAGER_UID: u64 = 0x804B0A; // _IOR('K', 10, struct ksu_get_manager_uid_cmd)
const KSU_IOCTL_GET_APP_PROFILE: u64 = 0xC04B0B; // _IOWR('K', 11, struct ksu_get_app_profile_cmd)
const KSU_IOCTL_SET_APP_PROFILE: u64 = 0x404B0C; // _IOW('K', 12, struct ksu_set_app_profile_cmd)
const KSU_IOCTL_IS_SU_ENABLED: u64 = 0x804B0D; // _IOR('K', 13, struct ksu_is_su_enabled_cmd)
const KSU_IOCTL_ENABLE_SU: u64 = 0x404B0E; // _IOW('K', 14, struct ksu_enable_su_cmd)

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
static DRIVER_FD: OnceLock<RawFd> = OnceLock::new();
static INFO_CACHE: OnceLock<GetInfoCmd> = OnceLock::new();

const KSU_INSTALL_MAGIC1: u32 = 0xDEADBEEF;
const KSU_INSTALL_MAGIC2: u32 = 0xCAFEBABE;

fn get_driver_fd() -> i32 {
    let mut fd = -1;
    unsafe {
        libc::syscall(libc::SYS_reboot, KSU_INSTALL_MAGIC1, KSU_INSTALL_MAGIC2, 0, &mut fd);
    };
    fd
}
// ioctl wrapper using libc
#[cfg(any(target_os = "linux", target_os = "android"))]
fn ksuctl<T>(request: u64, arg: *mut T) -> std::io::Result<i32> {
    use std::io;

    let fd = *DRIVER_FD.get_or_init(get_driver_fd);

    unsafe {
        let ret = libc::ioctl(fd, request as libc::c_ulong, arg);
        if ret < 0 {
            Err(io::Error::last_os_error())
        } else {
            Ok(ret)
        }
    }
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
fn ksuctl<T>(_request: u64, _arg: *mut T) -> std::io::Result<i32> {
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

pub fn is_safe_mode() -> bool {
    check_kernel_safemode()
}

pub fn set_sepolicy(cmd: &SetSepolicyCmd) -> std::io::Result<()> {
    let mut ioctl_cmd = *cmd;
    ksuctl(KSU_IOCTL_SET_SEPOLICY, &mut ioctl_cmd as *mut _)?;
    Ok(())
}
