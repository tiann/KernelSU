// Utility functions migrated from ksud
use anyhow::{Context, Error, Ok, Result};
use std::{
    fs::{File, OpenOptions},
    io::Write,
    path::Path,
};

#[allow(unused_imports)]
use std::fs::{Permissions, set_permissions};

use hole_punch::*;
use std::io::{Read, Seek, SeekFrom};

use jwalk::WalkDir;

#[cfg(any(target_os = "linux", target_os = "android"))]
use std::os::unix::fs::PermissionsExt;

// no top-level rustix imports required; use fully qualified paths where needed

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

pub fn getprop(prop: &str) -> Option<String> { ksu_core::props::getprop(prop) }

pub fn get_zip_uncompressed_size(zip_path: &str) -> Result<u64> {
    ksu_core::utils::get_zip_uncompressed_size(zip_path)
        .map_err(|e| Error::msg(format!("{e}")))
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

/// Check if system is in safe mode
pub fn is_safe_mode() -> bool {
    // 优先询问内核侧的安全模式
    let kernel_safe = crate::ksucalls::check_kernel_safemode();
    if kernel_safe {
        log::info!("kernel safemode: true");
        return true;
    }
    // 退回到属性判断
    let safemode = getprop("persist.sys.safemode")
        .filter(|prop| prop == "1")
        .is_some()
        || getprop("ro.sys.safemode")
            .filter(|prop| prop == "1")
            .is_some();
    log::info!("safemode: {safemode}");
    safemode
}

/// Ensure boot completed before module operations
pub fn ensure_boot_completed() -> Result<()> {
    // ensure getprop sys.boot_completed == 1
    if getprop("sys.boot_completed").as_deref() != Some("1") {
        anyhow::bail!("Android is Booting!");
    }
    Ok(())
}

// 元模块安全机制标志：在 ksud 的 post-fs-data 创建、boot-completed 清理
// 留存表示上次启动未完成，下一次应降级运行模块系统
pub fn check_metamodule_safety() -> bool { ksu_core::safety::exists() }
