use anyhow::{bail, ensure, Ok, Result};

#[cfg(any(target_os = "linux", target_os = "android"))]
use anyhow::Context;
#[cfg(any(target_os = "linux", target_os = "android"))]
use retry::delay::NoDelay;
#[cfg(any(target_os = "linux", target_os = "android"))]
use sys_mount::{unmount, FilesystemType, Mount, MountFlags, Unmount, UnmountFlags};

#[cfg(any(target_os = "linux", target_os = "android"))]
use std::fs;
use std::fs::File;
use std::os::fd::AsRawFd;
use std::os::unix::fs::OpenOptionsExt;
use std::path::PathBuf;
use log::{info, warn};
use crate::defs::KSU_OVERLAY_SOURCE;
#[cfg(any(target_os = "linux", target_os = "android"))]
use crate::mount_tree::{MountNode, MountTree};

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
pub fn mount_overlay(dest: &PathBuf, lower_dirs: &Vec<String>, root_mounted: &mut bool) -> Result<()> {
    let mut mount_seq: Vec<MountTree> = vec![];
    let dest_str = dest.to_str().unwrap();
    info!("mount overlay for {}", dest_str);
    let tree = MountNode::get_tree()?;
    let tree = MountNode::get_mount_for_path(&tree, &dest).unwrap();
    MountNode::get_top_mounts_under_path(&mut mount_seq, &tree, &dest);
    let old_root = File::options()
        .read(true)
        .custom_flags(libc::O_PATH)
        .open(&dest)?;
    ensure!(old_root.metadata()?.is_dir());
    let old_root = format!("/proc/self/fd/{}", old_root.as_raw_fd());
    let mut first = true;
    for mount in mount_seq.iter().rev() {
        let mut lower_count: usize = 0;
        let src: String;
        let mount_point: String;
        let mut overlay_lower_dir = String::from("lowerdir=");
        let stock_is_dir: bool;
        let modified_is_dir: bool;
        if first {
            mount_point = String::from(dest_str);
            src = String::from(dest_str);
            stock_is_dir = true; // ensured
        } else {
            mount_point = String::from(mount.mount_info.mount_point.to_str().unwrap());
            let relative = mount_point.clone().replacen(dest_str, "", 1);
            src = format!("{}{}", old_root, relative);
            match fs::metadata(&src) {
                Result::Ok(stat) => {
                    stock_is_dir = stat.is_dir();
                }
                Err(e) => bail!("stat {}: {:#}", src, e)
            }
        }
        match fs::metadata(&mount_point) {
            Result::Ok(stat) => {
                modified_is_dir = stat.is_dir();
            }
            Err(e) => {
                match e.raw_os_error().unwrap() {
                    libc::ENOENT | libc::ENOTDIR => {
                        warn!("skip {} since it doesn't exists, maybe removed by module?", mount_point);
                        first = false;
                        continue;
                    }
                    _ => {
                        bail!("stat {}: {:#}", mount_point, e);
                    }
                }
            }
        }
        for lower in lower_dirs {
            let lower_dir = format!("{}{}", lower, mount_point);
            match fs::metadata(&lower_dir) {
                Result::Ok(stat) => {
                    if !stat.is_dir() {
                        if first {
                            warn!("{} is an invalid module", lower_dir);
                        }
                        continue;
                    }
                    lower_count += 1;
                    if lower_count > 1 {
                        overlay_lower_dir.push_str(":");
                    }
                    overlay_lower_dir.push_str(lower_dir.as_str());
                }
                Err(..) => {
                    continue;
                }
            }
        }
        if lower_count == 0 {
            if first {
                warn!("no valid modules, skip mount");
                break;
            }
            info!("mount bind mount_point={}, src={}", mount_point, src);
            Mount::builder()
                .flags(MountFlags::BIND)
                .mount(&src, &mount_point)
                .with_context(|| format!("bind mount failed: {} -> {}", src, mount_point))?;
        } else if stock_is_dir && modified_is_dir {
            overlay_lower_dir.push_str(":");
            overlay_lower_dir.push_str(&src);
            info!("mount overlayfs mount_point={}, src={}, options={}", mount_point, src, overlay_lower_dir);
            if let Err(e) = Mount::builder()
                .fstype(FilesystemType::from("overlay"))
                .data(&overlay_lower_dir)
                .mount(KSU_OVERLAY_SOURCE, &mount_point) {
                if first {
                    bail!(e);
                }
                warn!("mount overlayfs failed: {:#}, fallback to bind mount", e);
                Mount::builder()
                    .flags(MountFlags::BIND)
                    .mount(&src, &mount_point)
                    .with_context(|| format!("fallback bind mount failed: {} -> {}", src, mount_point))?;
            }
            if first {
                *root_mounted = true;
            }
        }
        first = false;
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

