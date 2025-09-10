use anyhow::{Context, Error, Ok, Result};
use std::{
    fs::{remove_file, write},
    io::ErrorKind::NotFound,
    path::Path,
    process::Command,
};

use crate::{assets, boot_patch, defs, modsys};
use log::warn;
#[allow(unused_imports)]
use std::fs::{Permissions, set_permissions};
#[cfg(unix)]
use std::os::unix::prelude::PermissionsExt;

use std::path::PathBuf;

// no top-level rustix imports required; use fully qualified paths

pub fn ensure_dir_exists<T: AsRef<Path>>(dir: T) -> Result<()> {
    ksu_core::utils::ensure_dir_exists(dir)
        .map_err(Error::from)
        .with_context(|| "ensure_dir_exists failed")
}

pub fn ensure_binary<T: AsRef<Path>>(
    path: T,
    contents: &[u8],
    ignore_if_exist: bool,
) -> Result<()> {
    if ignore_if_exist && path.as_ref().exists() {
        return Ok(());
    }

    ensure_dir_exists(path.as_ref().parent().ok_or_else(|| {
        anyhow::anyhow!(
            "{} does not have parent directory",
            path.as_ref().to_string_lossy()
        )
    })?)?;

    if let Err(e) = remove_file(path.as_ref()) {
        if e.kind() != NotFound {
            return Err(Error::from(e))
                .with_context(|| format!("failed to unlink {}", path.as_ref().display()));
        }
    }

    write(&path, contents)?;
    #[cfg(unix)]
    set_permissions(&path, Permissions::from_mode(0o755))?;
    Ok(())
}

// getprop and is_safe_mode are provided by ksu-core

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn switch_mnt_ns(pid: i32) -> Result<()> {
    ksu_core::sys::switch_mnt_ns(pid).map_err(|e| Error::msg(format!("{e}")))
}

pub fn switch_cgroups() {
    ksu_core::sys::switch_cgroups()
}

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn umask(mask: u32) {
    ksu_core::sys::umask(mask)
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
pub fn umask(_mask: u32) {
    unimplemented!("umask is not supported on this platform")
}

pub fn has_magisk() -> bool {
    ksu_core::utils::has_magisk()
}

#[cfg(target_os = "android")]
fn link_ksud_to_bin() -> Result<()> {
    let ksu_bin = PathBuf::from(defs::DAEMON_PATH);
    let ksu_bin_link = PathBuf::from(defs::DAEMON_LINK_PATH);
    if ksu_bin.exists() && !ksu_bin_link.exists() {
        std::os::unix::fs::symlink(&ksu_bin, &ksu_bin_link)?;
    }
    Ok(())
}

pub fn install(magiskboot: Option<PathBuf>) -> Result<()> {
    ensure_dir_exists(defs::ADB_DIR)?;
    std::fs::copy("/proc/self/exe", defs::DAEMON_PATH)?;
    ksu_core::restorecon::lsetfilecon(defs::DAEMON_PATH, ksu_core::restorecon::ADB_CON)?;
    // install binary assets
    assets::ensure_binaries(false).with_context(|| "Failed to extract assets")?;

    #[cfg(target_os = "android")]
    link_ksud_to_bin()?;

    if let Some(magiskboot) = magiskboot {
        ensure_dir_exists(defs::BINARY_DIR)?;
        let _ = std::fs::copy(magiskboot, defs::MAGISKBOOT_PATH);
    }

    Ok(())
}

pub fn uninstall(magiskboot_path: Option<PathBuf>) -> Result<()> {
    if Path::new(defs::MODULE_DIR).exists() {
        println!("- Uninstall modules..");
        // Initialize modsys and delegate module cleanup
        if let Err(e) = modsys::init() {
            warn!("Failed to initialize modsys during uninstall: {e}");
        }
        // Note: We don't call modsys functions here as they require the module system to be working
        // Instead, we'll do manual cleanup of module directories
    }
    println!("- Removing directories..");
    std::fs::remove_dir_all(defs::WORKING_DIR).ok();
    std::fs::remove_file(defs::DAEMON_PATH).ok();
    // 卸载模块目录（若存在挂载）
    #[cfg(any(target_os = "linux", target_os = "android"))]
    {
        use rustix::mount::{UnmountFlags, unmount};
        let _ = unmount(defs::MODULE_DIR, UnmountFlags::DETACH);
    }
    std::fs::remove_dir_all(defs::MODULE_DIR).ok();
    std::fs::remove_dir_all(defs::MODULE_UPDATE_TMP_DIR).ok();
    println!("- Restore boot image..");
    boot_patch::restore(None, magiskboot_path, true)?;
    println!("- Uninstall KernelSU manager..");
    Command::new("pm")
        .args(["uninstall", "me.weishu.kernelsu"])
        .spawn()?;
    println!("- Rebooting in 5 seconds..");
    std::thread::sleep(std::time::Duration::from_secs(5));
    Command::new("reboot").spawn()?;
    Ok(())
}

// copy_sparse_file and copy_module_files helpers live in overlay implementation
