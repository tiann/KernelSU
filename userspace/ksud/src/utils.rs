use std::path::Path;

use anyhow::{ensure, Context, Result};
use retry::delay::NoDelay;
use sys_mount::{unmount, FilesystemType, Mount, UnmountFlags};

fn do_mount_image(src: &str, target: &str) -> Result<()> {
    Mount::builder()
        .fstype(FilesystemType::from("ext4"))
        .mount(src, target)
        .with_context(|| format!("Failed to do mount: {src} -> {target}"))?;
    Ok(())
}

pub fn mount_image(src: &str, target: &str) -> Result<()> {
    // umount target first.
    let _ = umount_dir(target);
    let result = retry::retry(NoDelay.take(3), || do_mount_image(src, target));
    ensure!(result.is_ok(), "Failed to mount {} -> {}", src, target);
    Ok(())
}

pub fn umount_dir(src: &str) -> Result<()> {
    unmount(src, UnmountFlags::empty()).with_context(|| format!("Failed to umount {src}"))?;
    Ok(())
}

pub fn ensure_clean_dir(dir: &str) -> Result<()> {
    let path = Path::new(dir);
    if path.exists() {
        std::fs::remove_dir_all(path)?;
    }
    Ok(std::fs::create_dir_all(path)?)
}

pub fn getprop(prop: &str) -> Option<String> {
    android_properties::getprop(prop).value()
}

pub fn is_safe_mode() -> bool {
    getprop("persist.sys.safemode")
        .filter(|prop| prop == "1")
        .is_some()
        || getprop("ro.sys.safemode")
            .filter(|prop| prop == "1")
            .is_some()
}

pub fn get_zip_uncompressed_size(zip_path: &str) -> Result<u64> {
    let mut zip = zip::ZipArchive::new(std::fs::File::open(zip_path)?)?;
    let total: u64 = (0..zip.len())
        .map(|i| zip.by_index(i).unwrap().size())
        .sum();
    Ok(total)
}
