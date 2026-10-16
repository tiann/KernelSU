//! Hot install: make a freshly installed module live without a reboot.
//!
//! A module opts in from customize.sh with `MODULE_HOT_INSTALL_REQUEST=true` (and optionally
//! `MODULE_HOT_RUN_SCRIPT=<script>`). installer.sh records the request in a marker file inside
//! the staged module dir; after the install script succeeds ksud moves the staged dir over the
//! live one, applies sepolicy.rule and runs the requested script. Anything that only happens at
//! boot (mounting system/, system.prop, post-fs-data.sh, ...) still waits for the next boot.

use crate::{
    assets, defs,
    module::{ScriptWait, exec_script_with},
    sepolicy,
    utils::{ensure_dir_exists, is_safe_mode, switch_mnt_ns},
};

use anyhow::{Context, Result, bail};
use log::{info, warn};

use std::{
    fs::{canonicalize, remove_dir_all, remove_file, rename},
    os::unix::process::CommandExt,
    path::{Component, Path, PathBuf},
    process::{Command, Stdio},
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

/// Resolve the run script to a path relative to the module dir. A missing script is skipped,
/// like the metamodule implementations do.
fn check_run_script(staged_dir: &Path, script: &str) -> Result<Option<PathBuf>> {
    let mut components = Path::new(script).components().peekable();
    while components.next_if_eq(&Component::CurDir).is_some() {}
    let relative: PathBuf = components.collect();
    if relative.as_os_str().is_empty()
        || !relative
            .components()
            .all(|c| matches!(c, Component::Normal(_)))
    {
        bail!("invalid MODULE_HOT_RUN_SCRIPT '{script}'");
    }

    let path = staged_dir.join(&relative);
    if path.symlink_metadata().is_err() {
        println!("! MODULE_HOT_RUN_SCRIPT '{script}' not found, skipping it");
        return Ok(None);
    }
    // Symlinks may only point to files inside the module
    let resolved = canonicalize(&path)?;
    if !resolved.is_file() || !resolved.starts_with(canonicalize(staged_dir)?) {
        bail!("MODULE_HOT_RUN_SCRIPT '{script}' is not a file inside the module");
    }
    Ok(Some(relative))
}

fn check(module_id: &str, is_metamodule: bool, request: &Request) -> Result<Option<PathBuf>> {
    if is_metamodule {
        bail!("metamodules are always applied at boot");
    }
    if is_safe_mode() {
        bail!("modules are disabled in safe mode");
    }
    let Some(script) = &request.run_script else {
        return Ok(None);
    };
    check_run_script(&Path::new(defs::MODULE_UPDATE_DIR).join(module_id), script)
}

/// Move the live module dir out of the way. Running processes and existing mounts may still
/// reference it, so it is only deleted on the next boot.
fn stash(live_dir: &Path, module_id: &str) -> Result<PathBuf> {
    ensure_dir_exists(defs::HOT_INSTALL_TRASH_DIR)?;
    let nanos = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_nanos())
        .unwrap_or_default();
    let target = Path::new(defs::HOT_INSTALL_TRASH_DIR).join(format!("{module_id}.{nanos}"));
    // Never delete the live dir here: on failure the regular update path still needs it
    rename(live_dir, &target).with_context(|| format!("Failed to move {}", live_dir.display()))?;
    Ok(target)
}

fn swap(module_id: &str) -> Result<()> {
    let staged_dir = Path::new(defs::MODULE_UPDATE_DIR).join(module_id);
    let live_dir = Path::new(defs::MODULE_DIR).join(module_id);

    let stashed = if live_dir.exists() {
        Some(stash(&live_dir, module_id)?)
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

    let run_script = match check(module_id, is_metamodule, request)
        .and_then(|script| swap(module_id).map(|()| script))
    {
        Ok(script) => script,
        Err(e) => {
            println!("- Hot install skipped: {e:#}");
            println!("- Reboot to apply the module");
            return false;
        }
    };
    info!("Module {module_id} hot installed");

    let live_dir = Path::new(defs::MODULE_DIR).join(module_id);

    // Rules are additive, so loading the new ones now is safe; stale ones go away on reboot
    let rule_file = live_dir.join("sepolicy.rule");
    if rule_file.exists() && sepolicy::apply_file(&rule_file).is_err() {
        println!("! Failed to apply sepolicy.rule");
    }

    if !live_dir.join("skip_mount").exists()
        && ["system", "vendor", "product", "system_ext", "odm"]
            .iter()
            .any(|partition| live_dir.join(partition).is_dir())
    {
        println!("- Note: partition files are mounted on the next boot");
    }

    if live_dir.join(defs::MODULE_PREINIT_DIR).is_dir() {
        println!("- Note: preinit actions run from the next boot");
    }

    if let Some(script) = run_script {
        // The installer usually runs in the manager's mount namespace; boot scripts run in
        // init's, so mounts made by the script must land there too
        if let Err(e) = switch_mnt_ns(1) {
            warn!("Failed to switch to init mount namespace: {e}");
        }
        println!("- Running {}", script.display());
        let deadline = Instant::now() + defs::HOT_INSTALL_SCRIPT_TIMEOUT;
        let result = exec_script_with(
            live_dir.join(script),
            ScriptWait::Until(deadline),
            |command| {
                // Output goes to the installer's pipe, which the manager closes once the install
                // ends; ignore SIGPIPE so background processes started here keep running
                command.stdin(Stdio::null());
                unsafe {
                    command.pre_exec(|| {
                        libc::signal(libc::SIGPIPE, libc::SIG_IGN);
                        Ok(())
                    });
                }
            },
        );
        if let Err(e) = result {
            println!("! {e:#}");
        }
    }

    true
}

/// Drop module dirs replaced by hot installs during the previous boot. The trash is renamed
/// first and deleted in the background so a large one does not hold up boot.
pub fn clean_trash() {
    let trash = Path::new(defs::HOT_INSTALL_TRASH_DIR);
    if !trash.exists() {
        return;
    }
    let old = Path::new(defs::WORKING_DIR).join("hot_install_trash.old");
    // Left over if the previous background removal did not finish
    if old.exists()
        && let Err(e) = remove_dir_all(&old)
    {
        warn!("Failed to clean {}: {e}", old.display());
    }
    if let Err(e) = rename(trash, &old) {
        warn!("Failed to move {}: {e}", trash.display());
        return;
    }
    let spawned = Command::new(assets::BUSYBOX_PATH)
        .args(["rm", "-rf"])
        .arg(&old)
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .spawn();
    if let Err(e) = spawned {
        warn!("Failed to spawn trash removal: {e}");
        if let Err(e) = remove_dir_all(&old) {
            warn!("Failed to clean {}: {e}", old.display());
        }
    }
}
