use std::{
    path::Path,
    process::{Command, Stdio},
};

use anyhow::{ensure, Result};
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
