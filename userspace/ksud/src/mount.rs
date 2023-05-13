use anyhow::{bail, Ok, Result};

#[cfg(any(target_os = "linux", target_os = "android"))]
use anyhow::Context;
#[cfg(any(target_os = "linux", target_os = "android"))]
use retry::delay::NoDelay;
#[cfg(any(target_os = "linux", target_os = "android"))]
use sys_mount::{unmount, FilesystemType, Mount, MountFlags, Unmount, UnmountFlags};

use crate::defs::KSU_OVERLAY_SOURCE;
use log::{info, warn};
#[cfg(any(target_os = "linux", target_os = "android"))]
use procfs::process::Process;
use std::path::Path;

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
            .create_loop(true)
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
            .create_loop(true)
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
fn mount_overlayfs(
    lower_dirs: &[String],
    lowest: impl AsRef<Path>,
    dest: impl AsRef<Path>,
) -> Result<()> {
    let options = format!(
        "lowerdir={}:{}",
        lower_dirs.join(":"),
        lowest.as_ref().display()
    );
    info!(
        "mount overlayfs on {}, options={}",
        dest.as_ref().display(),
        options
    );
    Mount::builder()
        .fstype(FilesystemType::from("overlay"))
        .data(&options)
        .flags(MountFlags::RDONLY)
        .mount(KSU_OVERLAY_SOURCE, dest.as_ref())
        .with_context(|| {
            format!(
                "mount overlayfs on {} options {} failed",
                dest.as_ref().display(),
                options
            )
        })?;
    Ok(())
}

#[cfg(any(target_os = "linux", target_os = "android"))]
fn bind_mount(from: impl AsRef<Path>, to: impl AsRef<Path>) -> Result<()> {
    info!(
        "bind mount {} -> {}",
        from.as_ref().display(),
        to.as_ref().display()
    );
    Mount::builder()
        .flags(MountFlags::BIND)
        .mount(from.as_ref(), to.as_ref())
        .with_context(|| {
            format!(
                "bind mount failed: {} -> {}",
                from.as_ref().display(),
                to.as_ref().display()
            )
        })?;
    Ok(())
}

#[cfg(any(target_os = "linux", target_os = "android"))]
fn mount_overlay_child(
    mount_point: &str,
    relative: &String,
    module_roots: &Vec<String>,
    stock_root: &String,
) -> Result<()> {
    if !module_roots
        .iter()
        .any(|lower| Path::new(&format!("{lower}{relative}")).exists())
    {
        return bind_mount(stock_root, mount_point);
    }
    if !Path::new(&stock_root).is_dir() {
        return Ok(());
    }
    let mut lower_dirs: Vec<String> = vec![];
    for lower in module_roots {
        let lower_dir = format!("{lower}{relative}");
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
    if let Err(e) = mount_overlayfs(&lower_dirs, stock_root, mount_point) {
        warn!("failed: {:#}, fallback to bind mount", e);
        bind_mount(stock_root, mount_point)?;
    }
    Ok(())
}

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn mount_overlay(root: &String, module_roots: &Vec<String>) -> Result<()> {
    info!("mount overlay for {}", root);
    std::env::set_current_dir(root).with_context(|| format!("failed to chdir to {root}"))?;
    let stock_root = ".";

    // collect child mounts before mounting the root
    let mounts = Process::myself()?
        .mountinfo()
        .with_context(|| "get mountinfo")?;
    let mut mount_seq = mounts
        .iter()
        .filter(|m| {
            m.mount_point.starts_with(root) && !Path::new(&root).starts_with(&m.mount_point)
        })
        .map(|m| m.mount_point.to_str())
        .collect::<Vec<_>>();
    mount_seq.sort();
    mount_seq.dedup();

    mount_overlayfs(module_roots, root, root).with_context(|| "mount overlayfs for root failed")?;
    for mount_point in mount_seq.iter() {
        let Some(mount_point) = mount_point else {
            continue;
        };
        let relative = mount_point.replacen(root, "", 1);
        let stock_root: String = format!("{stock_root}{relative}");
        if !Path::new(&stock_root).exists() {
            continue;
        }
        if let Err(e) = mount_overlay_child(mount_point, &relative, module_roots, &stock_root) {
            warn!(
                "failed to mount overlay for child {}: {:#}, revert",
                mount_point, e
            );
            umount_dir(root).with_context(|| format!("failed to revert {root}"))?;
            bail!(e);
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
pub fn mount_overlay(_dest: &String, _lower_dirs: &Vec<String>) -> Result<()> {
    unimplemented!()
}
