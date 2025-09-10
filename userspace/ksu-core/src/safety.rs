use std::fs;
use std::io::Result;
use std::path::Path;

/// Determine if system is in safe mode.
/// Priority: kernel-reported safemode first, then fall back to Android properties.
pub fn is_safe_mode() -> bool {
    // ask kernel first
    let kernel_safe = crate::ksucalls::check_kernel_safemode();
    if kernel_safe {
        log::info!("kernel safemode: true");
        return true;
    }
    // fallback to properties
    let safemode = crate::props::getprop("persist.sys.safemode")
        .as_deref()
        .map(|v| v == "1")
        .unwrap_or(false)
        || crate::props::getprop("ro.sys.safemode")
            .as_deref()
            .map(|v| v == "1")
            .unwrap_or(false);
    log::info!("safemode: {}", safemode);
    safemode
}

pub const METAMODULE_SAFETY_FLAG: &str = "/data/adb/ksu/.metamodule_booting";

pub fn create() -> Result<()> {
    let _ = fs::File::create(METAMODULE_SAFETY_FLAG)?;
    Ok(())
}

pub fn clear() -> Result<()> {
    if Path::new(METAMODULE_SAFETY_FLAG).exists() {
        let _ = fs::remove_file(METAMODULE_SAFETY_FLAG)?;
    }
    Ok(())
}

pub fn exists() -> bool {
    Path::new(METAMODULE_SAFETY_FLAG).exists()
}
