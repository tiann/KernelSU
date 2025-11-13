use anyhow::{Context, Result, bail};
use log::{info, warn};
use std::{collections::HashMap, path::Path, process::Command};

use crate::module::prune_modules;
use crate::utils::is_safe_mode;
use crate::{
    assets, defs, ksucalls, mount, restorecon,
    utils::{self, ensure_clean_dir},
};

/// Execute metamodule mount script
fn execute_metamodule_mount(module_dir: &str) -> Result<()> {
    // Find the active metamodule
    let metamodule_id = crate::module::find_metamodule()?;

    let Some(metamodule_id) = metamodule_id else {
        info!("No metamodule found");
        return Ok(());
    };

    info!("Found metamodule: {}", metamodule_id);

    let metamodule_path = Path::new(module_dir).join(&metamodule_id);

    // Check if metamodule is disabled
    if metamodule_path.join(defs::DISABLE_FILE_NAME).exists() {
        warn!("Metamodule {} is disabled, skipping mount", metamodule_id);
        return Ok(());
    }

    let mount_script = metamodule_path.join(defs::METAMODULE_MOUNT_SCRIPT);
    if !mount_script.exists() {
        warn!(
            "Metamodule {} does not have {} script",
            metamodule_id,
            defs::METAMODULE_MOUNT_SCRIPT
        );
        return Ok(());
    }

    info!("Executing mount script for metamodule {}", metamodule_id);

    // Execute the mount script
    let result = Command::new(assets::BUSYBOX_PATH)
        .args(["sh", mount_script.to_str().unwrap()])
        .env("ASH_STANDALONE", "1")
        .env("KSU", "true")
        .env("KSU_KERNEL_VER_CODE", ksucalls::get_version().to_string())
        .env("KSU_VER_CODE", defs::VERSION_CODE)
        .env("KSU_VER", defs::VERSION_NAME)
        .env("METAMODULE_ID", &metamodule_id)
        .env("MODULE_DIR", module_dir)
        .status();

    match result {
        Ok(status) if status.success() => {
            info!("Metamodule {} mount script executed successfully", metamodule_id);
        }
        Ok(status) => {
            warn!(
                "Metamodule {} mount script failed with status: {:?}",
                metamodule_id, status
            );
        }
        Err(e) => {
            warn!("Failed to execute metamodule {} mount script: {}", metamodule_id, e);
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

    if safe_mode {
        // we should still mount modules.img to `/data/adb/modules` in safe mode
        // becuase we may need to operate the module dir in safe mode
        warn!("safe mode, skip common post-fs-data.d scripts");
    } else {
        // Then exec common post-fs-data scripts
        if let Err(e) = crate::module::exec_common_scripts("post-fs-data.d", true) {
            warn!("exec common post-fs-data scripts failed: {e}");
        }
    }

    let module_update_img = defs::MODULE_UPDATE_IMG;
    let module_img = defs::MODULE_IMG;
    let module_dir = defs::MODULE_DIR;
    let module_update_flag = Path::new(defs::WORKING_DIR).join(defs::UPDATE_FILE_NAME);

    // modules.img is the default image
    let mut target_update_img = &module_img;

    // we should clean the module mount point if it exists
    ensure_clean_dir(module_dir)?;

    assets::ensure_binaries(true).with_context(|| "Failed to extract bin assets")?;

    if Path::new(module_update_img).exists() {
        if module_update_flag.exists() {
            // if modules_update.img exists, and the the flag indicate this is an update
            // this make sure that if the update failed, we will fallback to the old image
            // if we boot succeed, we will rename the modules_update.img to modules.img #on_boot_complete
            target_update_img = &module_update_img;
            // And we should delete the flag immediately
            std::fs::remove_file(module_update_flag)?;
        } else {
            // if modules_update.img exists, but the flag not exist, we should delete it
            std::fs::remove_file(module_update_img)?;
        }
    }

    if !Path::new(target_update_img).exists() {
        return Ok(());
    }

    // we should always mount the module.img to module dir
    // becuase we may need to operate the module dir in safe mode
    info!("mount module image: {target_update_img} to {module_dir}");
    mount::AutoMountExt4::try_new(target_update_img, module_dir, false)
        .with_context(|| "mount module image failed".to_string())?;

    // tell kernel that we've mount the module, so that it can do some optimization
    ksucalls::report_module_mounted();

    // if we are in safe mode, we should disable all modules
    if safe_mode {
        warn!("safe mode, skip post-fs-data scripts and disable all modules!");
        if let Err(e) = crate::module::disable_all_modules() {
            warn!("disable all modules failed: {e}");
        }
        return Ok(());
    }

    if let Err(e) = prune_modules() {
        warn!("prune modules failed: {e}");
    }

    if let Err(e) = restorecon::restorecon() {
        warn!("restorecon failed: {e}");
    }

    // load sepolicy.rule
    if crate::module::load_sepolicy_rule().is_err() {
        warn!("load sepolicy.rule failed");
    }

    if let Err(e) = crate::profile::apply_sepolies() {
        warn!("apply root profile sepolicy failed: {e}");
    }

    // load feature config
    if is_safe_mode() {
        warn!("safe mode, skip load feature config");
    } else if let Err(e) = crate::feature::init_features() {
        warn!("init features failed: {e}");
    }

    // exec modules post-fs-data scripts
    // TODO: Add timeout
    if let Err(e) = crate::module::exec_stage_script("post-fs-data", true) {
        warn!("exec post-fs-data scripts failed: {e}");
    }

    // load system.prop
    if let Err(e) = crate::module::load_system_prop() {
        warn!("load system.prop failed: {e}");
    }

    // execute metamodule mount script
    if let Err(e) = execute_metamodule_mount(module_dir) {
        warn!("execute metamodule mount failed: {e}");
    }

    run_stage("post-mount", true);

    std::env::set_current_dir("/").with_context(|| "failed to chdir to /")?;

    Ok(())
}

fn run_stage(stage: &str, block: bool) {
    utils::umask(0);

    if utils::has_magisk() {
        warn!("Magisk detected, skip {stage}");
        return;
    }

    if crate::utils::is_safe_mode() {
        warn!("safe mode, skip {stage} scripts");
        return;
    }

    if let Err(e) = crate::module::exec_common_scripts(&format!("{stage}.d"), block) {
        warn!("Failed to exec common {stage} scripts: {e}");
    }
    if let Err(e) = crate::module::exec_stage_script(stage, block) {
        warn!("Failed to exec {stage} scripts: {e}");
    }
}

pub fn on_services() -> Result<()> {
    info!("on_services triggered!");
    run_stage("service", false);

    Ok(())
}

pub fn on_boot_completed() -> Result<()> {
    ksucalls::report_boot_complete();
    info!("on_boot_completed triggered!");
    let module_update_img = Path::new(defs::MODULE_UPDATE_IMG);
    let module_img = Path::new(defs::MODULE_IMG);
    if module_update_img.exists() {
        // this is a update and we successfully booted
        if std::fs::rename(module_update_img, module_img).is_err() {
            warn!("Failed to rename images, copy it now.",);
            utils::copy_sparse_file(module_update_img, module_img, false)
                .with_context(|| "Failed to copy images")?;
            std::fs::remove_file(module_update_img).with_context(|| "Failed to remove image!")?;
        }
    }

    run_stage("boot-completed", false);

    Ok(())
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
