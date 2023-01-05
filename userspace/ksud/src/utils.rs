use std::path::Path;

use anyhow::{Result, ensure};
use subprocess::Exec;

pub fn mount_image(src: &str, target: &str) -> Result<()> {
    let result = Exec::shell(format!("mount -t ext4 {} {}", src, target)).join()?;
    ensure!(result.success(), "mount: {} -> {} failed.", src, target);
    Ok(())
}

pub fn umount_dir(src: &str) -> Result<()> {
    let result = Exec::shell(format!("umount {}", src)).join()?;
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