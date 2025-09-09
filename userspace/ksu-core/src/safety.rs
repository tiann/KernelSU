use std::fs;
use std::io::Result;
use std::path::Path;

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

