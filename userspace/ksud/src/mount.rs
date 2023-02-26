use anyhow::{Ok, Result};

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

    pub fn umount_all(&self) {
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

    pub fn umount_all(&self) {
        log::info!("stock overlay: umount all: {:?}", self.mountinfos);
        for mnt in &self.mountinfos {
            let Some(p) = mnt.mount_point.to_str() else {
                log::warn!("Failed to umount: {}", mnt.mount_point.display());
                continue;
            };

            let result = umount_dir(p);
            log::info!("stock umount {}: {:?}", p, result);
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
    mountlist: proc_mounts::MountList,
}

#[cfg(any(target_os = "linux", target_os = "android"))]
impl StockMount {
    pub fn new(mnt: &str) -> Result<Self> {
        let mountlist = proc_mounts::MountList::new()?;
        Ok(Self {
            mnt: mnt.to_string(),
            mountlist,
        })
    }

    fn get_target_mounts(&self) -> Vec<&proc_mounts::MountInfo> {
        let mut mounts = self
            .mountlist
            .destination_starts_with(std::path::Path::new(&self.mnt))
            .filter(|m| m.fstype != "overlay" && m.fstype != "rootfs")
            .collect::<Vec<_>>();
        mounts.sort_by(|a, b| b.dest.cmp(&a.dest)); // inverse order
        mounts
    }

    pub fn umount(&self) -> Result<()> {
        let mounts = self.get_target_mounts();
        log::info!("umount stock for {} : {:?}", self.mnt, mounts);
        for m in mounts {
            let dst = m
                .dest
                .to_str()
                .ok_or(anyhow::anyhow!("Failed to get dst"))?;
            umount_dir(dst)?;
            log::info!("umount: {:?}", m);
        }
        log::info!("umount stock succeed!");
        Ok(())
    }

    pub fn remount(&self) -> Result<()> {
        let mut mounts = self.get_target_mounts();
        mounts.reverse(); // remount it in order
        log::info!("remount stock for {} : {:?}", self.mnt, mounts);
        for m in mounts {
            let src = std::fs::canonicalize(&m.source)?;

            let src = src.to_str().ok_or(anyhow::anyhow!("Failed to get src"))?;
            let dst = m
                .dest
                .to_str()
                .ok_or(anyhow::anyhow!("Failed to get dst"))?;

            let fstype = m.fstype.as_str();
            let options = m.options.join(",");

            log::info!("begin remount: {src} -> {dst}");
            let result = std::process::Command::new("mount")
                .arg("-t")
                .arg(fstype)
                .arg("-o")
                .arg(options)
                .arg(src)
                .arg(dst)
                .status();
            if let Err(e) = result {
                log::error!("remount failed: {}", e);
            } else {
                log::info!("remount {src} -> {dst} succeed!");
            }
        }
        Ok(())
    }
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
impl StockMount {
    pub fn new(mnt: &str) -> Result<Self> {
        Ok(Self {
            mnt: mnt.to_string(),
        })
    }

    pub fn umount(&self) -> Result<()> {
        unimplemented!()
    }

    pub fn remount(&self) -> Result<()> {
        unimplemented!()
    }
}
