use anyhow::Result;

#[cfg(target_os = "android")]
use anyhow::{ensure, Context, Ok};
#[cfg(target_os = "android")]
use retry::delay::NoDelay;
#[cfg(target_os = "android")]
use sys_mount::{unmount, FilesystemType, Mount, MountFlags, UnmountFlags};

#[cfg(target_os = "android")]
fn do_mount_image(src: &str, target: &str) -> Result<()> {
    Mount::builder()
        .fstype(FilesystemType::from("ext4"))
        .mount(src, target)
        .with_context(|| format!("Failed to do mount: {src} -> {target}"))?;
    Ok(())
}

#[cfg(target_os = "android")]
pub fn mount_ext4(src: &str, target: &str) -> Result<()> {
    // umount target first.
    let _ = umount_dir(target);
    let result = retry::retry(NoDelay.take(3), || do_mount_image(src, target));
    ensure!(result.is_ok(), "Failed to mount {} -> {}", src, target);
    Ok(())
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
pub fn mount_ext4(_src: &str, _target: &str) -> Result<()> {
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
