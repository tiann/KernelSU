use anyhow::{Ok, Result};

#[cfg(any(target_os = "linux", target_os = "android"))]
use anyhow::Context;
#[cfg(any(target_os = "linux", target_os = "android"))]
use retry::delay::NoDelay;
#[cfg(any(target_os = "linux", target_os = "android"))]
use sys_mount::{unmount, FilesystemType, Mount, MountFlags, Unmount, UnmountFlags};

#[cfg(any(target_os = "linux", target_os = "android"))]
use std::fs::File;
use std::os::fd::AsRawFd;
use std::os::unix::fs::OpenOptionsExt;
use std::path::{Path, PathBuf};
use log::{info, warn};
use procfs::process::Process;
use crate::defs::KSU_OVERLAY_SOURCE;
#[cfg(any(target_os = "linux", target_os = "android"))]

pub struct AutoMountExt4 {
    mnt: String,
    #[cfg(any(target_os = "linux", target_os = "android"))]
    mount: Option<Mount>,
    auto_umount: bool,
}

impl AutoMountExt4 {
    #[cfg(any(target_os = "linux", target_os = "android"))]
    pub fn try_new(src: &str, mnt: &str, auto_umount: bool) -> Result<Self> {
        let result = Mount::builder()
            .fstype(FilesystemType::from("ext4"))
            .flags(MountFlags::empty())
            .mount(src, mnt)
            .map(|mount| {
                Ok(Self {
                    mnt: mnt.to_string(),
                    mount: Some(mount),
                    auto_umount,
                })
            });
        if let Err(e) = result {
            println!("- Mount failed: {e}, retry with system mount");
            let result = std::process::Command::new("mount")
                .arg("-t")
                .arg("ext4")
                .arg(src)
                .arg(mnt)
                .status();
            if let Err(e) = result {
                Err(anyhow::anyhow!(
                    "mount partition: {src} -> {mnt} failed: {e}"
                ))
            } else {
                Ok(Self {
                    mnt: mnt.to_string(),
                    mount: None,
                    auto_umount,
                })
            }
        } else {
            result.unwrap()
        }
    }

    #[cfg(not(any(target_os = "linux", target_os = "android")))]
    pub fn try_new(_src: &str, _mnt: &str, _auto_umount: bool) -> Result<Self> {
        unimplemented!()
    }

    #[cfg(any(target_os = "linux", target_os = "android"))]
    pub fn umount(&self) -> Result<()> {
        if let Some(ref mount) = self.mount {
            mount
                .unmount(UnmountFlags::empty())
                .map_err(|e| anyhow::anyhow!(e))
        } else {
            let result = std::process::Command::new("umount").arg(&self.mnt).status();
            if let Err(e) = result {
                Err(anyhow::anyhow!("umount: {} failed: {e}", self.mnt))
            } else {
                Ok(())
            }
        }
    }
}

#[cfg(any(target_os = "linux", target_os = "android"))]
impl Drop for AutoMountExt4 {
    fn drop(&mut self) {
        log::info!(
            "AutoMountExt4 drop: {}, auto_umount: {}",
            self.mnt,
            self.auto_umount
        );
        if self.auto_umount {
            let _ = self.umount();
        }
    }
}

#[allow(dead_code)]
#[cfg(any(target_os = "linux", target_os = "android"))]
fn mount_image(src: &str, target: &str, autodrop: bool) -> Result<()> {
    if autodrop {
        Mount::builder()
            .fstype(FilesystemType::from("ext4"))
            .mount_autodrop(src, target, UnmountFlags::empty())
            .with_context(|| format!("Failed to do mount: {src} -> {target}"))?;
    } else {
        Mount::builder()
            .fstype(FilesystemType::from("ext4"))
            .mount(src, target)
            .with_context(|| format!("Failed to do mount: {src} -> {target}"))?;
    }
    Ok(())
}

#[allow(dead_code)]
#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn mount_ext4(src: &str, target: &str, autodrop: bool) -> Result<()> {
    // umount target first.
    let _ = umount_dir(target);
    let result = retry::retry(NoDelay.take(3), || mount_image(src, target, autodrop));
    result
        .map_err(|e| anyhow::anyhow!("mount partition: {src} -> {target} failed: {e}"))
        .map(|_| ())
}

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn umount_dir(src: &str) -> Result<()> {
    unmount(src, UnmountFlags::empty()).with_context(|| format!("Failed to umount {src}"))?;
    Ok(())
}

#[cfg(any(target_os = "linux", target_os = "android"))]
fn mount_overlayfs(lower_dirs: &Vec<String>, lowest: impl AsRef<str>, dest: impl AsRef<str>) -> Result<()> {
    let options = format!("lowerdir={}:{}", lower_dirs.join(":"), lowest.as_ref());
    info!("mount overlayfs on {}, options={}", dest.as_ref(), options);
    Mount::builder()
        .fstype(FilesystemType::from("overlay"))
        .data(&options)
        .mount(KSU_OVERLAY_SOURCE, dest.as_ref())
        .with_context(|| format!("mount overlayfs on {} options {} failed", dest.as_ref(), options))?;
    Ok(())
}

#[cfg(any(target_os = "linux", target_os = "android"))]
fn bind_mount(from: impl AsRef<Path>, to: impl AsRef<Path>) -> Result<()> {
    info!("bind mount {} -> {}", from.as_ref().display(), to.as_ref().display());
    Mount::builder()
        .flags(MountFlags::BIND)
        .mount(from.as_ref(), to.as_ref())
        .with_context(|| format!("bind mount failed: {} -> {}", from.as_ref().display(), to.as_ref().display()))?;
    Ok(())
}

#[cfg(any(target_os = "linux", target_os = "android"))]
fn mount_overlay_child(mount_point: &str, relative: &String, module_roots: &Vec<String>, stock_root: &String) -> Result<()> {
    if !module_roots.iter().any(|lower| Path::new(&format!("{}{}", lower, relative)).exists()) {
        bind_mount(&stock_root, mount_point)?;
    }
    if !Path::new(&stock_root).is_dir() {
        return Ok(());
    }
    let mut lower_dirs: Vec<String> = vec![];
    for lower in module_roots {
        let lower_dir = format!("{}{}", lower, relative);
        let path = Path::new(&lower_dir);
        if path.is_dir() {
            lower_dirs.push(lower_dir);
        } else if path.exists() {
            // stock root has been blocked by this file
            return Ok(());
        }
    }
    if lower_dirs.is_empty() {
        return Ok(());
    }
    // merge modules and stock
    if let Err(e) = mount_overlayfs(&lower_dirs, &stock_root, &mount_point) {
        warn!("failed: {:#}, fallback to bind mount", e);
        bind_mount(&stock_root, &mount_point)?;
    }
    Ok(())
}

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn mount_overlay(dest: &PathBuf, module_roots: &Vec<String>, root_mounted: &mut bool) -> Result<()> {
    let root = dest.to_str().unwrap();
    let dest = PathBuf::from(format!("{}/", root));
    info!("mount overlay for {}", root);
    let stock_root = File::options()
        .read(true)
        .custom_flags(libc::O_PATH)
        .open(&dest)?; // this have ensured it is a dir
    let stock_root = format!("/proc/self/fd/{}", stock_root.as_raw_fd());

    // collect child mounts before mounting the root
    let mounts = Process::myself()?.mountinfo()?;
    let mut mount_seq: Vec<&str> = mounts.iter()
        .filter(|m| m.mount_point.starts_with(&dest))
        .map(|m| m.mount_point.to_str().unwrap())
        .collect::<Vec<&str>>();
    mount_seq.sort();
    mount_seq.dedup();

    mount_overlayfs(module_roots, root, root)?;
    *root_mounted = true;
    for mount_point in mount_seq.iter() {
        let relative = mount_point.replacen(root, "", 1);
        let stock_root: String = format!("{}{}", stock_root, relative);
        if Path::new(&stock_root).exists() {
            mount_overlay_child(mount_point, &relative, &module_roots, &stock_root)?;
        }
    }
    Ok(())
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
pub fn mount_ext4(_src: &str, _target: &str, _autodrop: bool) -> Result<()> {
    unimplemented!()
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
pub fn umount_dir(_src: &str) -> Result<()> {
    unimplemented!()
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
pub fn mount_overlay(_dest: &PathBuf, _lower_dirs: &Vec<String>, _root_mounted: &mut bool) -> Result<()> {
    unimplemented!()
}

