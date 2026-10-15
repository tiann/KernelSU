//! Hot install: make a freshly installed module live without a reboot.
//!
//! A module opts in from customize.sh with `MODULE_HOT_INSTALL_REQUEST=true` (and optionally
//! `MODULE_HOT_RUN_SCRIPT=<script>`). installer.sh records the request in a marker file inside
//! the staged module dir; after the install script succeeds ksud moves the staged dir over the
//! live one, applies sepolicy.rule and runs the requested script. Anything that only happens at
//! boot (mounting system/, system.prop, post-fs-data.sh, ...) still waits for the next boot.

use crate::{
    defs,
    module::{ScriptWait, exec_script},
    sepolicy,
    utils::{ensure_dir_exists, is_safe_mode, switch_mnt_ns},
};

use anyhow::{Context, Result, bail};
use log::{info, warn};

use std::{
    fs::{remove_dir_all, remove_file, rename},
    path::{Component, Path, PathBuf},
    time::{Instant, SystemTime, UNIX_EPOCH},
};

pub struct Request {
    run_script: Option<String>,
}

/// Consume the hot install marker left in the staged module dir, if any.
pub fn take_request(staged_dir: &Path) -> Option<Request> {
    let marker = staged_dir.join(defs::HOT_INSTALL_MARKER);
    let content = std::fs::read_to_string(&marker).ok()?;
    if let Err(e) = remove_file(&marker) {
        warn!("Failed to remove {}: {e}", marker.display());
    }
    let script = content.trim();
    Some(Request {
        run_script: (!script.is_empty()).then(|| script.to_owned()),
    })
}

fn check_run_script(staged_dir: &Path, script: &str) -> Result<()> {
    let path = Path::new(script);
    if !path.components().all(|c| matches!(c, Component::Normal(_))) {
        bail!("invalid MODULE_HOT_RUN_SCRIPT '{script}'");
    }
    if !staged_dir.join(path).is_file() {
        bail!("MODULE_HOT_RUN_SCRIPT '{script}' not found");
    }
    Ok(())
}

fn check(module_id: &str, is_metamodule: bool, request: &Request) -> Result<()> {
    if is_metamodule {
        bail!("metamodules are always applied at boot");
    }
    if is_safe_mode() {
        bail!("modules are disabled in safe mode");
    }
    if let Some(script) = &request.run_script {
        check_run_script(&Path::new(defs::MODULE_UPDATE_DIR).join(module_id), script)?;
    }
    Ok(())
}

/// Move the live module dir out of the way. Running processes and existing mounts may still
/// reference it, so it is only deleted on the next boot.
fn stash(live_dir: &Path, module_id: &str) -> Result<Option<PathBuf>> {
    ensure_dir_exists(defs::HOT_INSTALL_TRASH_DIR)?;
    let nanos = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_nanos())
        .unwrap_or_default();
    let target = Path::new(defs::HOT_INSTALL_TRASH_DIR).join(format!("{module_id}.{nanos}"));
    match rename(live_dir, &target) {
        Ok(()) => Ok(Some(target)),
        Err(e) if e.raw_os_error() == Some(libc::EXDEV) => {
            remove_dir_all(live_dir)
                .with_context(|| format!("Failed to remove {}", live_dir.display()))?;
            Ok(None)
        }
        Err(e) => Err(e).with_context(|| format!("Failed to move {}", live_dir.display())),
    }
}

fn swap(module_id: &str) -> Result<()> {
    let staged_dir = Path::new(defs::MODULE_UPDATE_DIR).join(module_id);
    let live_dir = Path::new(defs::MODULE_DIR).join(module_id);

    let stashed = if live_dir.exists() {
        stash(&live_dir, module_id)?
    } else {
        None
    };

    if let Err(e) = rename(&staged_dir, &live_dir) {
        // Put the old dir back so the regular update-on-reboot path still works
        if let Some(old) = stashed
            && let Err(e) = rename(&old, &live_dir)
        {
            warn!("Failed to restore {}: {e}", live_dir.display());
        }
        return Err(e).with_context(|| format!("Failed to move {}", staged_dir.display()));
    }
    Ok(())
}

/// Try to apply the staged module right away. Returns false if the caller should fall back to
/// the regular update-on-reboot path.
pub fn apply(module_id: &str, is_metamodule: bool, request: &Request) -> bool {
    println!("- Hot install requested");

    if let Err(e) = check(module_id, is_metamodule, request).and_then(|()| swap(module_id)) {
        println!("- Hot install skipped: {e:#}");
        println!("- Reboot to apply the module");
        return false;
    }
    info!("Module {module_id} hot installed");

    let live_dir = Path::new(defs::MODULE_DIR).join(module_id);

    // Rules are additive, so loading the new ones now is safe; stale ones go away on reboot
    let rule_file = live_dir.join("sepolicy.rule");
    if rule_file.exists() && sepolicy::apply_file(&rule_file).is_err() {
        println!("! Failed to apply sepolicy.rule");
    }

    if live_dir.join("system").is_dir() && !live_dir.join("skip_mount").exists() {
        println!("- Note: files under system/ are mounted on the next boot");
    }

    if let Some(script) = &request.run_script {
        // The installer usually runs in the manager's mount namespace; boot scripts run in
        // init's, so mounts made by the script must land there too
        if let Err(e) = switch_mnt_ns(1) {
            warn!("Failed to switch to init mount namespace: {e}");
        }
        println!("- Running {script}");
        let deadline = Instant::now() + defs::HOT_INSTALL_SCRIPT_TIMEOUT;
        if let Err(e) = exec_script(live_dir.join(script), ScriptWait::Until(deadline)) {
            println!("! {e:#}");
        }
    }

    true
}

/// Drop module dirs replaced by hot installs during the previous boot.
pub fn clean_trash() {
    let trash = Path::new(defs::HOT_INSTALL_TRASH_DIR);
    if trash.exists()
        && let Err(e) = remove_dir_all(trash)
    {
        warn!("Failed to clean {}: {e}", trash.display());
    }
}
