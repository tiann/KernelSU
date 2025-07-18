use anyhow::{Context, Ok, Result, ensure};
use std::{
    path::{Path, PathBuf},
    process::Command,
};

const KERNEL_PARAM_PATH: &str = "/sys/module/kernelsu";

fn read_u32(path: &PathBuf) -> Result<u32> {
    let content = std::fs::read_to_string(path)?;
    let content = content.trim();
    let content = content.parse::<u32>()?;
    Ok(content)
}

fn set_kernel_param(uid: u32) -> Result<()> {
    let kernel_param_path = Path::new(KERNEL_PARAM_PATH).join("parameters");

    let ksu_debug_manager_uid = kernel_param_path.join("ksu_debug_manager_uid");
    let before_uid = read_u32(&ksu_debug_manager_uid)?;
    std::fs::write(&ksu_debug_manager_uid, uid.to_string())?;
    let after_uid = read_u32(&ksu_debug_manager_uid)?;

    println!("set manager uid: {before_uid} -> {after_uid}");

    Ok(())
}

#[cfg(target_os = "android")]
fn get_pkg_uid(pkg: &str) -> Result<u32> {
    // stat /data/data/<pkg>
    let uid = rustix::fs::stat(format!("/data/data/{pkg}"))
        .with_context(|| format!("stat /data/data/{pkg}"))?
        .st_uid;
    Ok(uid)
}

pub fn set_manager(pkg: &str) -> Result<()> {
    ensure!(
        Path::new(KERNEL_PARAM_PATH).exists(),
        "CONFIG_KSU_DEBUG is not enabled"
    );

    #[cfg(target_os = "android")]
    let uid = get_pkg_uid(pkg)?;
    #[cfg(not(target_os = "android"))]
    let uid = 0;
    set_kernel_param(uid)?;
    // force-stop it
    let _ = Command::new("am").args(["force-stop", pkg]).status();
    Ok(())
}
