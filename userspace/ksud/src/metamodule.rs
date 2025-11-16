//! Metamodule management
//!
//! This module handles all metamodule-related functionality.
//! Metamodules are special modules that manage how regular modules are mounted
//! and provide hooks for module installation/uninstallation.

use anyhow::{Context, Result, ensure};
use log::{info, warn};
use std::{
    path::{Path, PathBuf},
    process::Command,
};

use crate::{assets, defs, ksucalls};

/// Get metamodule path if it exists
/// The metamodule is stored in /data/adb/modules/{id} with a symlink at /data/adb/metamodule
pub fn get_metamodule_path() -> Option<PathBuf> {
    let path = Path::new(defs::METAMODULE_DIR);

    // Check if symlink exists and resolve it
    if path.is_symlink()
        && let Ok(target) = std::fs::read_link(path) {
            // If target is relative, resolve it
            let resolved = if target.is_absolute() {
                target
            } else {
                path.parent()?.join(target)
            };

            if resolved.exists() && resolved.is_dir() {
                return Some(resolved);
            } else {
                warn!(
                    "Metamodule symlink points to non-existent path: {:?}",
                    resolved
                );
            }
        }

    // Fallback: search for metamodule=1 in modules directory
    let mut result = None;
    let _ = crate::module::foreach_module(false, |module_path| {
        if let Ok(props) = crate::module::read_module_prop(module_path)
            && props
                .get("metamodule")
                .map(|s| s.trim().to_lowercase())
                .map(|s| s == "true" || s == "1")
                .unwrap_or(false)
            {
                info!("Found metamodule in modules directory: {:?}", module_path);
                result = Some(module_path.to_path_buf());
            }
        Ok(())
    });

    result
}

/// Check if metamodule exists
pub fn has_metamodule() -> bool {
    get_metamodule_path().is_some()
}

/// Create or update the metamodule symlink
/// Points /data/adb/metamodule -> /data/adb/modules/{module_id}
pub(crate) fn ensure_symlink(module_path: &Path) -> Result<()> {
    // METAMODULE_DIR might have trailing slash, so we need to trim it
    let symlink_path = Path::new(defs::METAMODULE_DIR.trim_end_matches('/'));

    info!(
        "Creating metamodule symlink: {:?} -> {:?}",
        symlink_path, module_path
    );

    // Remove existing symlink if it exists
    if symlink_path.exists() || symlink_path.is_symlink() {
        info!("Removing old metamodule symlink/path");
        if symlink_path.is_symlink() {
            std::fs::remove_file(symlink_path).with_context(|| "Failed to remove old symlink")?;
        } else {
            // Could be a directory, remove it
            std::fs::remove_dir_all(symlink_path)
                .with_context(|| "Failed to remove old directory")?;
        }
    }

    // Create symlink
    #[cfg(unix)]
    std::os::unix::fs::symlink(module_path, symlink_path)
        .with_context(|| format!("Failed to create symlink to {:?}", module_path))?;

    info!("Metamodule symlink created successfully");
    Ok(())
}

/// Remove the metamodule symlink
pub(crate) fn remove_symlink() -> Result<()> {
    let symlink_path = Path::new(defs::METAMODULE_DIR);

    if symlink_path.is_symlink() {
        info!("Removing metamodule symlink");
        std::fs::remove_file(symlink_path)
            .with_context(|| "Failed to remove metamodule symlink")?;
        info!("Metamodule symlink removed");
    }

    Ok(())
}

/// Get the install script content, using metainstall.sh from metamodule if available
/// Returns the script content to be executed
pub(crate) fn get_install_script(
    is_metamodule: bool,
    installer_content: &str,
    install_module_script: &str,
) -> Result<String> {
    // Check if there's a metamodule with metainstall.sh
    // Only apply this logic for regular modules (not when installing metamodule itself)
    let install_script = if !is_metamodule {
        if let Some(metamodule_path) = get_metamodule_path() {
            if metamodule_path.join(defs::DISABLE_FILE_NAME).exists() {
                info!("Metamodule is disabled, using default installer");
                install_module_script.to_string()
            } else {
                let metainstall_path = metamodule_path.join(defs::METAMODULE_METAINSTALL_SCRIPT);

                if metainstall_path.exists() {
                    info!("Using metainstall.sh from metamodule");
                    let metamodule_content = std::fs::read_to_string(&metainstall_path)
                        .with_context(|| "Failed to read metamodule metainstall.sh")?;
                    format!("{}\n{}\nexit 0\n", installer_content, metamodule_content)
                } else {
                    info!("Metamodule exists but has no metainstall.sh, using default installer");
                    install_module_script.to_string()
                }
            }
        } else {
            info!("No metamodule found, using default installer");
            install_module_script.to_string()
        }
    } else {
        info!("Installing metamodule, using default installer");
        install_module_script.to_string()
    };

    Ok(install_script)
}

/// Execute metamodule's metauninstall.sh for a specific module
pub(crate) fn exec_metauninstall_script(module_id: &str) -> Result<()> {
    // Check if metamodule exists
    let Some(metamodule_path) = get_metamodule_path() else {
        return Ok(());
    };

    // Check if metamodule is disabled
    if metamodule_path.join(defs::DISABLE_FILE_NAME).exists() {
        info!("Metamodule is disabled, skipping metauninstall");
        return Ok(());
    }

    // Check if metauninstall.sh exists
    let metauninstall_path = metamodule_path.join(defs::METAMODULE_METAUNINSTALL_SCRIPT);
    if !metauninstall_path.exists() {
        return Ok(());
    }

    info!(
        "Executing metamodule metauninstall.sh for module: {}",
        module_id
    );

    // Execute metauninstall.sh with module ID as environment variable
    let result = Command::new(assets::BUSYBOX_PATH)
        .args(["sh", metauninstall_path.to_str().unwrap()])
        .current_dir(&metamodule_path)
        .env("ASH_STANDALONE", "1")
        .env("MODULE_ID", module_id)
        .env("KSU", "true")
        .status()?;

    ensure!(
        result.success(),
        "Metamodule metauninstall.sh failed for module {}: {:?}",
        module_id,
        result
    );

    info!(
        "Metamodule metauninstall.sh executed successfully for {}",
        module_id
    );
    Ok(())
}

/// Execute metamodule mount script
pub fn exec_mount_script(module_dir: &str) -> Result<()> {
    let Some(metamodule_path) = get_metamodule_path() else {
        info!("No metamodule found");
        return Ok(());
    };

    // Check if metamodule is disabled
    if metamodule_path.join(defs::DISABLE_FILE_NAME).exists() {
        warn!("Metamodule is disabled, skipping mount");
        return Ok(());
    }

    let mount_script = metamodule_path.join(defs::METAMODULE_MOUNT_SCRIPT);
    if !mount_script.exists() {
        warn!(
            "Metamodule does not have {} script",
            defs::METAMODULE_MOUNT_SCRIPT
        );
        return Ok(());
    }

    info!("Executing mount script for metamodule");

    // Execute the mount script
    let result = Command::new(assets::BUSYBOX_PATH)
        .args(["sh", mount_script.to_str().unwrap()])
        .env("ASH_STANDALONE", "1")
        .env("KSU", "true")
        .env("KSU_KERNEL_VER_CODE", ksucalls::get_version().to_string())
        .env("KSU_VER_CODE", defs::VERSION_CODE)
        .env("KSU_VER", defs::VERSION_NAME)
        .env("MODULE_DIR", module_dir)
        .status();

    match result {
        Ok(status) if status.success() => {
            info!("Metamodule mount script executed successfully");
        }
        Ok(status) => {
            warn!("Metamodule mount script failed with status: {:?}", status);
        }
        Err(e) => {
            warn!("Failed to execute metamodule mount script: {}", e);
        }
    }

    Ok(())
}

/// Execute metamodule script for a specific stage
pub fn exec_stage_script(stage: &str, block: bool) -> Result<()> {
    let Some(metamodule_path) = get_metamodule_path() else {
        return Ok(());
    };

    if metamodule_path.join(defs::DISABLE_FILE_NAME).exists() {
        info!("Metamodule is disabled, skipping {}.sh", stage);
        return Ok(());
    }

    let script_path = metamodule_path.join(format!("{}.sh", stage));
    if !script_path.exists() {
        return Ok(());
    }

    info!("Executing metamodule {}.sh", stage);
    crate::module::exec_script(&script_path, block)?;
    info!("Metamodule {}.sh executed successfully", stage);
    Ok(())
}
