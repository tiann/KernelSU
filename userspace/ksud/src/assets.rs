use anyhow::Result;
use const_format::concatcp;
use crate::{utils, defs};

pub const RESETPROP_PATH: &str = concatcp!(defs::BINARY_DIR, "resetprop");

#[cfg(target_arch = "aarch64")]
const RESETPROP: &[u8] = include_bytes!("../bin/aarch64/resetprop");
#[cfg(target_arch = "x86_64")]
const RESETPROP: &[u8] = include_bytes!("../bin/x86_64/resetprop");

pub const BUSYBOX_PATH: &str = concatcp!(defs::BINARY_DIR, "busybox");
#[cfg(target_arch = "aarch64")]
const BUSYBOX: &[u8] = include_bytes!("../bin/aarch64/busybox");
#[cfg(target_arch = "x86_64")]
const BUSYBOX: &[u8] = include_bytes!("../bin/x86_64/busybox");

pub fn ensure_bin_assets() -> Result<()> {
    utils::ensure_binary(RESETPROP_PATH, RESETPROP)?;
    utils::ensure_binary(BUSYBOX_PATH, BUSYBOX)?;
    Ok(())
}