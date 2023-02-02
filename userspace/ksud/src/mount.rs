use anyhow::Result;

#[cfg(target_os = "android")]
use anyhow::{Context, Ok};
#[cfg(target_os = "android")]
use retry::delay::NoDelay;
#[cfg(target_os = "android")]
use sys_mount::{unmount, FilesystemType, Mount, MountFlags, Unmount, UnmountFlags};

#[allow(dead_code)]
pub struct AutoMountExt4 {
    mnt: String,
    #[cfg(target_os = "android")]
    mount: Option<Mount>,
    auto_umount: bool,
}

#[allow(dead_code)]
impl AutoMountExt4 {
    #[cfg(target_os = "android")]
    pub fn try_new(src: &str, mnt: &str, auto_umount: bool) -> Result<Self> {
        let result = Mount::builder()
            .fstype(FilesystemType::from("ext4"))
            .flags(MountFlags::empty())
            .mount(src, mnt)
            .map(|mount| {
                Ok(Self {
                    mnt: mnt.to_string(),
                    mount: Some(mount),
                    auto_umount
                })
            });
        if let Err(e) = result {
            println!("- Mount failed: {}, retry with system mount", e);
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
                    auto_umount
                })
            }
        } else {
            result.unwrap()
        }
    }

    #[cfg(not(target_os = "android"))]
    pub fn try_new(_src: &str, _mnt: &str, _auto_umount: bool) -> Result<Self> {
        unimplemented!()
    }

    #[cfg(target_os = "android")]
    pub fn umount(&self) -> Result<()> {
        match self.mount {
            Some(ref mount) => mount
                .unmount(UnmountFlags::empty())
                .map_err(|e| anyhow::anyhow!(e)),
            None => {
                let result = std::process::Command::new("umount").arg(&self.mnt).status();
                if let Err(e) = result {
                    Err(anyhow::anyhow!("umount: {} failed: {e}", self.mnt))
                } else {
                    Ok(())
                }
            }
        }
    }
}

#[cfg(target_os = "android")]
impl Drop for AutoMountExt4 {
    fn drop(&mut self) {
        log::info!("AutoMountExt4 drop: {}, auto_umount: {}", self.mnt, self.auto_umount);
        if self.auto_umount {
            let _ = self.umount();
        }
    }
}

#[cfg(target_os = "android")]
fn do_mount_image(src: &str, target: &str, autodrop: bool) -> Result<()> {
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

#[cfg(target_os = "android")]
pub fn mount_ext4(src: &str, target: &str, autodrop: bool) -> Result<()> {
    // umount target first.
    let _ = umount_dir(target);
    let result = retry::retry(NoDelay.take(3), || do_mount_image(src, target, autodrop));
    result
        .map_err(|e| anyhow::anyhow!("mount partition: {src} -> {target} failed: {e}"))
        .map(|_| ())
}

#[cfg(target_os = "android")]
pub fn umount_dir(src: &str) -> Result<()> {
    unmount(src, UnmountFlags::empty()).with_context(|| format!("Failed to umount {src}"))?;
    Ok(())
}

#[cfg(target_os = "android")]
pub fn mount_overlay(lowerdir: &str, mnt: &str) -> Result<()> {
    Mount::builder()
        .fstype(FilesystemType::from("overlay"))
        .flags(MountFlags::RDONLY)
        .data(&format!("lowerdir={lowerdir}"))
        .mount("overlay", mnt)
        .map(|_| ())
        .map_err(|e| anyhow::anyhow!("mount partition: {mnt} overlay failed: {e}"))
}

#[cfg(not(target_os = "android"))]
pub fn mount_ext4(_src: &str, _target: &str, _autodrop: bool) -> Result<()> {
    unimplemented!()
}

#[cfg(not(target_os = "android"))]
pub fn umount_dir(_src: &str) -> Result<()> {
    unimplemented!()
}

#[cfg(not(target_os = "android"))]
pub fn mount_overlay(_lowerdir: &str, _mnt: &str) -> Result<()> {
    unimplemented!()
}
