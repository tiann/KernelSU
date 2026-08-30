//! Metamodule management
//!
//! This module handles all metamodule-related functionality.
//! Metamodules are special modules that manage how regular modules are mounted
//! and provide hooks for module installation/uninstallation.

use anyhow::{Context, Result, anyhow, ensure};
use log::{debug, info, warn};
use std::{
    collections::{HashMap, HashSet},
    path::{Path, PathBuf},
    process::Command,
};

use crate::module::ModuleType::All;
use crate::{assets, defs, ksucalls};

/// Determine whether the provided module properties mark it as a metamodule
pub fn is_metamodule(props: &HashMap<String, String>) -> bool {
    props.get("metamodule").is_some_and(|s| {
        let trimmed = s.trim();
        trimmed == "1" || trimmed.eq_ignore_ascii_case("true")
    })
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
        }
        warn!(
            "Metamodule symlink points to non-existent path: {}",
            resolved.display()
        );
    }

    // Fallback: search for metamodule=1 in modules directory
    let mut result = None;
    let _ = crate::module::foreach_module(All, |module_path| {
        if let Ok(props) = crate::module::read_module_prop(module_path)
            && is_metamodule(&props)
        {
            info!(
                "Found metamodule in modules directory: {}",
                module_path.display()
            );
            result = Some(module_path.to_path_buf());
        }
        Ok(())
    });

    result
}

/// Get Metamodule Id
pub fn get_metamodule_id() -> Option<String> {
    get_metamodule_path().and_then(|path| {
        path.file_name()
            .and_then(|os_str| os_str.to_str())
            .map(ToString::to_string)
    })
}

/// Check if metamodule exists
pub fn has_metamodule() -> bool {
    get_metamodule_path().is_some()
}

fn unescape_mountinfo_field(field: &str) -> String {
    let bytes = field.as_bytes();
    let mut out = Vec::with_capacity(bytes.len());
    let mut i = 0;

    while i < bytes.len() {
        if bytes[i] == b'\\'
            && i + 3 < bytes.len()
            && matches!(bytes[i + 1], b'0'..=b'7')
            && matches!(bytes[i + 2], b'0'..=b'7')
            && matches!(bytes[i + 3], b'0'..=b'7')
        {
            out.push(
                (bytes[i + 1] - b'0') * 64 + (bytes[i + 2] - b'0') * 8 + (bytes[i + 3] - b'0'),
            );
            i += 4;
        } else {
            out.push(bytes[i]);
            i += 1;
        }
    }

    String::from_utf8_lossy(&out).into_owned()
}

fn path_is_at_or_under(path: &str, base: &str) -> bool {
    let base = base.trim_end_matches('/');
    path == base
        || path
            .strip_prefix(base)
            .is_some_and(|rest| rest.starts_with('/'))
}

fn is_module_mount_root(root: &str) -> bool {
    path_is_at_or_under(root, "/adb/modules")
        || path_is_at_or_under(root, "/adb/metamodule")
        || path_is_at_or_under(root, defs::MODULE_DIR)
        || path_is_at_or_under(root, defs::METAMODULE_DIR)
}

fn io_errno(err: &anyhow::Error) -> Option<i32> {
    err.downcast_ref::<std::io::Error>()
        .and_then(std::io::Error::raw_os_error)
}

#[derive(Debug)]
struct MountInfoEntry {
    id: u64,
    parent_id: u64,
    root: String,
    target: String,
    source: String,
}

fn read_mountinfo() -> Result<Vec<MountInfoEntry>> {
    let mountinfo = std::fs::read_to_string("/proc/self/mountinfo")
        .context("Failed to inspect module mounts")?;
    let mut entries = Vec::new();

    for line in mountinfo.lines() {
        let Some((left, right)) = line.split_once(" - ") else {
            continue;
        };
        let mut left = left.split_ascii_whitespace();
        let Some(id) = left.next().and_then(|s| s.parse::<u64>().ok()) else {
            continue;
        };
        let Some(parent_id) = left.next().and_then(|s| s.parse::<u64>().ok()) else {
            continue;
        };
        if left.next().is_none() {
            continue;
        }
        let Some(root) = left.next() else {
            continue;
        };
        let Some(target) = left.next() else {
            continue;
        };

        let mut right = right.split_ascii_whitespace();
        if right.next().is_none() {
            continue;
        }
        let Some(source) = right.next() else {
            continue;
        };

        entries.push(MountInfoEntry {
            id,
            parent_id,
            root: unescape_mountinfo_field(root),
            target: unescape_mountinfo_field(target),
            source: unescape_mountinfo_field(source),
        });
    }

    Ok(entries)
}

fn is_module_mount(entry: &MountInfoEntry) -> bool {
    entry.source == "KSU" || is_module_mount_root(&entry.root)
}

fn top_entry_for_target<'a>(
    entries: &'a [MountInfoEntry],
    target: &str,
) -> Result<Option<&'a MountInfoEntry>> {
    let target_entries = entries
        .iter()
        .filter(|entry| entry.target == target)
        .collect::<Vec<_>>();
    if target_entries.is_empty() {
        return Ok(None);
    }

    let parent_ids = target_entries
        .iter()
        .map(|entry| entry.parent_id)
        .collect::<HashSet<_>>();
    let tops = target_entries
        .into_iter()
        .filter(|entry| !parent_ids.contains(&entry.id))
        .collect::<Vec<_>>();

    if tops.len() != 1 {
        return Err(anyhow!(
            "Ambiguous mount stack for {target}: found {} top candidates",
            tops.len()
        ));
    }
    Ok(tops.into_iter().next())
}

fn module_mount_layers(entries: &[MountInfoEntry]) -> Result<Vec<(String, u32)>> {
    let module_targets = entries
        .iter()
        .filter(|entry| is_module_mount(entry))
        .map(|entry| entry.target.clone())
        .collect::<HashSet<_>>();
    let by_id = entries
        .iter()
        .map(|entry| (entry.id, entry))
        .collect::<HashMap<_, _>>();
    let mut mounts = Vec::new();

    for target in module_targets {
        let Some(mut current) = top_entry_for_target(entries, &target)? else {
            continue;
        };
        let mut layers = 0u32;

        while current.target == target && is_module_mount(current) {
            layers = layers
                .checked_add(1)
                .ok_or_else(|| anyhow!("Too many mount layers for {target}"))?;
            let Some(parent) = by_id.get(&current.parent_id).copied() else {
                break;
            };
            if parent.target != target {
                break;
            }
            current = parent;
        }

        if layers == 0 {
            warn!(
                "Module mount exists below a foreign top layer at {target}; refusing path-based isolation for this target"
            );
            continue;
        }
        mounts.push((target, layers));
    }

    mounts.sort_by(|(a, _), (b, _)| {
        let a_depth = a.bytes().filter(|ch| *ch == b'/').count();
        let b_depth = b.bytes().filter(|ch| *ch == b'/').count();
        a_depth.cmp(&b_depth).then_with(|| a.cmp(b))
    });
    Ok(mounts)
}

#[cfg(target_os = "android")]
fn detach_top_module_mount(target: &str) -> Result<bool> {
    ensure!(target != "/", "Refusing to detach the root mount");
    let entries = read_mountinfo()?;
    let Some(top) = top_entry_for_target(&entries, target)? else {
        return Ok(false);
    };
    if !is_module_mount(top) {
        return Ok(false);
    }

    let target = std::ffi::CString::new(target).context("Module mount path contains NUL")?;
    let ret = unsafe { libc::umount2(target.as_ptr(), libc::MNT_DETACH) };
    if ret != 0 {
        return Err(std::io::Error::last_os_error())
            .context("Failed to detach unregistered module mount");
    }
    Ok(true)
}

fn register_module_mounts_legacy(mounts: &[(String, u32)]) -> Result<()> {
    let mut first_error = None;

    for (mount, _) in mounts {
        match ksucalls::umount_list_add(mount, 0) {
            Ok(()) => {}
            Err(err) if io_errno(&err) == Some(libc::EEXIST) => {
                debug!("Module mount already registered: {mount}");
            }
            Err(err) => {
                warn!("Failed to register module mount {mount}: {err:#}");
                if first_error.is_none() {
                    first_error =
                        Some(err.context(format!("Failed to register module mount {mount}")));
                }
            }
        }
    }

    ksucalls::report_module_mounted();
    if let Some(err) = first_error {
        return Err(err);
    }
    Ok(())
}

pub fn register_module_mounts() -> Result<()> {
    let initial_mounts = module_mount_layers(&read_mountinfo()?)?;

    match ksucalls::umount_list_managed_wipe() {
        Ok(()) => {
            if initial_mounts.is_empty() {
                return Ok(());
            }
        }
        Err(err) if matches!(err.raw_os_error(), Some(libc::EINVAL | libc::ENOTTY)) => {
            if ksucalls::get_info().uapi_version >= 3 {
                return Err(err).context(
                    "Kernel reports UAPI v3+ but managed mount synchronization is unavailable",
                );
            }
            if initial_mounts.is_empty() {
                return Ok(());
            }
            debug!("Managed umount synchronization unavailable; using legacy registration");
            return register_module_mounts_legacy(&initial_mounts);
        }
        Err(err) => return Err(err).context("Failed to reset managed module mount isolation"),
    }

    let mut first_error = None;
    loop {
        let mounts = module_mount_layers(&read_mountinfo()?)?;
        ksucalls::umount_list_managed_wipe()
            .context("Failed to reset managed module mount isolation")?;

        if mounts.is_empty() {
            if let Some(err) = first_error {
                return Err(err);
            }
            return Ok(());
        }

        let mut failed = Vec::new();
        for (mount, layers) in &mounts {
            if let Err(err) = ksucalls::umount_list_managed_set(mount, *layers) {
                warn!("Failed to protect module mount {mount} ({layers} layers): {err:#}");
                if first_error.is_none() {
                    first_error = Some(err.context(format!(
                        "Failed to protect module mount {mount} ({layers} layers)"
                    )));
                }
                failed.push((mount.clone(), *layers));
            }
        }

        if failed.is_empty() {
            ksucalls::report_module_mounted();
            if let Some(err) = first_error {
                return Err(err);
            }
            return Ok(());
        }

        #[cfg(target_os = "android")]
        {
            let mut detached_any = false;
            for (mount, layers) in failed.iter().rev() {
                let mut detached = 0u32;
                while detached < *layers {
                    match detach_top_module_mount(mount) {
                        Ok(true) => {
                            detached += 1;
                            detached_any = true;
                        }
                        Ok(false) => break,
                        Err(err) => {
                            warn!(
                                "Failed to fail-close module mount {mount} after registration failure: {err:#}"
                            );
                            break;
                        }
                    }
                }
            }
            if detached_any {
                continue;
            }
        }

        ksucalls::report_module_mounted();
        return Err(
            first_error.unwrap_or_else(|| anyhow!("Module mount isolation synchronization failed"))
        );
    }
}

/// Check if it's safe to install a regular module
/// Returns Ok(()) if safe, Err(is_disabled) if blocked
/// - Err(true) means metamodule is disabled
/// - Err(false) means metamodule is in other unstable state
pub fn check_install_safety() -> Result<(), bool> {
    // No metamodule → safe
    let Some(metamodule_path) = get_metamodule_path() else {
        return Ok(());
    };

    // No metainstall.sh → safe (uses default installer)
    // The staged update directory may contain the latest scripts, so check both locations
    let has_metainstall = metamodule_path
        .join(defs::METAMODULE_METAINSTALL_SCRIPT)
        .exists()
        || metamodule_path.file_name().is_some_and(|module_id| {
            Path::new(defs::MODULE_UPDATE_DIR)
                .join(module_id)
                .join(defs::METAMODULE_METAINSTALL_SCRIPT)
                .exists()
        });
    if !has_metainstall {
        return Ok(());
    }

    // Check for marker files
    let has_update = metamodule_path.join(defs::UPDATE_FILE_NAME).exists();
    let has_remove = metamodule_path.join(defs::REMOVE_FILE_NAME).exists();
    let has_disable = metamodule_path.join(defs::DISABLE_FILE_NAME).exists();

    // Stable state (no markers) → safe
    if !has_update && !has_remove && !has_disable {
        return Ok(());
    }

    // Return true if disabled, false for other unstable states
    Err(has_disable && !has_update && !has_remove)
}

/// Create or update the metamodule symlink
/// Points /data/adb/metamodule -> /data/adb/modules/{module_id}
pub fn ensure_symlink(module_path: &Path) -> Result<()> {
    // METAMODULE_DIR might have trailing slash, so we need to trim it
    let symlink_path = Path::new(defs::METAMODULE_DIR.trim_end_matches('/'));

    info!(
        "Creating metamodule symlink: {} -> {}",
        symlink_path.display(),
        module_path.display()
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
        .with_context(|| format!("Failed to create symlink to {}", module_path.display()))?;

    info!("Metamodule symlink created successfully");
    Ok(())
}

/// Remove the metamodule symlink
pub fn remove_symlink() -> Result<()> {
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
pub fn get_install_script(
    is_metamodule: bool,
    installer_content: &str,
    install_module_script: &str,
) -> Result<String> {
    // Check if there's a metamodule with metainstall.sh
    // Only apply this logic for regular modules (not when installing metamodule itself)
    let install_script = if is_metamodule {
        info!("Installing metamodule, using default installer");
        install_module_script.to_string()
    } else if let Some(metamodule_path) = get_metamodule_path() {
        if metamodule_path.join(defs::DISABLE_FILE_NAME).exists() {
            info!("Metamodule is disabled, using default installer");
            install_module_script.to_string()
        } else {
            let metainstall_path = metamodule_path.join(defs::METAMODULE_METAINSTALL_SCRIPT);

            if metainstall_path.exists() {
                info!("Using metainstall.sh from metamodule");
                let metamodule_content = std::fs::read_to_string(&metainstall_path)
                    .with_context(|| "Failed to read metamodule metainstall.sh")?;
                format!("{installer_content}\n{metamodule_content}\nexit 0\n")
            } else {
                info!("Metamodule exists but has no metainstall.sh, using default installer");
                install_module_script.to_string()
            }
        }
    } else {
        info!("No metamodule found, using default installer");
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
        info!("Metamodule is disabled, skipping {script_name}");
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
pub fn exec_metauninstall_script(module_id: &str) -> Result<()> {
    let Some(metauninstall_path) = check_metamodule_script(defs::METAMODULE_METAUNINSTALL_SCRIPT)
    else {
        return Ok(());
    };

    info!("Executing metamodule metauninstall.sh for module: {module_id}");

    let result = Command::new(assets::BUSYBOX_PATH)
        .args(["sh", metauninstall_path.to_str().unwrap()])
        .current_dir(metauninstall_path.parent().unwrap())
        .envs(crate::module::get_common_script_envs(
            get_metamodule_id().as_deref(),
        ))
        .env("MODULE_ID", module_id)
        .status()?;

    ensure!(
        result.success(),
        "Metamodule metauninstall.sh failed for module {module_id}: {result:?}"
    );

    info!("Metamodule metauninstall.sh executed successfully for {module_id}");
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
        .envs(crate::module::get_common_script_envs(
            get_metamodule_id().as_deref(),
        ))
        .env("MODULE_DIR", module_dir)
        .status()?;

    // Inspect and register mounts even when the script failed so partial mounts
    // do not silently escape app-namespace isolation. If both the script and
    // registration fail, preserve the registration failure because it carries
    // the safety-critical isolation state.
    let registration = register_module_mounts();

    match (result.success(), registration) {
        (true, Ok(())) => {}
        (true, Err(err)) => return Err(err),
        (false, Ok(())) => {
            ensure!(
                false,
                "Metamodule mount script failed with status: {result:?}"
            );
        }
        (false, Err(err)) => {
            return Err(err).context(format!(
                "Metamodule mount script also failed with status: {result:?}"
            ));
        }
    }

    info!("Metamodule mount script executed successfully");
    Ok(())
}

/// Execute metamodule script for a specific stage
pub fn exec_stage_script(stage: &str, block: bool) -> Result<()> {
    let Some(script_path) = check_metamodule_script(&format!("{stage}.sh")) else {
        return Ok(());
    };

    info!("Executing metamodule {stage}.sh");
    crate::module::exec_script(&script_path, block)?;
    info!("Metamodule {stage}.sh executed successfully");
    Ok(())
}
