use anyhow::{bail, Ok, Result};

#[cfg(any(target_os = "linux", target_os = "android"))]
use anyhow::Context;
#[cfg(any(target_os = "linux", target_os = "android"))]
use retry::delay::NoDelay;
#[cfg(any(target_os = "linux", target_os = "android"))]
use sys_mount::{unmount, FilesystemType, Mount, MountFlags, Unmount, UnmountFlags};

#[cfg(any(target_os = "linux", target_os = "android"))]
use procfs::process::{MountInfo, Process};
#[cfg(any(target_os = "linux", target_os = "android"))]
use std::collections::HashSet;

use crate::utils;

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
pub fn mount_overlay(lowerdir: &str, mnt: &str) -> Result<()> {
    Mount::builder()
        .fstype(FilesystemType::from("overlay"))
        .flags(MountFlags::RDONLY)
        .data(&format!("lowerdir={lowerdir}"))
        .mount("overlay", mnt)
        .map(|_| ())
        .map_err(|e| anyhow::anyhow!("mount partition: {mnt} overlay failed: {e}"))
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
pub fn mount_overlay(_lowerdir: &str, _mnt: &str) -> Result<()> {
    unimplemented!()
}

pub struct StockOverlay {
    #[cfg(any(target_os = "linux", target_os = "android"))]
    mountinfos: Vec<MountInfo>,
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
impl StockOverlay {
    pub fn new() -> Self {
        unimplemented!()
    }

    pub fn mount_all(&self) {
        unimplemented!()
    }
}

#[cfg(any(target_os = "linux", target_os = "android"))]
impl StockOverlay {
    pub fn new() -> Self {
        if let std::result::Result::Ok(process) = Process::myself() {
            if let std::result::Result::Ok(mountinfos) = process.mountinfo() {
                let overlay_mounts = mountinfos
                    .into_iter()
                    .filter(|m| m.fs_type == "overlay")
                    .collect::<Vec<_>>();
                return Self {
                    mountinfos: overlay_mounts,
                };
            }
        }
        Self { mountinfos: vec![] }
    }

    pub fn mount_all(&self) {
        log::info!("stock overlay: mount all: {:?}", self.mountinfos);
        for mount in self.mountinfos.clone() {
            let Some(mnt) = mount.mount_point.to_str() else {
                log::warn!("Failed to get mount point");
                continue;
            };

            if mnt == "/system" {
                log::warn!("stock overlay found /system, skip!");
                continue;
            }

            let (_flags, b): (HashSet<_>, HashSet<_>) = mount
                .mount_options
                .into_iter()
                .chain(mount.super_options)
                .partition(|(_, m)| m.is_none());

            let mut overlay_opts = vec![];
            for (opt, val) in b {
                if let Some(val) = val {
                    overlay_opts.push(format!("{opt}={val}"));
                } else {
                    log::warn!("opt empty: {}", opt);
                }
            }
            let overlay_data = overlay_opts.join(",");
            let result = Mount::builder()
                .fstype(FilesystemType::from("overlay"))
                .flags(MountFlags::RDONLY)
                .data(&overlay_data)
                .mount("overlay", mnt);
            if let Err(e) = result {
                log::error!(
                    "stock mount overlay: {} failed: {}",
                    mount.mount_point.display(),
                    e
                );
            } else {
                log::info!(
                    "stock mount :{} overlay_opts: {}",
                    mount.mount_point.display(),
                    overlay_opts.join(",")
                );
            }
        }
    }
}

// some ROMs mount device(ext4,exfat) to /vendor, when we do overlay mount, it will overlay
// the stock mounts, these mounts include bt_firmware, wifi_firmware, etc.
// so we to remount these mounts when we do overlay mount.
// this is a workaround, we should find a better way to do this.
#[derive(Debug)]
pub struct StockMount {
    mnt: String,
    #[cfg(any(target_os = "linux", target_os = "android"))]
    mountlist: Vec<(proc_mounts::MountInfo, std::path::PathBuf)>,
    #[cfg(any(target_os = "linux", target_os = "android"))]
    rootmount: sys_mount::Mount,
}

#[cfg(any(target_os = "linux", target_os = "android"))]
impl StockMount {
    pub fn new(mnt: &str) -> Result<Self> {
        let mountlist = proc_mounts::MountList::new()?;
        let mut mounts = mountlist
            .destination_starts_with(std::path::Path::new(mnt))
            .filter(|m| m.fstype != "overlay" && m.fstype != "rootfs")
            .collect::<Vec<_>>();
        mounts.sort_by(|a, b| b.dest.cmp(&a.dest)); // inverse order

        let mntroot = std::path::Path::new(crate::defs::STOCK_MNT_ROOT);
        utils::ensure_dir_exists(mntroot)?;
        log::info!("stock mount root: {}", mntroot.display());

        let rootdir = mntroot.join(
            mnt.strip_prefix('/')
                .ok_or(anyhow::anyhow!("invalid mnt: {}!", mnt))?,
        );
        utils::ensure_dir_exists(&rootdir)?;
        let rootmount = Mount::builder().fstype("tmpfs").mount("tmpfs", &rootdir)?;

        let mut ms = vec![];
        for m in mounts {
            let dest = &m.dest;
            if dest == std::path::Path::new(mnt) {
                continue;
            }

            let path = rootdir.join(dest.strip_prefix("/")?);
            log::info!("rootdir: {}, path: {}", rootdir.display(), path.display());
            if dest.is_dir() {
                utils::ensure_dir_exists(&path)
                    .with_context(|| format!("Failed to create dir: {}", path.display(),))?;
            } else if dest.is_file() {
                if !path.exists() {
                    let parent = path
                        .parent()
                        .with_context(|| format!("Failed to get parent: {}", path.display()))?;
                    utils::ensure_dir_exists(parent).with_context(|| {
                        format!("Failed to create parent: {}", parent.display())
                    })?;
                    std::fs::File::create(&path)
                        .with_context(|| format!("Failed to create file: {}", path.display(),))?;
                }
            } else {
                bail!("unknown file type: {:?}", dest)
            }
            log::info!("bind stock mount: {} -> {}", dest.display(), path.display());
            Mount::builder()
                .flags(MountFlags::BIND)
                .mount(dest, &path)
                .with_context(|| {
                    format!("Failed to mount: {} -> {}", dest.display(), path.display())
                })?;

            ms.push((m.clone(), path));
        }

        Ok(Self {
            mnt: mnt.to_string(),
            mountlist: ms,
            rootmount,
        })
    }

    // Yes, we move self here!
    pub fn remount(self) -> Result<()> {
        log::info!("remount stock for {} : {:?}", self.mnt, self.mountlist);
        let mut result = Ok(());
        for (m, src) in self.mountlist {
            let dst = m.dest;

            log::info!("begin remount: {} -> {}", src.display(), dst.display());
            let mount_result = Mount::builder()
                .flags(MountFlags::BIND | MountFlags::MOVE)
                .mount(&src, &dst);
            if let Err(e) = mount_result {
                log::error!("remount failed: {}", e);
                result = Err(e.into());
            } else {
                log::info!(
                    "remount {}({}) -> {} succeed!",
                    m.source.display(),
                    src.display(),
                    dst.display()
                );
            }
        }

        // umount the root tmpfs mount
        if let Err(e) = self.rootmount.unmount(UnmountFlags::DETACH) {
            log::warn!("umount root mount failed: {}", e);
        }

        result
    }
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
impl StockMount {
    pub fn new(mnt: &str) -> Result<Self> {
        Ok(Self {
            mnt: mnt.to_string(),
        })
    }

    pub fn remount(&self) -> Result<()> {
        unimplemented!()
    }
}
