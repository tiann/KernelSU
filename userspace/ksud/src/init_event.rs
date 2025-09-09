use anyhow::{Context, Result};
use log::{info, warn};
use std::path::Path;

use crate::{ assets, defs, ksucalls, modsys, restorecon, utils };

// 通过 modsys 执行挂载逻辑，ksud 无需再保留重复实现

pub fn on_post_data_fs() -> Result<()> {
    ksucalls::report_post_fs_data();

    utils::umask(0);

    #[cfg(unix)]
    let _ = catch_bootlog("logcat", vec!["logcat"]);
    #[cfg(unix)]
    let _ = catch_bootlog("dmesg", vec!["dmesg", "-w"]);

    if utils::has_magisk() {
        warn!("Magisk detected, skip post-fs-data!");
        return Ok(());
    }

    let _safe_mode = crate::utils::is_safe_mode();

    // Extract assets before delegating to modsys
    assets::ensure_binaries(true).with_context(|| "Failed to extract bin assets")?;

    // Initialize modsys
    modsys::init()?;
    
    // Create metamodule safety flag (core API)
    if let Err(e) = ksu_core::safety::create() {
        warn!("Failed to create metamodule safety flag: {e}");
    } else {
        info!("Created metamodule safety flag");
    }

    // Delegate to modsys for module-related operations
    if let Err(e) = modsys::stage("post-fs-data") {
        warn!("modsys post-fs-data stage failed: {e}");
    }

    // ksud retains: props, sepolicy, restorecon
    if let Err(e) = restorecon::restorecon() {
        warn!("restorecon failed: {e}");
    }

    if let Err(e) = crate::profile::apply_sepolies() {
        warn!("apply root profile sepolicy failed: {e}");
    }

    std::env::set_current_dir("/").with_context(|| "failed to chdir to /")?;

    Ok(())
}

// run_stage function removed - functionality delegated to modsys

pub fn on_services() -> Result<()> {
    info!("on_services triggered!");
    
    // Delegate to modsys
    if let Err(e) = modsys::stage("service") {
        warn!("modsys service stage failed: {e}");
    }

    Ok(())
}

pub fn on_boot_completed() -> Result<()> {
    ksucalls::report_boot_complete();
    info!("on_boot_completed triggered!");
    
    // Clear metamodule safety flag if system boots successfully
    if let Err(e) = ksu_core::safety::clear() {
        warn!("Failed to clear metamodule safety flag: {e}");
    } else {
        info!("Cleared metamodule safety flag - boot successful");
    }
    
    // Delegate to modsys for boot-completed stage
    if let Err(e) = modsys::stage("boot-completed") {
        warn!("modsys boot-completed stage failed: {e}");
    }

    Ok(())
}

/// New metamodule safety mode implementation - moved to ksu-core::safety

#[cfg(unix)]
fn catch_bootlog(logname: &str, command: Vec<&str>) -> Result<()> {
    use std::os::unix::process::CommandExt;
    use std::process::Stdio;

    let logdir = Path::new(defs::LOG_DIR);
    utils::ensure_dir_exists(logdir)?;
    let bootlog = logdir.join(format!("{logname}.log"));
    let oldbootlog = logdir.join(format!("{logname}.old.log"));

    if bootlog.exists() {
        std::fs::rename(&bootlog, oldbootlog)?;
    }

    let bootlog = std::fs::File::create(bootlog)?;

    let mut args = vec!["-s", "9", "30s"];
    args.extend_from_slice(&command);
    // timeout -s 9 30s logcat > boot.log
    let result = unsafe {
        std::process::Command::new("timeout")
            .process_group(0)
            .pre_exec(|| {
                utils::switch_cgroups();
                Ok(())
            })
            .args(args)
            .stdout(Stdio::from(bootlog))
            .spawn()
    };

    if let Err(e) = result {
        warn!("Failed to start logcat: {e:#}");
    }

    Ok(())
}
