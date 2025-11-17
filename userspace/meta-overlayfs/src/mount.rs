// Overlayfs mounting implementation
// Migrated from ksud/src/mount.rs and ksud/src/init_event.rs

use anyhow::{Context, Result, bail};
use log::{info, warn};
use std::collections::HashMap;
use std::path::{Path, PathBuf};

#[cfg(any(target_os = "linux", target_os = "android"))]
use procfs::process::Process;
#[cfg(any(target_os = "linux", target_os = "android"))]
use rustix::{fd::AsFd, fs::CWD, mount::*};

use crate::defs::{DISABLE_FILE_NAME, KSU_OVERLAY_SOURCE, SKIP_MOUNT_FILE_NAME, SYSTEM_RW_DIR};

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn mount_overlayfs(
    lower_dirs: &[String],
    lowest: &str,
    upperdir: Option<PathBuf>,
    workdir: Option<PathBuf>,
    dest: impl AsRef<Path>,
) -> Result<()> {
    let lowerdir_config = lower_dirs
        .iter()
        .map(|s| s.as_ref())
        .chain(std::iter::once(lowest))
        .collect::<Vec<_>>()
        .join(":");
    info!(
        "mount overlayfs on {:?}, lowerdir={}, upperdir={:?}, workdir={:?}",
        dest.as_ref(),
        lowerdir_config,
        upperdir,
        workdir
    );

    let upperdir = upperdir
        .filter(|up| up.exists())
        .map(|e| e.display().to_string());
    let workdir = workdir
        .filter(|wd| wd.exists())
        .map(|e| e.display().to_string());

    let result = (|| {
        let fs = fsopen("overlay", FsOpenFlags::FSOPEN_CLOEXEC)?;
        let fs = fs.as_fd();
        fsconfig_set_string(fs, "lowerdir", &lowerdir_config)?;
        if let (Some(upperdir), Some(workdir)) = (&upperdir, &workdir) {
            fsconfig_set_string(fs, "upperdir", upperdir)?;
            fsconfig_set_string(fs, "workdir", workdir)?;
        }
        fsconfig_set_string(fs, "source", KSU_OVERLAY_SOURCE)?;
        fsconfig_create(fs)?;
        let mount = fsmount(fs, FsMountFlags::FSMOUNT_CLOEXEC, MountAttrFlags::empty())?;
        move_mount(
            mount.as_fd(),
            "",
            CWD,
            dest.as_ref(),
            MoveMountFlags::MOVE_MOUNT_F_EMPTY_PATH,
        )
    })();

    if let Err(e) = result {
        warn!("fsopen mount failed: {e:#}, fallback to mount");
        let mut data = format!("lowerdir={lowerdir_config}");
        if let (Some(upperdir), Some(workdir)) = (upperdir, workdir) {
            data = format!("{data},upperdir={upperdir},workdir={workdir}");
        }
        mount(
            KSU_OVERLAY_SOURCE,
            dest.as_ref(),
            "overlay",
            MountFlags::empty(),
            data,
        )?;
    }
    Ok(())
}

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn bind_mount(from: impl AsRef<Path>, to: impl AsRef<Path>) -> Result<()> {
    info!(
        "bind mount {} -> {}",
        from.as_ref().display(),
        to.as_ref().display()
    );
    let tree = open_tree(
        CWD,
        from.as_ref(),
        OpenTreeFlags::OPEN_TREE_CLOEXEC
            | OpenTreeFlags::OPEN_TREE_CLONE
            | OpenTreeFlags::AT_RECURSIVE,
    )?;
    move_mount(
        tree.as_fd(),
        "",
        CWD,
        to.as_ref(),
        MoveMountFlags::MOVE_MOUNT_F_EMPTY_PATH,
    )?;
    Ok(())
}

#[cfg(any(target_os = "linux", target_os = "android"))]
fn mount_overlay_child(
    mount_point: &str,
    relative: &String,
    module_roots: &Vec<String>,
    stock_root: &String,
) -> Result<()> {
    if !module_roots
        .iter()
        .any(|lower| Path::new(&format!("{lower}{relative}")).exists())
    {
        return bind_mount(stock_root, mount_point);
    }
    if !Path::new(&stock_root).is_dir() {
        return Ok(());
    }
    let mut lower_dirs: Vec<String> = vec![];
    for lower in module_roots {
        let lower_dir = format!("{lower}{relative}");
        let path = Path::new(&lower_dir);
        if path.is_dir() {
            lower_dirs.push(lower_dir);
        } else if path.exists() {
            // stock root has been blocked by this file
            return Ok(());
        }
    }
    if lower_dirs.is_empty() {
        return Ok(());
    }
    // merge modules and stock
    if let Err(e) = mount_overlayfs(&lower_dirs, stock_root, None, None, mount_point) {
        warn!("failed: {e:#}, fallback to bind mount");
        bind_mount(stock_root, mount_point)?;
    }
    Ok(())
}

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn mount_overlay(
    root: &String,
    module_roots: &Vec<String>,
    workdir: Option<PathBuf>,
    upperdir: Option<PathBuf>,
) -> Result<()> {
    info!("mount overlay for {root}");
    std::env::set_current_dir(root).with_context(|| format!("failed to chdir to {root}"))?;
    let stock_root = ".";

    // collect child mounts before mounting the root
    let mounts = Process::myself()?
        .mountinfo()
        .with_context(|| "get mountinfo")?;
    let mut mount_seq = mounts
        .0
        .iter()
        .filter(|m| {
            m.mount_point.starts_with(root) && !Path::new(&root).starts_with(&m.mount_point)
        })
        .map(|m| m.mount_point.to_str())
        .collect::<Vec<_>>();
    mount_seq.sort();
    mount_seq.dedup();

    mount_overlayfs(module_roots, root, upperdir, workdir, root)
        .with_context(|| "mount overlayfs for root failed")?;
    for mount_point in mount_seq.iter() {
        let Some(mount_point) = mount_point else {
            continue;
        };
        let relative = mount_point.replacen(root, "", 1);
        let stock_root: String = format!("{stock_root}{relative}");
        if !Path::new(&stock_root).exists() {
            continue;
        }
        if let Err(e) = mount_overlay_child(mount_point, &relative, module_roots, &stock_root) {
            warn!("failed to mount overlay for child {mount_point}: {e:#}, revert");
            umount_dir(root).with_context(|| format!("failed to revert {root}"))?;
            bail!(e);
        }
    }
    Ok(())
}

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn umount_dir(src: impl AsRef<Path>) -> Result<()> {
    unmount(src.as_ref(), UnmountFlags::empty())
        .with_context(|| format!("Failed to umount {}", src.as_ref().display()))?;
    Ok(())
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
pub fn mount_overlay(
    _root: &String,
    _module_roots: &Vec<String>,
    _workdir: Option<PathBuf>,
    _upperdir: Option<PathBuf>,
) -> Result<()> {
    unimplemented!("mount_overlay is only supported on Linux/Android")
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
pub fn mount_overlayfs(
    _lower_dirs: &[String],
    _lowest: &str,
    _upperdir: Option<PathBuf>,
    _workdir: Option<PathBuf>,
    _dest: impl AsRef<Path>,
) -> Result<()> {
    unimplemented!("mount_overlayfs is only supported on Linux/Android")
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
pub fn bind_mount(_from: impl AsRef<Path>, _to: impl AsRef<Path>) -> Result<()> {
    unimplemented!("bind_mount is only supported on Linux/Android")
}

// ========== Mount coordination logic (from init_event.rs) ==========

#[cfg(any(target_os = "linux", target_os = "android"))]
fn mount_partition(partition_name: &str, lowerdir: &Vec<String>) -> Result<()> {
    if lowerdir.is_empty() {
        warn!("partition: {partition_name} lowerdir is empty");
        return Ok(());
    }

    let partition = format!("/{partition_name}");

    // if /partition is a symlink and linked to /system/partition, then we don't need to overlay it separately
    if Path::new(&partition).read_link().is_ok() {
        warn!("partition: {partition} is a symlink");
        return Ok(());
    }

    let mut workdir = None;
    let mut upperdir = None;
    let system_rw_dir = Path::new(SYSTEM_RW_DIR);
    if system_rw_dir.exists() {
        workdir = Some(system_rw_dir.join(partition_name).join("workdir"));
        upperdir = Some(system_rw_dir.join(partition_name).join("upperdir"));
    }

    mount_overlay(&partition, lowerdir, workdir, upperdir)
}

/// Collect enabled module IDs from metadata directory
///
/// Reads module list and status from metadata directory, returns enabled module IDs
#[cfg(any(target_os = "linux", target_os = "android"))]
fn collect_enabled_modules(metadata_dir: &str) -> Result<Vec<String>> {
    let dir = std::fs::read_dir(metadata_dir)
        .with_context(|| format!("Failed to read metadata directory: {}", metadata_dir))?;

    let mut enabled = Vec::new();

    for entry in dir.flatten() {
        let path = entry.path();
        if !path.is_dir() {
            continue;
        }

        let module_id = match entry.file_name().to_str() {
            Some(id) => id.to_string(),
            None => continue,
        };

        // Check status markers
        if path.join(DISABLE_FILE_NAME).exists() {
            info!("Module {} is disabled, skipping", module_id);
            continue;
        }

        if path.join(SKIP_MOUNT_FILE_NAME).exists() {
            info!("Module {} has skip_mount, skipping", module_id);
            continue;
        }

        // Optional: verify module.prop exists
        if !path.join("module.prop").exists() {
            warn!("Module {} has no module.prop, skipping", module_id);
            continue;
        }

        info!("Module {} enabled", module_id);
        enabled.push(module_id);
    }

    Ok(enabled)
}

/// Dual-directory version of mount_modules_systemlessly
///
/// Parameters:
/// - metadata_dir: Metadata directory, stores module.prop, disable, skip_mount, etc.
/// - content_dir: Content directory, stores system/, vendor/ and other partition content (ext4 image mount point)
#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn mount_modules_systemlessly(metadata_dir: &str, content_dir: &str) -> Result<()> {
    info!("Scanning modules (dual-directory mode)");
    info!("  Metadata: {}", metadata_dir);
    info!("  Content: {}", content_dir);

    // 1. Traverse metadata directory, collect enabled module IDs
    let enabled_modules = collect_enabled_modules(metadata_dir)?;

    if enabled_modules.is_empty() {
        info!("No enabled modules found");
        return Ok(());
    }

    info!("Found {} enabled module(s)", enabled_modules.len());

    // 2. Initialize partition lowerdir lists
    let partition = vec!["vendor", "product", "system_ext", "odm", "oem"];
    let mut system_lowerdir: Vec<String> = Vec::new();
    let mut partition_lowerdir: HashMap<String, Vec<String>> = HashMap::new();

    for part in &partition {
        partition_lowerdir.insert((*part).to_string(), Vec::new());
    }

    // 3. Read module content from content directory
    for module_id in &enabled_modules {
        let module_content_path = Path::new(content_dir).join(module_id);

        if !module_content_path.exists() {
            warn!("Module {} has no content directory, skipping", module_id);
            continue;
        }

        info!("Processing module: {}", module_id);

        // Collect system partition
        let system_path = module_content_path.join("system");
        if system_path.is_dir() {
            system_lowerdir.push(system_path.display().to_string());
            info!("  + system/");
        }

        // Collect other partitions
        for part in &partition {
            let part_path = module_content_path.join(part);
            if part_path.is_dir()
                && let Some(v) = partition_lowerdir.get_mut(*part)
            {
                v.push(part_path.display().to_string());
                info!("  + {}/", part);
            }
        }
    }

    // 4. Mount partitions
    info!("Mounting partitions...");

    if let Err(e) = mount_partition("system", &system_lowerdir) {
        warn!("mount system failed: {e:#}");
    }

    for (k, v) in partition_lowerdir {
        if let Err(e) = mount_partition(&k, &v) {
            warn!("mount {k} failed: {e:#}");
        }
    }

    info!("All partitions processed");
    Ok(())
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
pub fn mount_modules_systemlessly(_metadata_dir: &str, _content_dir: &str) -> Result<()> {
    unimplemented!("mount_modules_systemlessly is only supported on Linux/Android")
}
