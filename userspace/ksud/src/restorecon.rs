use anyhow::Result;
use jwalk::{Parallelism::Serial, WalkDir};
use std::path::Path;

#[cfg(any(target_os = "linux", target_os = "android"))]
use anyhow::{Context, Ok};
#[cfg(any(target_os = "linux", target_os = "android"))]
use extattr::{setxattr, Flags as XattrFlags};

pub const SYSTEM_CON: &str = "u:object_r:system_file:s0";
pub const ADB_CON: &str = "u:object_r:adb_data_file:s0";
const SELINUX_XATTR: &str = "security.selinux";

pub fn setcon<P: AsRef<Path>>(path: P, con: &str) -> Result<()> {
    #[cfg(any(target_os = "linux", target_os = "android"))]
    setxattr(&path, SELINUX_XATTR, con, XattrFlags::empty()).with_context(|| {
        format!(
            "Failed to change SELinux context for {}",
            path.as_ref().display()
        )
    })?;
    Ok(())
}

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn setsyscon<P: AsRef<Path>>(path: P) -> Result<()> {
    setcon(path, SYSTEM_CON)
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
pub fn setsyscon<P: AsRef<Path>>(path: P) -> Result<()> {
    unimplemented!()
}

pub fn restore_syscon<P: AsRef<Path>>(dir: P) -> Result<()> {
    for dir_entry in WalkDir::new(dir).parallelism(Serial) {
        if let Some(path) = dir_entry.ok().map(|dir_entry| dir_entry.path()) {
            #[cfg(any(target_os = "linux", target_os = "android"))]
            setxattr(&path, SELINUX_XATTR, SYSTEM_CON, XattrFlags::empty()).with_context(|| {
                format!("Failed to change SELinux context for {}", path.display())
            })?;
        }
    }
    Ok(())
}
