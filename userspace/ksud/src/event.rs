use std::path::{Path, PathBuf};

use crate::{defs, utils::{mount_image, ensure_clean_dir}};
use anyhow::{bail, Result};
use subprocess::Exec;

pub fn on_post_data_fs() -> Result<()> {
    let module_update_img = defs::MODULE_UPDATE_IMG;
    let module_img = defs::MODULE_IMG;
    let module_dir = defs::MODULE_DIR;
    let module_update_flag = Path::new(defs::WORKING_DIR).join(defs::UPDATE_FILE_NAME);

    // modules.img is the default image
    let mut target_update_img = &module_img;

    // we should clean the module mount point if it exists
    ensure_clean_dir(module_dir)?;

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
        // no image exist, do nothing for module!
        return Ok(());
    }

    println!("mount {} to {}", target_update_img, module_dir);
    mount_image(target_update_img, module_dir)?;

    // construct overlay mount params
    let dir = std::fs::read_dir(module_dir);
    let Ok(dir) = dir else {
        bail!("open {} failed", defs::MODULE_DIR);
    };

    let mut lowerdir: Vec<String> = Vec::new();
    for entry in dir.flatten() {
        let module = entry.path();
        if !module.is_dir() {
            continue;
        }
        let disabled = module.join(defs::DISABLE_FILE_NAME).exists();
        if disabled {
            println!("module: {} is disabled, ignore!", module.display());
            continue;
        }

        let mut module_system = PathBuf::new();
        module_system.push(&module);
        module_system.push("system");

        if !module_system.as_path().exists() {
            println!(
                "module: {} has no system overlay, ignore!",
                module.display()
            );
            continue;
        }
        lowerdir.push(format!("{}", module_system.display()));
    }

    if lowerdir.is_empty() {
        println!("lowerdir is empty, ignore!");
        return Ok(());
    }

    // add /system as the last lowerdir
    lowerdir.push(String::from("/system"));

    let lowerdir = lowerdir.join(":");
    println!("lowerdir: {}", lowerdir);

    let mount_args = format!(
        "mount -t overlay overlay -o ro,lowerdir={} /system",
        lowerdir
    );
    let result = Exec::shell(mount_args).join()?;
    if !result.success() {
        println!("mount overlay failed");
    }

    // module mounted, exec modules post-fs-data scripts
    if !crate::utils::is_safe_mode().unwrap_or(false) {
        // todo: Add timeout
        let _ = crate::module::exec_post_fs_data();
        let _ = crate::module::load_system_prop();
    } else {
        println!("safe mode, skip module post-fs-data scripts");
    }

    Ok(())
}

pub fn on_services() -> Result<()> {
    if !crate::utils::is_safe_mode().unwrap_or(false) {
        // todo: Add timeout
        let _ = crate::module::exec_service();
    } else {
        println!("safe mode, skip module service scripts");
    }

    Ok(())
}

pub fn on_boot_completed() -> Result<()> {
    let module_update_img = Path::new(defs::MODULE_UPDATE_IMG);
    let module_img = Path::new(defs::MODULE_IMG);
    if module_update_img.exists() {
        // this is a update and we successfully booted
        std::fs::rename(module_update_img, module_img)?;
    }
    Ok(())
}

pub fn daemon() -> Result<()> {
    Ok(())
}

pub fn install() -> Result<()> {
    let src = "/proc/self/exe";
    let dst = defs::DAEMON_PATH;

    std::fs::copy(src, dst)?;
    Ok(())
}
