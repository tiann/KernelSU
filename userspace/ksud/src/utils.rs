use std::{
    path::Path,
    process::{Command, Stdio},
};

use anyhow::{ensure, Result};
use retry::delay::NoDelay;
use subprocess::Exec;

fn do_mount_image(src: &str, target: &str) -> Result<()> {
    let result = Exec::shell(format!("mount -t ext4 {} {}", src, target))
        .stdout(subprocess::NullFile)
        .stderr(subprocess::Redirection::Merge)
        .join()?;
    ensure!(result.success(), "do mount: {} -> {} failed.", src, target);
    Ok(())
}

pub fn mount_image(src: &str, target: &str) -> Result<()> {
    // umount target first.
    let _ = umount_dir(target);
    let result = retry::retry(NoDelay.take(3), || do_mount_image(src, target));
    ensure!(result.is_ok(), "mount: {} -> {} failed.", src, target);
    Ok(())
}

pub fn umount_dir(src: &str) -> Result<()> {
    let result = Exec::shell(format!("umount {}", src))
        .stdout(subprocess::NullFile)
        .stderr(subprocess::Redirection::Merge)
        .join()?;
    ensure!(result.success(), "umount: {} failed", src);
    Ok(())
}

pub fn ensure_clean_dir(dir: &str) -> Result<()> {
    let path = Path::new(dir);
    if path.exists() {
        std::fs::remove_dir_all(path)?;
    }
    Ok(std::fs::create_dir_all(path)?)
}

pub fn getprop(prop: &str) -> Result<String> {
    let output = Command::new("getprop")
        .arg(prop)
        .stdout(Stdio::piped())
        .output()?;
    let output = String::from_utf8_lossy(&output.stdout);
    Ok(output.trim().to_string())
}

pub fn is_safe_mode() -> Result<bool> {
    Ok(getprop("persist.sys.safemode")?.eq("1") || getprop("ro.sys.safemode")?.eq("1"))
}

pub fn get_zip_uncompressed_size(zip_path: &str) -> Result<u64> {
    let mut zip = zip::ZipArchive::new(std::fs::File::open(zip_path)?)?;
    let total: u64 = (0..zip.len())
        .map(|i| zip.by_index(i).unwrap().size())
        .sum();
    Ok(total)
}
