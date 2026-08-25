#![allow(clippy::unreadable_literal)]
use anyhow::bail;

use crate::ksu_uapi;
use std::fs;
use std::os::fd::{AsRawFd, FromRawFd, OwnedFd, RawFd};
use std::sync::{Mutex, MutexGuard, OnceLock};

enum DriverFdState {
    Empty,
    Owned(OwnedFd),
    TakenForUnload,
}

static DRIVER_FD: Mutex<DriverFdState> = Mutex::new(DriverFdState::Empty);
static INFO_CACHE: OnceLock<ksu_uapi::ksu_get_info_cmd> = OnceLock::new();

fn lock_driver_fd() -> MutexGuard<'static, DriverFdState> {
    DRIVER_FD
        .lock()
        .unwrap_or_else(std::sync::PoisonError::into_inner)
}

fn dup_fd_cloexec(fd: RawFd) -> Option<OwnedFd> {
    let duplicated = unsafe { libc::fcntl(fd, libc::F_DUPFD_CLOEXEC, 0) };
    (duplicated >= 0).then(|| unsafe { OwnedFd::from_raw_fd(duplicated) })
}

fn scan_driver_fd() -> Option<OwnedFd> {
    let fd_dir = fs::read_dir("/proc/self/fd").ok()?;

    for entry in fd_dir.flatten() {
        if let Ok(fd_num) = entry.file_name().to_string_lossy().parse::<i32>() {
            let link_path = format!("/proc/self/fd/{fd_num}");
            if let Ok(target) = fs::read_link(&link_path) {
                let target_str = target.to_string_lossy();
                if target_str.contains("[ksu_driver]")
                    && let Some(duplicated) = dup_fd_cloexec(fd_num)
                {
                    // Never create a second OwnedFd for the same raw descriptor.
                    // The duplicate has independent ownership even if the
                    // descriptor discovered through /proc closes concurrently.
                    return Some(duplicated);
                }
            }
        }
    }

    None
}

fn request_driver_fd() -> Option<OwnedFd> {
    let mut fd = -1;
    unsafe {
        libc::syscall(
            libc::SYS_reboot,
            ksu_uapi::KSU_INSTALL_MAGIC1,
            ksu_uapi::KSU_INSTALL_MAGIC2,
            0,
            &mut fd,
        );
    };
    (fd >= 0).then(|| unsafe { OwnedFd::from_raw_fd(fd) })
}

fn init_driver_fd() -> Option<OwnedFd> {
    scan_driver_fd().or_else(request_driver_fd)
}

fn ensure_driver_fd_locked(state: &mut DriverFdState) -> Option<RawFd> {
    if matches!(&*state, DriverFdState::Empty)
        && let Some(fd) = init_driver_fd()
    {
        *state = DriverFdState::Owned(fd);
    }

    match state {
        DriverFdState::Owned(fd) => Some(fd.as_raw_fd()),
        DriverFdState::Empty | DriverFdState::TakenForUnload => None,
    }
}

pub fn install_driver_fd() -> Option<OwnedFd> {
    request_driver_fd()
}

pub fn take_driver_fd() -> Option<OwnedFd> {
    let mut state = lock_driver_fd();
    ensure_driver_fd_locked(&mut state)?;

    match std::mem::replace(&mut *state, DriverFdState::TakenForUnload) {
        DriverFdState::Owned(fd) => Some(fd),
        other => {
            *state = other;
            None
        }
    }
}

pub fn restore_driver_fd(fd: OwnedFd) {
    let mut state = lock_driver_fd();
    if matches!(
        &*state,
        DriverFdState::TakenForUnload | DriverFdState::Empty
    ) {
        *state = DriverFdState::Owned(fd);
    }
}

fn ksuctl_fd<T>(fd: RawFd, request: u32, arg: *mut T) -> std::io::Result<i32> {
    use std::io;

    unsafe {
        let ret = libc::ioctl(fd as libc::c_int, request as i32, arg);
        if ret < 0 {
            Err(io::Error::last_os_error())
        } else {
            Ok(ret)
        }
    }
}

#[allow(clippy::significant_drop_tightening)]
fn ksuctl<T>(request: u32, arg: *mut T) -> std::io::Result<i32> {
    use std::io;

    // Keep the cache lock through ioctl so unload cannot take and close this
    // descriptor while the syscall is using its raw fd number.
    let mut state = lock_driver_fd();
    if matches!(&*state, DriverFdState::TakenForUnload) {
        return Err(io::Error::from_raw_os_error(libc::EBUSY));
    }
    let fd = ensure_driver_fd_locked(&mut state).unwrap_or(-1);
    ksuctl_fd(fd, request, arg)
}

pub fn get_info() -> ksu_uapi::ksu_get_info_cmd {
    *INFO_CACHE.get_or_init(|| {
        let mut cmd = ksu_uapi::ksu_get_info_cmd {
            version: 0,
            flags: 0,
            features: 0,
            uapi_version: 0,
        };
        if ksuctl(ksu_uapi::KSU_IOCTL_GET_INFO, &raw mut cmd).is_err() {
            let _ = ksuctl(ksu_uapi::KSU_IOCTL_GET_INFO_LEGACY, &raw mut cmd);
        }
        cmd
    })
}

pub fn get_version() -> i32 {
    get_info().version as i32
}

pub fn is_late_load() -> bool {
    get_info().flags & ksu_uapi::KSU_GET_INFO_FLAG_LATE_LOAD != 0
}

pub fn is_lkm() -> bool {
    get_info().flags & ksu_uapi::KSU_GET_INFO_FLAG_LKM != 0
}

pub const fn uapi_version() -> u32 {
    ksu_uapi::KERNEL_SU_UAPI_VERSION
}

pub fn runtime_mode() -> &'static str {
    if is_late_load() {
        "late-load"
    } else if is_lkm() {
        "lkm"
    } else {
        "built-in"
    }
}

pub fn ensure_uapi_version_matched() -> anyhow::Result<()> {
    let kernel_uapi = get_info().uapi_version;
    let userspace_uapi = uapi_version();
    if kernel_uapi != userspace_uapi {
        bail!(
            "UAPI version mismatch: kernel={kernel_uapi}, ksud={userspace_uapi}. Please update KernelSU!"
        );
    }
    Ok(())
}

pub fn grant_root() -> std::io::Result<()> {
    ksuctl(ksu_uapi::KSU_IOCTL_GRANT_ROOT, std::ptr::null_mut::<u8>())?;
    Ok(())
}

fn report_event(event: u32) {
    let mut cmd = ksu_uapi::ksu_report_event_cmd { event };
    let _ = ksuctl(ksu_uapi::KSU_IOCTL_REPORT_EVENT, &raw mut cmd);
}

pub fn report_post_fs_data() {
    report_event(ksu_uapi::EVENT_POST_FS_DATA);
}

pub fn report_boot_complete() {
    report_event(ksu_uapi::EVENT_BOOT_COMPLETED);
}

pub fn report_module_mounted() {
    report_event(ksu_uapi::EVENT_MODULE_MOUNTED);
}

pub fn check_kernel_safemode() -> bool {
    let mut cmd = ksu_uapi::ksu_check_safemode_cmd { in_safe_mode: 0 };
    let _ = ksuctl(ksu_uapi::KSU_IOCTL_CHECK_SAFEMODE, &raw mut cmd);
    cmd.in_safe_mode != 0
}

pub fn set_sepolicy(payload: *const u8, payload_len: u64) -> std::io::Result<i32> {
    let mut ioctl_cmd = crate::ksu_uapi::ksu_set_sepolicy_cmd {
        data_len: payload_len,
        data: payload as u64,
    };

    ksuctl(ksu_uapi::KSU_IOCTL_SET_SEPOLICY, &raw mut ioctl_cmd)
}

pub fn get_feature(feature_id: u32) -> std::io::Result<(u64, bool)> {
    let mut cmd = ksu_uapi::ksu_get_feature_cmd {
        feature_id,
        value: 0,
        supported: 0,
    };
    ksuctl(ksu_uapi::KSU_IOCTL_GET_FEATURE, &raw mut cmd)?;
    Ok((cmd.value, cmd.supported != 0))
}

pub fn set_feature(feature_id: u32, value: u64) -> std::io::Result<()> {
    let mut cmd = ksu_uapi::ksu_set_feature_cmd { feature_id, value };
    ksuctl(ksu_uapi::KSU_IOCTL_SET_FEATURE, &raw mut cmd)?;
    Ok(())
}

pub fn get_wrapped_fd(fd: RawFd) -> std::io::Result<RawFd> {
    let mut cmd = ksu_uapi::ksu_get_wrapper_fd_cmd {
        fd: fd as u32,
        flags: 0,
    };
    let result = ksuctl(ksu_uapi::KSU_IOCTL_GET_WRAPPER_FD, &raw mut cmd)?;
    Ok(result)
}

pub fn get_sulog_fd() -> std::io::Result<RawFd> {
    let mut cmd = ksu_uapi::ksu_get_sulog_fd_cmd { flags: 0 };
    let result = ksuctl(ksu_uapi::KSU_IOCTL_GET_SULOG_FD, &raw mut cmd)?;
    Ok(result)
}

pub fn mark_get(pid: i32) -> std::io::Result<u32> {
    let mut cmd = ksu_uapi::ksu_manage_mark_cmd {
        operation: ksu_uapi::KSU_MARK_GET,
        pid,
        result: 0,
    };
    ksuctl(ksu_uapi::KSU_IOCTL_MANAGE_MARK, &raw mut cmd)?;
    Ok(cmd.result)
}

pub fn mark_set(pid: i32) -> std::io::Result<()> {
    let mut cmd = ksu_uapi::ksu_manage_mark_cmd {
        operation: ksu_uapi::KSU_MARK_MARK,
        pid,
        result: 0,
    };
    ksuctl(ksu_uapi::KSU_IOCTL_MANAGE_MARK, &raw mut cmd)?;
    Ok(())
}

pub fn mark_unset(pid: i32) -> std::io::Result<()> {
    let mut cmd = ksu_uapi::ksu_manage_mark_cmd {
        operation: ksu_uapi::KSU_MARK_UNMARK,
        pid,
        result: 0,
    };
    ksuctl(ksu_uapi::KSU_IOCTL_MANAGE_MARK, &raw mut cmd)?;
    Ok(())
}

pub fn mark_refresh() -> std::io::Result<()> {
    let mut cmd = ksu_uapi::ksu_manage_mark_cmd {
        operation: ksu_uapi::KSU_MARK_REFRESH,
        pid: 0,
        result: 0,
    };
    ksuctl(ksu_uapi::KSU_IOCTL_MANAGE_MARK, &raw mut cmd)?;
    Ok(())
}

pub fn nuke_ext4_sysfs(mnt: &str) -> anyhow::Result<()> {
    let c_mnt = std::ffi::CString::new(mnt)?;
    let mut ioctl_cmd = crate::ksu_uapi::ksu_nuke_ext4_sysfs_cmd {
        arg: c_mnt.as_ptr() as u64,
    };
    ksuctl(ksu_uapi::KSU_IOCTL_NUKE_EXT4_SYSFS, &raw mut ioctl_cmd)?;
    Ok(())
}

pub fn umount_list_wipe() -> std::io::Result<()> {
    let mut cmd = ksu_uapi::ksu_add_try_umount_cmd {
        arg: 0,
        flags: 0,
        mode: ksu_uapi::KSU_UMOUNT_WIPE,
    };
    ksuctl(ksu_uapi::KSU_IOCTL_ADD_TRY_UMOUNT, &raw mut cmd)?;
    Ok(())
}

pub fn umount_list_managed_wipe() -> std::io::Result<()> {
    let mut cmd = ksu_uapi::ksu_add_try_umount_cmd {
        arg: 0,
        flags: 0,
        mode: ksu_uapi::KSU_UMOUNT_MANAGED_WIPE,
    };
    ksuctl(ksu_uapi::KSU_IOCTL_ADD_TRY_UMOUNT, &raw mut cmd)?;
    Ok(())
}

pub fn umount_list_add(path: &str, flags: u32) -> anyhow::Result<()> {
    let c_path = std::ffi::CString::new(path)?;
    let mut cmd = ksu_uapi::ksu_add_try_umount_cmd {
        arg: c_path.as_ptr() as u64,
        flags,
        mode: ksu_uapi::KSU_UMOUNT_ADD,
    };
    ksuctl(ksu_uapi::KSU_IOCTL_ADD_TRY_UMOUNT, &raw mut cmd)?;
    Ok(())
}

pub fn umount_list_managed_set(path: &str, layers: u32) -> anyhow::Result<()> {
    let c_path = std::ffi::CString::new(path)?;
    let mut cmd = ksu_uapi::ksu_add_try_umount_cmd {
        arg: c_path.as_ptr() as u64,
        flags: layers,
        mode: ksu_uapi::KSU_UMOUNT_MANAGED_SET,
    };
    ksuctl(ksu_uapi::KSU_IOCTL_ADD_TRY_UMOUNT, &raw mut cmd)?;
    Ok(())
}

pub fn umount_list_del(path: &str) -> anyhow::Result<()> {
    let c_path = std::ffi::CString::new(path)?;
    let mut cmd = ksu_uapi::ksu_add_try_umount_cmd {
        arg: c_path.as_ptr() as u64,
        flags: 0,
        mode: ksu_uapi::KSU_UMOUNT_DEL,
    };
    ksuctl(ksu_uapi::KSU_IOCTL_ADD_TRY_UMOUNT, &raw mut cmd)?;
    Ok(())
}

pub fn set_init_pgrp() -> std::io::Result<()> {
    ksuctl(
        ksu_uapi::KSU_IOCTL_SET_INIT_PGRP,
        std::ptr::null_mut::<u8>(),
    )?;
    Ok(())
}

pub fn set_ksu_no_new_privs() -> anyhow::Result<()> {
    let result = ksuctl(
        ksu_uapi::KSU_IOCTL_DISABLE_ESCAPE_TO_ROOT,
        std::ptr::null_mut::<u8>(),
    )?;
    if result != 0 {
        bail!("unexpected result: {result}");
    }
    Ok(())
}

pub fn prepare_unload_on(fd: RawFd) -> std::io::Result<()> {
    ksuctl_fd(
        fd,
        ksu_uapi::KSU_IOCTL_PREPARE_UNLOAD,
        std::ptr::null_mut::<u8>(),
    )?;
    Ok(())
}

pub fn commit_unload_on(fd: RawFd) -> std::io::Result<()> {
    ksuctl_fd(
        fd,
        ksu_uapi::KSU_IOCTL_COMMIT_UNLOAD,
        std::ptr::null_mut::<u8>(),
    )?;
    Ok(())
}

pub fn abort_unload_on(fd: RawFd) -> std::io::Result<()> {
    ksuctl_fd(
        fd,
        ksu_uapi::KSU_IOCTL_ABORT_UNLOAD,
        std::ptr::null_mut::<u8>(),
    )?;
    Ok(())
}
