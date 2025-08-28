use anyhow::{Context, Result, bail};
use log::{info, warn};
use std::{collections::HashMap, path::Path};

use crate::{
    assets, defs, ksucalls, modsys, mount, restorecon,
    utils::{self, ensure_clean_dir},
};

fn mount_partition(partition_name: &str, lowerdir: &Vec<String>) -> Result<()> {
    if lowerdir.is_empty() {
        warn!("partition: {partition_name} lowerdir is empty");
        return Ok(());
    }

    let partition = format!("/{partition_name}");

    // if /partition is a symlink and linked to /system/partition, then we don't need to overlay it separately
    if Path::new(&partition).read_link().is_ok() {
        warn!("partition: {partition} is a symlink");
        return Ok(());
    }

    let mut workdir = None;
    let mut upperdir = None;
    let system_rw_dir = Path::new(defs::SYSTEM_RW_DIR);
    if system_rw_dir.exists() {
        workdir = Some(system_rw_dir.join(partition_name).join("workdir"));
        upperdir = Some(system_rw_dir.join(partition_name).join("upperdir"));
    }

    mount::mount_overlay(&partition, lowerdir, workdir, upperdir)
}

pub fn mount_modules_systemlessly(module_dir: &str) -> Result<()> {
    // construct overlay mount params
    let dir = std::fs::read_dir(module_dir);
    let Ok(dir) = dir else {
        bail!("open {} failed", defs::MODULE_DIR);
    };

    let mut system_lowerdir: Vec<String> = Vec::new();

    let partition = vec!["vendor", "product", "system_ext", "odm", "oem"];
    let mut partition_lowerdir: HashMap<String, Vec<String>> = HashMap::new();
    for ele in &partition {
        partition_lowerdir.insert((*ele).to_string(), Vec::new());
    }

    for entry in dir.flatten() {
        let module = entry.path();
        if !module.is_dir() {
            continue;
        }
        let disabled = module.join(defs::DISABLE_FILE_NAME).exists();
        if disabled {
            info!("module: {} is disabled, ignore!", module.display());
            continue;
        }
        let skip_mount = module.join(defs::SKIP_MOUNT_FILE_NAME).exists();
        if skip_mount {
            info!("module: {} skip_mount exist, skip!", module.display());
            continue;
        }

        let module_system = Path::new(&module).join("system");
        if module_system.is_dir() {
            system_lowerdir.push(format!("{}", module_system.display()));
        }

        for part in &partition {
            // if /partition is a mountpoint, we would move it to $MODPATH/$partition when install
            // otherwise it must be a symlink and we don't need to overlay!
            let part_path = Path::new(&module).join(part);
            if part_path.is_dir() {
                if let Some(v) = partition_lowerdir.get_mut(*part) {
                    v.push(format!("{}", part_path.display()));
                }
            }
        }
    }

    // mount /system first
    if let Err(e) = mount_partition("system", &system_lowerdir) {
        warn!("mount system failed: {e:#}");
    }

    // mount other partitions
    for (k, v) in partition_lowerdir {
        if let Err(e) = mount_partition(&k, &v) {
            warn!("mount {k} failed: {e:#}");
        }
    }

    Ok(())
}

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

    let safe_mode = crate::utils::is_safe_mode();

    // Extract assets before delegating to modsys
    assets::ensure_binaries(true).with_context(|| "Failed to extract bin assets")?;

    // Initialize modsys
    modsys::init()?;
    
    // Create metamodule safety flag
    create_metamodule_safety_flag()?;

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
    clear_metamodule_safety_flag()?;
    
    // Delegate to modsys for boot-completed stage
    if let Err(e) = modsys::stage("boot-completed") {
        warn!("modsys boot-completed stage failed: {e}");
    }

    Ok(())
}

/// New metamodule safety mode implementation
/// Create a flag during post-fs-data, clear it on boot-completed
/// If the flag still exists on next boot, disable module system
const METAMODULE_SAFETY_FLAG: &str = "/data/adb/ksu/.metamodule_booting";

fn create_metamodule_safety_flag() -> Result<()> {
    use std::fs::File;
    
    if let Err(e) = File::create(METAMODULE_SAFETY_FLAG) {
        warn!("Failed to create metamodule safety flag: {e}");
    } else {
        info!("Created metamodule safety flag");
    }
    Ok(())
}

fn clear_metamodule_safety_flag() -> Result<()> {
    if std::path::Path::new(METAMODULE_SAFETY_FLAG).exists() {
        if let Err(e) = std::fs::remove_file(METAMODULE_SAFETY_FLAG) {
            warn!("Failed to clear metamodule safety flag: {e}");
        } else {
            info!("Cleared metamodule safety flag - boot successful");
        }
    }
    Ok(())
}

pub fn check_metamodule_safety() -> bool {
    std::path::Path::new(METAMODULE_SAFETY_FLAG).exists()
}

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
