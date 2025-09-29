// Stage execution logic for module system
use anyhow::{Context, Result};
use log::{info, warn};
use std::path::Path;

use crate::{defs, module, mount, utils};

/// Run stage scripts for modules and common scripts
pub fn run_stage(stage: &str) -> Result<()> {
    match stage {
        "post-fs-data" => run_post_fs_data(),
        "service" => run_service(),
        "boot-completed" => run_boot_completed(),
        _ => {
            warn!("Unknown stage: {}", stage);
            Ok(())
        }
    }
}

fn run_post_fs_data() -> Result<()> {
    info!("Running post-fs-data stage");

    utils::umask(0);

    // Check for Magisk
    if utils::has_magisk() {
        warn!("Magisk detected, skip post-fs-data!");
        return Ok(());
    }

    let safe_mode = ksu_core::safety::is_safe_mode();
    let metamodule_safety = utils::check_metamodule_safety();

    if safe_mode || metamodule_safety {
        // we should still mount modules.img to `/data/adb/modules` in safe mode
        // because we may need to operate the module dir in safe mode
        warn!("safe mode, skip common post-fs-data.d scripts");
    } else {
        // Then exec common post-fs-data scripts
        if let Err(e) = module::exec_common_scripts("post-fs-data.d", true) {
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
    utils::ensure_clean_dir(module_dir)?;

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
    // because we may need to operate the module dir in safe mode
    info!("mount module image: {target_update_img} to {module_dir}");
    mount::AutoMountExt4::try_new(target_update_img, module_dir, false)
        .with_context(|| "mount module image failed".to_string())?;

    // Report module image mounted (only this event is reported by modsys)
    ksu_core::ksucalls::report_module_mounted();

    // if we are in safe mode, we should disable all modules
    if safe_mode || metamodule_safety {
        warn!("safe mode/metamodule safety, skip post-fs-data scripts and disable all modules!");
        if let Err(e) = crate::module::disable_all_modules() {
            warn!("disable all modules failed: {e}");
        }
        return Ok(());
    }

    if let Err(e) = module::prune_modules() {
        warn!("prune modules failed: {e}");
    }

    // Restore SELinux contexts for daemon and module dir
    if let Err(e) = ksu_core::restorecon::restorecon() {
        warn!("restorecon failed: {e}");
    }

    // load sepolicy.rule for active modules
    if let Err(e) = crate::module::load_sepolicy_rule() {
        warn!("load sepolicy.rule failed: {e}");
    }

    // mount temp dir
    if let Err(e) = mount::mount_tmpfs(defs::TEMP_DIR) {
        warn!("do temp dir mount failed: {e}");
    }

    // exec modules post-fs-data scripts
    // TODO: Add timeout
    if let Err(e) = module::exec_stage_script("post-fs-data", true) {
        warn!("exec post-fs-data scripts failed: {e}");
    }

    // load system.prop
    if let Err(e) = module::load_system_prop() {
        warn!("load system.prop failed: {e}");
    }

    // mount module systemlessly by overlay
    if let Err(e) = mount::mount_modules_systemlessly() {
        warn!("do systemless mount failed: {e}");
    }

    run_stage_scripts("post-mount", true);

    std::env::set_current_dir("/").with_context(|| "failed to chdir to /")?;

    Ok(())
}

fn run_service() -> Result<()> {
    info!("Running service stage");

    run_stage_scripts("service", false);

    Ok(())
}

fn run_boot_completed() -> Result<()> {
    info!("Running boot-completed stage");
    // Event reporting is handled by ksud; do not report boot-completed here

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

    run_stage_scripts("boot-completed", false);

    Ok(())
}

fn run_stage_scripts(stage: &str, block: bool) {
    utils::umask(0);

    if utils::has_magisk() {
        warn!("Magisk detected, skip {stage}");
        return;
    }

    if ksu_core::safety::is_safe_mode() {
        warn!("safe mode, skip {stage} scripts");
        return;
    }

    if let Err(e) = module::exec_common_scripts(&format!("{stage}.d"), block) {
        warn!("Failed to exec common {stage} scripts: {e}");
    }
    if let Err(e) = module::exec_stage_script(stage, block) {
        warn!("Failed to exec {stage} scripts: {e}");
    }
}
