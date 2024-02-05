use anyhow::{anyhow, bail, Ok, Result};

#[cfg(any(target_os = "linux", target_os = "android"))]
use anyhow::Context;
#[cfg(any(target_os = "linux", target_os = "android"))]
#[cfg(any(target_os = "linux", target_os = "android"))]
use rustix::{fd::AsFd, fs::CWD, mount::*};

use crate::defs::KSU_OVERLAY_SOURCE;
use log::{info, warn};
#[cfg(any(target_os = "linux", target_os = "android"))]
use procfs::process::Process;
use std::path::Path;
use std::path::PathBuf;

pub struct AutoMountExt4 {
    target: String,
    auto_umount: bool,
}

impl AutoMountExt4 {
    #[cfg(any(target_os = "linux", target_os = "android"))]
    pub fn try_new(source: &str, target: &str, auto_umount: bool) -> Result<Self> {
        mount_ext4(source, target)?;
        Ok(Self {
            target: target.to_string(),
            auto_umount,
        })
    }

    #[cfg(not(any(target_os = "linux", target_os = "android")))]
    pub fn try_new(_src: &str, _mnt: &str, _auto_umount: bool) -> Result<Self> {
        unimplemented!()
    }

    #[cfg(any(target_os = "linux", target_os = "android"))]
    pub fn umount(&self) -> Result<()> {
        unmount(self.target.as_str(), UnmountFlags::DETACH)?;
        Ok(())
    }
}

#[cfg(any(target_os = "linux", target_os = "android"))]
impl Drop for AutoMountExt4 {
    fn drop(&mut self) {
        log::info!(
            "AutoMountExt4 drop: {}, auto_umount: {}",
            self.target,
            self.auto_umount
        );
        if self.auto_umount {
            let _ = self.umount();
        }
    }
}

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn mount_ext4(source: impl AsRef<Path>, target: impl AsRef<Path>) -> Result<()> {
    let new_loopback = loopdev::LoopControl::open()?.next_free()?;
    new_loopback.with().attach(source)?;
    let lo = new_loopback.path().ok_or(anyhow!("no loop"))?;
    let fs = fsopen("ext4", FsOpenFlags::FSOPEN_CLOEXEC)?;
    let fs = fs.as_fd();
    fsconfig_set_string(fs, "source", lo)?;
    fsconfig_create(fs)?;
    let mount = fsmount(fs, FsMountFlags::FSMOUNT_CLOEXEC, MountAttrFlags::empty())?;
    move_mount(
        mount.as_fd(),
        "",
        CWD,
        target.as_ref(),
        MoveMountFlags::MOVE_MOUNT_F_EMPTY_PATH,
    )?;
    Ok(())
}

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn umount_dir(src: impl AsRef<Path>) -> Result<()> {
    unmount(src.as_ref(), UnmountFlags::empty())
        .with_context(|| format!("Failed to umount {}", src.as_ref().display()))?;
    Ok(())
}

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn mount_overlayfs(
    lower_dirs: &[String],
    lowest: &str,
    upperdir: Option<PathBuf>,
    workdir: Option<PathBuf>,
    dest: impl AsRef<Path>,
) -> Result<()> {
    let lowerdir_config = lower_dirs
        .iter()
        .map(|s| s.as_ref())
        .chain(std::iter::once(lowest))
        .collect::<Vec<_>>()
        .join(":");
    info!(
        "mount overlayfs on {:?}, lowerdir={}, upperdir={:?}, workdir={:?}",
        dest.as_ref(),
        lowerdir_config,
        upperdir,
        workdir
    );
    let fs = fsopen("overlay", FsOpenFlags::FSOPEN_CLOEXEC)?;
    let fs = fs.as_fd();
    fsconfig_set_string(fs, "lowerdir", lowerdir_config)?;
    if let (Some(upperdir), Some(workdir)) = (upperdir, workdir) {
        if upperdir.exists() && workdir.exists() {
            fsconfig_set_string(fs, "upperdir", upperdir.display().to_string())?;
            fsconfig_set_string(fs, "workdir", workdir.display().to_string())?;
        }
    }
    fsconfig_set_string(fs, "source", KSU_OVERLAY_SOURCE)?;
    fsconfig_create(fs)?;
    let mount = fsmount(fs, FsMountFlags::FSMOUNT_CLOEXEC, MountAttrFlags::empty())?;
    move_mount(
        mount.as_fd(),
        "",
        CWD,
        dest.as_ref(),
        MoveMountFlags::MOVE_MOUNT_F_EMPTY_PATH,
    )?;
    Ok(())
}

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn mount_tmpfs(dest: impl AsRef<Path>) -> Result<()> {
    info!("mount tmpfs on {}", dest.as_ref().display());
    let fs = fsopen("tmpfs", FsOpenFlags::FSOPEN_CLOEXEC)?;
    let fs = fs.as_fd();
    fsconfig_set_string(fs, "source", KSU_OVERLAY_SOURCE)?;
    fsconfig_create(fs)?;
    let mount = fsmount(fs, FsMountFlags::FSMOUNT_CLOEXEC, MountAttrFlags::empty())?;
    move_mount(
        mount.as_fd(),
        "",
        CWD,
        dest.as_ref(),
        MoveMountFlags::MOVE_MOUNT_F_EMPTY_PATH,
    )?;
    Ok(())
}

#[cfg(any(target_os = "linux", target_os = "android"))]
fn bind_mount(from: impl AsRef<Path>, to: impl AsRef<Path>) -> Result<()> {
    info!(
        "bind mount {} -> {}",
        from.as_ref().display(),
        to.as_ref().display()
    );
    let tree = open_tree(
        CWD,
        from.as_ref(),
        OpenTreeFlags::OPEN_TREE_CLOEXEC
            | OpenTreeFlags::OPEN_TREE_CLONE
            | OpenTreeFlags::AT_RECURSIVE,
    )?;
    move_mount(
        tree.as_fd(),
        "",
        CWD,
        to.as_ref(),
        MoveMountFlags::MOVE_MOUNT_F_EMPTY_PATH,
    )?;
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
    if let Err(e) = mount_overlayfs(&lower_dirs, stock_root, None, None, mount_point) {
        warn!("failed: {:#}, fallback to bind mount", e);
        bind_mount(stock_root, mount_point)?;
    }
    Ok(())
}

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn mount_overlay(
    root: &String,
    module_roots: &Vec<String>,
    workdir: Option<PathBuf>,
    upperdir: Option<PathBuf>,
) -> Result<()> {
    info!("mount overlay for {}", root);
    std::env::set_current_dir(root).with_context(|| format!("failed to chdir to {root}"))?;
    let stock_root = ".";

    // collect child mounts before mounting the root
    let mounts = Process::myself()?
        .mountinfo()
        .with_context(|| "get mountinfo")?;
    let mut mount_seq = mounts
        .0
        .iter()
        .filter(|m| {
            m.mount_point.starts_with(root) && !Path::new(&root).starts_with(&m.mount_point)
        })
        .map(|m| m.mount_point.to_str())
        .collect::<Vec<_>>();
    mount_seq.sort();
    mount_seq.dedup();

    mount_overlayfs(module_roots, root, upperdir, workdir, root)
        .with_context(|| "mount overlayfs for root failed")?;
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
pub fn mount_overlay(
    _root: &String,
    _module_roots: &Vec<String>,
    _workdir: Option<PathBuf>,
    _upperdir: Option<PathBuf>,
) -> Result<()> {
    unimplemented!()
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
pub fn mount_tmpfs(_dest: impl AsRef<Path>) -> Result<()> {
    unimplemented!()
}
