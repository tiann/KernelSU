// KernelSU Module System Interface
//
// All module management functionality has been migrated to ksu-modsys-overlay.
// This file remains as a placeholder for compatibility.
//
// Module operations are now handled through the modsys interface layer,
// which delegates to the selected module system implementation.

use anyhow::Result;
use log::warn;

// Legacy compatibility functions - these now return appropriate messages
// directing users to use the modsys interface

/// Install module - use `ksud module install <zip>` instead
#[deprecated(note = "Use modsys interface through CLI: ksud module install <zip>")]
pub fn install_module(_zip: &str) -> Result<()> {
    warn!("Direct module::install_module calls are deprecated. Use modsys interface.");
    anyhow::bail!("Module installation must be done through the CLI interface");
}

/// Uninstall module - use `ksud module uninstall <id>` instead  
#[deprecated(note = "Use modsys interface through CLI: ksud module uninstall <id>")]
pub fn uninstall_module(_id: &str) -> Result<()> {
    warn!("Direct module::uninstall_module calls are deprecated. Use modsys interface.");
    anyhow::bail!("Module uninstallation must be done through the CLI interface");
}

/// Enable module - use `ksud module enable <id>` instead
#[deprecated(note = "Use modsys interface through CLI: ksud module enable <id>")]
pub fn enable_module(_id: &str) -> Result<()> {
    warn!("Direct module::enable_module calls are deprecated. Use modsys interface.");
    anyhow::bail!("Module enabling must be done through the CLI interface");
}

/// Disable module - use `ksud module disable <id>` instead
#[deprecated(note = "Use modsys interface through CLI: ksud module disable <id>")]
pub fn disable_module(_id: &str) -> Result<()> {
    warn!("Direct module::disable_module calls are deprecated. Use modsys interface.");
    anyhow::bail!("Module disabling must be done through the CLI interface");
}

/// List modules - use `ksud module list` instead
#[deprecated(note = "Use modsys interface through CLI: ksud module list")]
pub fn list_modules() -> Result<()> {
    warn!("Direct module::list_modules calls are deprecated. Use modsys interface.");
    anyhow::bail!("Module listing must be done through the CLI interface");
}

/// Run action - use `ksud module action <id>` instead
#[deprecated(note = "Use modsys interface through CLI: ksud module action <id>")]
pub fn run_action(_id: &str) -> Result<()> {
    warn!("Direct module::run_action calls are deprecated. Use modsys interface.");
    anyhow::bail!("Module actions must be run through the CLI interface");
}

/// Shrink images - use `ksud module shrink` instead
#[deprecated(note = "Use modsys interface through CLI: ksud module shrink")]
pub fn shrink_ksu_images() -> Result<()> {
    warn!("Direct module::shrink_ksu_images calls are deprecated. Use modsys interface.");
    anyhow::bail!("Image shrinking must be done through the CLI interface");
}

// These functions are only used during uninstall process and can't be easily delegated
// We'll keep minimal implementations for compatibility

/// Legacy function for uninstall process - functionality limited
#[deprecated(note = "Module uninstall process now handled by modsys")]
pub fn uninstall_all_modules() -> Result<()> {
    warn!("uninstall_all_modules: Module management delegated to modsys");
    // During uninstall, we can't rely on modsys, so we do basic cleanup
    Ok(())
}

/// Legacy function for cleanup - functionality limited  
#[deprecated(note = "Module cleanup now handled by modsys")]
pub fn prune_modules() -> Result<()> {
    warn!("prune_modules: Module management delegated to modsys");
    // During uninstall, we can't rely on modsys, so we do basic cleanup
    Ok(())
}

/// Legacy function - not used in current architecture
#[deprecated(note = "Script execution now handled by modsys")]
pub fn exec_stage_script(_stage: &str, _block: bool) -> Result<()> {
    warn!("exec_stage_script: Stage execution delegated to modsys");
    Ok(())
}

/// Legacy function - not used in current architecture
#[deprecated(note = "Script execution now handled by modsys")]
pub fn exec_common_scripts(_dir: &str, _wait: bool) -> Result<()> {
    warn!("exec_common_scripts: Script execution delegated to modsys");
    Ok(())
}

/// Legacy function - not used in current architecture
#[deprecated(note = "Property loading now handled by modsys")]
pub fn load_system_prop() -> Result<()> {
    warn!("load_system_prop: Property loading delegated to modsys");
    Ok(())
}

/// Legacy function - not used in current architecture
#[deprecated(note = "SEPolicy loading now handled by modsys")]
pub fn load_sepolicy_rule() -> Result<()> {
    warn!("load_sepolicy_rule: SEPolicy loading delegated to modsys");
    Ok(())
}

/// Legacy function - not used in current architecture
#[deprecated(note = "Module disabling now handled by modsys")]
pub fn disable_all_modules() -> Result<()> {
    warn!("disable_all_modules: Module management delegated to modsys");
    Ok(())
}