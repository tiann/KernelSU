use anyhow::{ensure, Context, Ok, Result};
use std::{path::Path, process::Command};

use crate::apk_sign::get_apk_signature;

const KERNEL_PARAM_PATH: &str = "/sys/module/kernelsu";

fn set_kernel_param(hash: String) -> Result<()> {
    let kernel_param_path = Path::new(KERNEL_PARAM_PATH).join("parameters");
    let expeced_hash_path = kernel_param_path.join("ksu_expected_hash");

    println!(
        "before hash: {}",
        std::fs::read_to_string(&expeced_hash_path)?
    );

    std::fs::write(&expeced_hash_path, hash)?;

    println!(
        "after hash: {}",
        std::fs::read_to_string(&expeced_hash_path)?
    );

    Ok(())
}

fn get_apk_path(package_name: &str) -> Result<String> {
    // `cmd package path` is not available below Android 9
    let output = Command::new("pm").args(["path", package_name]).output()?;

    // package:/data/app/<xxxx>/base.apk
    let output = String::from_utf8_lossy(&output.stdout);
    let path = output.trim().replace("package:", "");
    Ok(path)
}

pub fn set_manager(pkg: &str) -> Result<()> {
    ensure!(
        Path::new(KERNEL_PARAM_PATH).exists(),
        "CONFIG_KSU_DEBUG is not enabled"
    );

    let path = get_apk_path(pkg).with_context(|| format!("{pkg} does not exist!"))?;
    let sign = get_apk_signature(&path)?;
    set_kernel_param(sign)?;
    Ok(())
}
