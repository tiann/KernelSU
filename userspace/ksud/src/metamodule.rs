//! Metamodule management
//!
//! This module handles all metamodule-related functionality.
//! Metamodules are special modules that manage how regular modules are mounted
//! and provide hooks for module installation/uninstallation.

use anyhow::{Context, Result, ensure};
use log::{info, warn};
use std::{
    collections::HashMap,
    path::{Path, PathBuf},
    process::Command,
};

use crate::{assets, defs};

/// Determine whether the provided module properties mark it as a metamodule
pub fn is_metamodule(props: &HashMap<String, String>) -> bool {
    props
        .get("metamodule")
        .map(|s| {
            let trimmed = s.trim();
            trimmed == "1" || trimmed.eq_ignore_ascii_case("true")
        })
        .unwrap_or(false)
}

/// Get metamodule path if it exists
/// The metamodule is stored in /data/adb/modules/{id} with a symlink at /data/adb/metamodule
pub fn get_metamodule_path() -> Option<PathBuf> {
    let path = Path::new(defs::METAMODULE_DIR);

    // Check if symlink exists and resolve it
    if path.is_symlink()
        && let Ok(target) = std::fs::read_link(path)
    {
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
            && is_metamodule(&props)
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
    let symlink_path = Path::new(defs::METAMODULE_DIR.trim_end_matches('/'));

    if symlink_path.is_symlink() {
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

/// Check if metamodule script exists and is ready to execute
/// Returns None if metamodule doesn't exist, is disabled, or script is missing
/// Returns Some(script_path) if script is ready to execute
fn check_metamodule_script(script_name: &str) -> Option<PathBuf> {
    // Check if metamodule exists
    let metamodule_path = get_metamodule_path()?;

    // Check if metamodule is disabled
    if metamodule_path.join(defs::DISABLE_FILE_NAME).exists() {
        info!("Metamodule is disabled, skipping {}", script_name);
        return None;
    }

    // Check if script exists
    let script_path = metamodule_path.join(script_name);
    if !script_path.exists() {
        return None;
    }

    Some(script_path)
}

/// Execute metamodule's metauninstall.sh for a specific module
pub(crate) fn exec_metauninstall_script(module_id: &str) -> Result<()> {
    let Some(metauninstall_path) = check_metamodule_script(defs::METAMODULE_METAUNINSTALL_SCRIPT)
    else {
        return Ok(());
    };

    info!(
        "Executing metamodule metauninstall.sh for module: {}",
        module_id
    );

    let result = Command::new(assets::BUSYBOX_PATH)
        .args(["sh", metauninstall_path.to_str().unwrap()])
        .current_dir(metauninstall_path.parent().unwrap())
        .envs(crate::module::get_common_script_envs())
        .env("MODULE_ID", module_id)
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
    let Some(mount_script) = check_metamodule_script(defs::METAMODULE_MOUNT_SCRIPT) else {
        return Ok(());
    };

    info!("Executing mount script for metamodule");

    let result = Command::new(assets::BUSYBOX_PATH)
        .args(["sh", mount_script.to_str().unwrap()])
        .envs(crate::module::get_common_script_envs())
        .env("MODULE_DIR", module_dir)
        .status()?;

    ensure!(
        result.success(),
        "Metamodule mount script failed with status: {:?}",
        result
    );

    info!("Metamodule mount script executed successfully");
    Ok(())
}

/// Execute metamodule script for a specific stage
pub fn exec_stage_script(stage: &str, block: bool) -> Result<()> {
    let Some(script_path) = check_metamodule_script(&format!("{}.sh", stage)) else {
        return Ok(());
    };

    info!("Executing metamodule {}.sh", stage);
    crate::module::exec_script(&script_path, block)?;
    info!("Metamodule {}.sh executed successfully", stage);
    Ok(())
}
