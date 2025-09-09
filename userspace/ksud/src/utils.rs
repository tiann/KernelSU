use anyhow::{Context, Error, Ok, Result};
use std::{
    fs::{File, OpenOptions, remove_file, write},
    io::{
        ErrorKind::{NotFound},
        Write,
    },
    path::Path,
    process::Command,
};

use crate::{assets, boot_patch, defs, ksucalls, modsys, restorecon};
use log::warn;
#[allow(unused_imports)]
use std::fs::{Permissions, set_permissions};
#[cfg(unix)]
use std::os::unix::prelude::PermissionsExt;

use hole_punch::*;
use std::io::{Read, Seek, SeekFrom};

use jwalk::WalkDir;
use std::path::PathBuf;

// no top-level rustix imports required; use fully qualified paths

pub fn ensure_clean_dir(dir: impl AsRef<Path>) -> Result<()> {
    ksu_core::utils::ensure_clean_dir(dir).map_err(Error::from)
}

pub fn ensure_file_exists<T: AsRef<Path>>(file: T) -> Result<()> {
    ksu_core::utils::ensure_file_exists(file)
        .map_err(Error::from)
        .with_context(|| "ensure_file_exists failed")
}

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

pub fn getprop(prop: &str) -> Option<String> { ksu_core::props::getprop(prop) }

pub fn is_safe_mode() -> bool {
    let safemode = getprop("persist.sys.safemode")
        .filter(|prop| prop == "1")
        .is_some()
        || getprop("ro.sys.safemode")
            .filter(|prop| prop == "1")
            .is_some();
    log::info!("safemode: {safemode}");
    if safemode {
        return true;
    }
    let safemode = ksucalls::check_kernel_safemode();
    log::info!("kernel_safemode: {safemode}");
    safemode
}

pub fn get_zip_uncompressed_size(zip_path: &str) -> Result<u64> {
    let mut zip = zip::ZipArchive::new(std::fs::File::open(zip_path)?)?;
    let total: u64 = (0..zip.len())
        .map(|i| zip.by_index(i).unwrap().size())
        .sum();
    Ok(total)
}

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn switch_mnt_ns(pid: i32) -> Result<()> {
    ksu_core::sys::switch_mnt_ns(pid).map_err(|e| Error::msg(format!("{e}")))
}

pub fn switch_cgroups() { ksu_core::sys::switch_cgroups() }

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn umask(mask: u32) { ksu_core::sys::umask(mask) }

#[cfg(not(any(target_os = "linux", target_os = "android")))]
pub fn umask(_mask: u32) {
    unimplemented!("umask is not supported on this platform")
}

pub fn has_magisk() -> bool { ksu_core::utils::has_magisk() }

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
    restorecon::lsetfilecon(defs::DAEMON_PATH, restorecon::ADB_CON)?;
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
        use rustix::mount::{unmount, UnmountFlags};
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

// TODO: use libxcp to improve the speed if cross's MSRV is 1.70
pub fn copy_sparse_file<P: AsRef<Path>, Q: AsRef<Path>>(
    src: P,
    dst: Q,
    punch_hole: bool,
) -> Result<()> {
    let mut src_file = File::open(src.as_ref())?;
    let mut dst_file = OpenOptions::new()
        .write(true)
        .create(true)
        .truncate(true)
        .open(dst.as_ref())?;

    dst_file.set_len(src_file.metadata()?.len())?;

    let segments = src_file.scan_chunks()?;
    for segment in segments {
        if let SegmentType::Data = segment.segment_type {
            let start = segment.start;
            let end = segment.end + 1;

            src_file.seek(SeekFrom::Start(start))?;
            dst_file.seek(SeekFrom::Start(start))?;

            let mut buffer = [0; 4096];
            let mut total_bytes_copied = 0;

            while total_bytes_copied < end - start {
                let bytes_to_read =
                    std::cmp::min(buffer.len() as u64, end - start - total_bytes_copied);
                let bytes_read = src_file.read(&mut buffer[..bytes_to_read as usize])?;

                if bytes_read == 0 {
                    break;
                }

                if punch_hole && buffer[..bytes_read].iter().all(|&x| x == 0) {
                    // all zero, don't copy it at all!
                    dst_file.seek(SeekFrom::Current(bytes_read as i64))?;
                    total_bytes_copied += bytes_read as u64;
                    continue;
                }
                dst_file.write_all(&buffer[..bytes_read])?;
                total_bytes_copied += bytes_read as u64;
            }
        }
    }

    Ok(())
}

#[cfg(any(target_os = "linux", target_os = "android"))]
fn copy_xattrs(src_path: impl AsRef<Path>, dest_path: impl AsRef<Path>) -> Result<()> {
    use rustix::path::Arg;
    let std::result::Result::Ok(xattrs) = extattr::llistxattr(src_path.as_ref()) else {
        return Ok(());
    };
    for xattr in xattrs {
        let std::result::Result::Ok(value) = extattr::lgetxattr(src_path.as_ref(), &xattr) else {
            continue;
        };
        log::info!(
            "Set {:?} xattr {} = {}",
            dest_path.as_ref(),
            xattr.to_string_lossy(),
            value.to_string_lossy(),
        );
        if let Err(e) =
            extattr::lsetxattr(dest_path.as_ref(), &xattr, &value, extattr::Flags::empty())
        {
            log::warn!("Failed to set xattr: {e}");
        }
    }
    Ok(())
}

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn copy_module_files(source: impl AsRef<Path>, destination: impl AsRef<Path>) -> Result<()> {
    use rustix::fs::FileTypeExt;
    use rustix::fs::MetadataExt;

    for entry in WalkDir::new(source.as_ref()).into_iter() {
        let entry = entry.context("Failed to access entry")?;
        let source_path = entry.path();
        let relative_path = source_path
            .strip_prefix(source.as_ref())
            .context("Failed to generate relative path")?;
        let dest_path = destination.as_ref().join(relative_path);

        if let Some(parent) = dest_path.parent() {
            std::fs::create_dir_all(parent).context("Failed to create directory")?;
        }

        if entry.file_type().is_file() {
            std::fs::copy(&source_path, &dest_path).with_context(|| {
                format!("Failed to copy file from {source_path:?} to {dest_path:?}",)
            })?;
            copy_xattrs(&source_path, &dest_path)?;
        } else if entry.file_type().is_symlink() {
            if dest_path.exists() {
                std::fs::remove_file(&dest_path).context("Failed to remove file")?;
            }
            let target = std::fs::read_link(entry.path()).context("Failed to read symlink")?;
            log::info!("Symlink: {dest_path:?} -> {target:?}");
            std::os::unix::fs::symlink(target, &dest_path).context("Failed to create symlink")?;
            copy_xattrs(&source_path, &dest_path)?;
        } else if entry.file_type().is_dir() {
            std::fs::create_dir_all(&dest_path)?;
            let metadata = std::fs::metadata(&source_path).context("Failed to read metadata")?;
            std::fs::set_permissions(&dest_path, metadata.permissions())
                .with_context(|| format!("Failed to set permissions for {dest_path:?}"))?;
            copy_xattrs(&source_path, &dest_path)?;
        } else if entry.file_type().is_char_device() {
            if dest_path.exists() {
                std::fs::remove_file(&dest_path).context("Failed to remove file")?;
            }
            let metadata = std::fs::metadata(&source_path).context("Failed to read metadata")?;
            let mode = metadata.permissions().mode();
            let dev = metadata.rdev();
            if dev == 0 {
                log::info!(
                    "Found a char device with major 0: {}",
                    entry.path().display()
                );
                rustix::fs::mknodat(
                    rustix::fs::CWD,
                    &dest_path,
                    rustix::fs::FileType::CharacterDevice,
                    mode.into(),
                    dev,
                )
                .with_context(|| format!("Failed to create device file at {dest_path:?}"))?;
                copy_xattrs(&source_path, &dest_path)?;
            }
        } else {
            log::info!(
                "Unknown file type: {:?}, {:?},",
                entry.file_type(),
                entry.path(),
            );
        }
    }
    Ok(())
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
pub fn copy_module_files(_source: impl AsRef<Path>, _destination: impl AsRef<Path>) -> Result<()> {
    unimplemented!()
}
