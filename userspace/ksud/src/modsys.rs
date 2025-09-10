// Module System Interface - provides transparent forwarding to selected modsys implementation
use anyhow::{Context, Result, bail};
use log::{debug, info, warn};
use serde_json::Value;
use std::{
    fs,
    path::Path,
    process::{Command, ExitStatus},
};

use crate::defs;

const MODSYS_SELECTED_FILE: &str = "/data/adb/ksu/modsys.selected";
const MODSYS_BINARY_DIR: &str = "/data/adb/ksu/msp";
const DEFAULT_MODSYS: &str = "ksu-modsys-overlay";

/// Get the currently selected modsys implementation
fn get_selected_modsys() -> String {
    match fs::read_to_string(MODSYS_SELECTED_FILE) {
        Ok(content) => {
            let selected = content.trim();
            if !selected.is_empty() {
                selected.to_string()
            } else {
                DEFAULT_MODSYS.to_string()
            }
        }
        Err(_) => {
            debug!(
                "modsys.selected not found, using default: {}",
                DEFAULT_MODSYS
            );
            DEFAULT_MODSYS.to_string()
        }
    }
}

/// Get the path to the selected modsys binary
fn get_modsys_binary_path() -> Result<String> {
    let selected = get_selected_modsys();
    let binary_path = format!("{}/{}", MODSYS_BINARY_DIR, selected);

    if !Path::new(&binary_path).exists() {
        bail!("Selected modsys binary not found: {}", binary_path);
    }

    Ok(binary_path)
}

/// Check if the selected modsys implementation is supported
pub fn check_supported() -> Result<()> {
    let binary_path = get_modsys_binary_path()?;

    let output = Command::new(&binary_path)
        .arg("--supported")
        .output()
        .with_context(|| format!("Failed to execute modsys binary: {}", binary_path))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        bail!("Modsys {} is not supported: {}", binary_path, stderr);
    }

    let stdout = String::from_utf8_lossy(&output.stdout);
    let response: Value =
        serde_json::from_str(&stdout).with_context(|| "Failed to parse modsys response")?;

    let code = response["code"].as_i64().unwrap_or(-1);
    let msg = response["msg"].as_str().unwrap_or("Unknown");

    if code != 0 {
        bail!("Modsys not supported: {}", msg);
    }

    info!("Modsys {} is supported: {}", binary_path, msg);
    Ok(())
}

/// Execute a modsys command and return the exit status
fn execute_modsys_command(args: &[String]) -> Result<ExitStatus> {
    let binary_path = get_modsys_binary_path()?;

    debug!("Executing modsys command: {} {:?}", binary_path, args);

    let status = Command::new(&binary_path)
        .args(args)
        .env(
            "KSU_KERNEL_VER_CODE",
            ksu_core::ksucalls::get_version().to_string(),
        )
        .env("KSU_VER", defs::VERSION_NAME)
        .env("KSU_VER_CODE", defs::VERSION_CODE)
        .status()
        .with_context(|| format!("Failed to execute modsys binary: {}", binary_path))?;

    Ok(status)
}

/// Execute a modsys command and ensure it succeeds
fn run_modsys_command(args: &[String]) -> Result<()> {
    let status = execute_modsys_command(args)?;

    if !status.success() {
        let args_str = args.join(" ");
        bail!("Modsys command failed: {}", args_str);
    }

    Ok(())
}

/// Install a module
pub fn install_module(zip: &str) -> Result<()> {
    check_supported()?;
    run_modsys_command(&["install".to_string(), zip.to_string()])
}

/// Uninstall a module
pub fn uninstall_module(id: &str) -> Result<()> {
    check_supported()?;
    run_modsys_command(&["uninstall".to_string(), id.to_string()])
}

/// Enable a module
pub fn enable_module(id: &str) -> Result<()> {
    check_supported()?;
    run_modsys_command(&["enable".to_string(), id.to_string()])
}

/// Disable a module
pub fn disable_module(id: &str) -> Result<()> {
    check_supported()?;
    run_modsys_command(&["disable".to_string(), id.to_string()])
}

/// List all modules
pub fn list_modules() -> Result<()> {
    check_supported()?;
    run_modsys_command(&["list".to_string()])
}

/// Run action for a module
pub fn run_action(id: &str) -> Result<()> {
    check_supported()?;
    run_modsys_command(&["action".to_string(), id.to_string()])
}

/// Shrink module images
pub fn shrink_images() -> Result<()> {
    check_supported()?;
    run_modsys_command(&["shrink".to_string()])
}

/// Execute stage scripts
pub fn stage(stage_name: &str) -> Result<()> {
    check_supported()?;
    run_modsys_command(&["stage".to_string(), stage_name.to_string()])
}

/// Mount modules systemlessly (for debug)
pub fn mount_systemless() -> Result<()> {
    check_supported()?;
    run_modsys_command(&["mount".to_string(), "systemless".to_string()])
}

/// Initialize the modsys system - create default selection if not exists
pub fn init() -> Result<()> {
    // Create modsys directories if they don't exist
    if let Err(e) = fs::create_dir_all(MODSYS_BINARY_DIR) {
        warn!("Failed to create modsys binary directory: {}", e);
    }

    // Create default selection file if it doesn't exist
    if !Path::new(MODSYS_SELECTED_FILE).exists() {
        if let Err(e) = fs::write(MODSYS_SELECTED_FILE, DEFAULT_MODSYS) {
            warn!("Failed to create default modsys selection: {}", e);
        } else {
            info!("Created default modsys selection: {}", DEFAULT_MODSYS);
        }
    }

    Ok(())
}
