use anyhow::{Context, Ok, Result};
#[cfg(target_os = "android")]
use extattr::{setxattr, Flags as XattrFlags};
use jwalk::{Parallelism::Serial, WalkDir};

const SYSTEM_CON: &str = "u:object_r:system_file:s0";
const _ADB_CON: &str = "u:object_r:adb_data_file:s0";

pub fn setcon(path: &str, con: &str) -> Result<()> {
    #[cfg(target_os = "android")]
    setxattr(path, "security.selinux", con, XattrFlags::empty())
        .with_context(|| format!("Failed to change SELinux context for {path}"))?;
    Ok(())
}

pub fn setsyscon(path: &str) -> Result<()> {
    setcon(path, SYSTEM_CON)
}

pub fn restore_syscon(dir: &str) -> Result<()> {
    for dir_entry in WalkDir::new(dir).parallelism(Serial) {
        if let Some(path) = dir_entry.ok().map(|dir_entry| dir_entry.path()) {
            #[cfg(target_os = "android")]
            setxattr(&path, "security.selinux", SYSTEM_CON, XattrFlags::empty()).with_context(
                || {
                    format!(
                        "Failed to change SELinux context for {}",
                        path.to_str().unwrap()
                    )
                },
            )?;
        }
    }
    Ok(())
}
