#![allow(dead_code)]

use std::fs;
use std::os::unix::io::RawFd;
use std::sync::OnceLock;

// Event constants
const EVENT_POST_FS_DATA: u64 = 1;
const EVENT_BOOT_COMPLETED: u64 = 2;
const EVENT_MODULE_MOUNTED: u64 = 3;

// Constants
const KSU_APP_PROFILE_VER: u32 = 2;
const KSU_MAX_PACKAGE_NAME: usize = 256;
const KSU_MAX_GROUPS: usize = 32;
const KSU_SELINUX_DOMAIN: usize = 64;

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

// Data structures
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct RootProfile {
    pub uid: i32,
    pub gid: i32,
    pub groups_count: i32,
    pub groups: [i32; KSU_MAX_GROUPS],
    pub capabilities: Capabilities,
    pub selinux_domain: [u8; KSU_SELINUX_DOMAIN],
    pub namespaces: i32,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct Capabilities {
    pub effective: u64,
    pub permitted: u64,
    pub inheritable: u64,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct NonRootProfile {
    pub umount_modules: bool,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct RootProfileConfig {
    pub use_default: bool,
    pub template_name: [u8; KSU_MAX_PACKAGE_NAME],
    pub profile: RootProfile,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct NonRootProfileConfig {
    pub use_default: bool,
    pub profile: NonRootProfile,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub union ProfileConfig {
    pub rp_config: RootProfileConfig,
    pub nrp_config: NonRootProfileConfig,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct AppProfile {
    pub version: u32,
    pub key: [u8; KSU_MAX_PACKAGE_NAME],
    pub current_uid: i32,
    pub allow_su: bool,
    pub config: ProfileConfig,
}

// Command structures
#[repr(C)]
#[derive(Clone, Copy)]
struct GetInfoCmd {
    version: u32,
    flags: u32,
}

#[repr(C)]
struct ReportEventCmd {
    event: u32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SetSepolicyCmd {
    pub cmd: u64,
    pub arg: u64,
}

#[repr(C)]
struct CheckSafemodeCmd {
    in_safe_mode: u8,
}

#[repr(C)]
pub struct GetAllowListCmd {
    pub uids: [u32; 128],
    pub count: u32,
    pub allow: u8,
}

#[repr(C)]
struct UidGrantedRootCmd {
    uid: u32,
    granted: u8,
}

#[repr(C)]
struct UidShouldUmountCmd {
    uid: u32,
    should_umount: u8,
}

#[repr(C)]
struct GetManagerUidCmd {
    uid: u32,
}

#[repr(C)]
struct SetManagerUidCmd {
    uid: u32,
}

#[repr(C)]
struct GetAppProfileCmd {
    profile: AppProfile,
}

#[repr(C)]
struct SetAppProfileCmd {
    profile: AppProfile,
}

#[repr(C)]
struct IsSuEnabledCmd {
    enabled: u8,
}

#[repr(C)]
struct EnableSuCmd {
    enable: u8,
}

// Global driver fd cache
static DRIVER_FD: OnceLock<RawFd> = OnceLock::new();
static INFO_CACHE: OnceLock<GetInfoCmd> = OnceLock::new();

// Scan /proc/self/fd to find [ksu_driver]
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
fn get_driver_fd() -> Option<RawFd> {
    DRIVER_FD.get_or_init(|| scan_driver_fd().unwrap_or(-1)).copied()
        .filter(|&fd| fd >= 0)
}

// ioctl wrapper using libc
#[cfg(any(target_os = "linux", target_os = "android"))]
fn ksuctl<T>(request: u64, arg: *mut T) -> std::io::Result<i32> {
    use std::io;

    let fd = get_driver_fd().ok_or_else(|| io::Error::from_raw_os_error(libc::ENODEV))?;

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
    INFO_CACHE.get_or_init(|| {
        let mut cmd = GetInfoCmd {
            version: 0,
            flags: 0,
        };
        let _ = ksuctl(KSU_IOCTL_GET_INFO, &mut cmd as *mut _);
        cmd
    }).clone()
}

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn get_version() -> i32 {
    get_info().version as i32
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
pub fn get_version() -> i32 {
    0
}

pub fn is_lkm_mode() -> bool {
    (get_info().flags & 0x1) != 0
}

pub fn is_manager() -> bool {
    (get_info().flags & 0x2) != 0
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
fn report_event(event: u64) {
    let mut cmd = ReportEventCmd { event: event as u32 };
    let _ = ksuctl(KSU_IOCTL_REPORT_EVENT, &mut cmd as *mut _);
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
fn report_event(_event: u64) {}

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

pub fn uid_should_umount(uid: u32) -> bool {
    let mut cmd = UidShouldUmountCmd {
        uid,
        should_umount: 0,
    };
    let _ = ksuctl(KSU_IOCTL_UID_SHOULD_UMOUNT, &mut cmd as *mut _);
    cmd.should_umount != 0
}

pub fn uid_granted_root(uid: u32) -> bool {
    let mut cmd = UidGrantedRootCmd { uid, granted: 0 };
    let _ = ksuctl(KSU_IOCTL_UID_GRANTED_ROOT, &mut cmd as *mut _);
    cmd.granted != 0
}

pub fn get_allow_list() -> Option<Vec<u32>> {
    let mut cmd = GetAllowListCmd {
        uids: [0; 128],
        count: 0,
        allow: 1,
    };

    if ksuctl(KSU_IOCTL_GET_ALLOW_LIST, &mut cmd as *mut _).is_ok() {
        Some(cmd.uids[..cmd.count as usize].to_vec())
    } else {
        None
    }
}

pub fn get_deny_list() -> Option<Vec<u32>> {
    let mut cmd = GetAllowListCmd {
        uids: [0; 128],
        count: 0,
        allow: 0,
    };

    if ksuctl(KSU_IOCTL_GET_DENY_LIST, &mut cmd as *mut _).is_ok() {
        Some(cmd.uids[..cmd.count as usize].to_vec())
    } else {
        None
    }
}

pub fn get_manager_uid() -> Option<u32> {
    let mut cmd = GetManagerUidCmd { uid: 0 };

    if ksuctl(KSU_IOCTL_GET_MANAGER_UID, &mut cmd as *mut _).is_ok() {
        Some(cmd.uid)
    } else {
        None
    }
}

pub fn set_manager_uid(uid: u32) -> std::io::Result<()> {
    let mut cmd = SetManagerUidCmd { uid };
    ksuctl(KSU_IOCTL_GET_MANAGER_UID, &mut cmd as *mut _)?;
    Ok(())
}

pub fn get_app_profile(profile: &mut AppProfile) -> std::io::Result<()> {
    let mut cmd = GetAppProfileCmd {
        profile: *profile,
    };

    ksuctl(KSU_IOCTL_GET_APP_PROFILE, &mut cmd as *mut _)?;
    *profile = cmd.profile;
    Ok(())
}

pub fn set_app_profile(profile: &AppProfile) -> std::io::Result<()> {
    let mut cmd = SetAppProfileCmd {
        profile: *profile,
    };

    ksuctl(KSU_IOCTL_SET_APP_PROFILE, &mut cmd as *mut _)?;
    Ok(())
}

pub fn is_su_enabled() -> bool {
    let mut cmd = IsSuEnabledCmd { enabled: 0 };

    if ksuctl(KSU_IOCTL_IS_SU_ENABLED, &mut cmd as *mut _).is_ok() {
        cmd.enabled != 0
    } else {
        false
    }
}

pub fn set_su_enabled(enabled: bool) -> std::io::Result<()> {
    let mut cmd = EnableSuCmd {
        enable: if enabled { 1 } else { 0 },
    };

    ksuctl(KSU_IOCTL_ENABLE_SU, &mut cmd as *mut _)?;
    Ok(())
}

pub fn set_sepolicy(cmd: &SetSepolicyCmd) -> std::io::Result<()> {
    let mut ioctl_cmd = *cmd;
    ksuctl(KSU_IOCTL_SET_SEPOLICY, &mut ioctl_cmd as *mut _)?;
    Ok(())
}
