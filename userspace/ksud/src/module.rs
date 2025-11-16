#[allow(clippy::wildcard_imports)]
use crate::utils::*;
use crate::{
    assets, defs, ksucalls,
    restorecon::{restore_syscon, setsyscon},
    sepolicy,
};

use anyhow::{Context, Result, anyhow, bail, ensure};
use const_format::concatcp;
use is_executable::is_executable;
use java_properties::PropertiesIter;
use log::{info, warn};

use std::{
    collections::HashMap,
    env::var as env_var,
    fs::{File, Permissions, remove_dir_all, set_permissions},
    io::Cursor,
    path::{Path, PathBuf},
    process::Command,
    str::FromStr,
};
use zip_extensions::zip_extract_file_to_memory;

#[cfg(unix)]
use std::os::unix::{prelude::PermissionsExt, process::CommandExt};

const INSTALLER_CONTENT: &str = include_str!("./installer.sh");
const INSTALL_MODULE_SCRIPT: &str = concatcp!(
    INSTALLER_CONTENT,
    "\n",
    "install_module",
    "\n",
    "exit 0",
    "\n"
);

fn exec_install_script(module_file: &str, is_metamodule: bool) -> Result<()> {
    let realpath = std::fs::canonicalize(module_file)
        .with_context(|| format!("realpath: {module_file} failed"))?;

    // Check if there's a metamodule with metainstall.sh
    // Only apply this logic for regular modules (not when installing metamodule itself)
    let install_script = if !is_metamodule {
        if let Some(metamodule_path) = get_metamodule_path() {
            if metamodule_path.join(defs::DISABLE_FILE_NAME).exists() {
                info!("Metamodule is disabled, using default installer");
                INSTALL_MODULE_SCRIPT.to_string()
            } else {
                let metainstall_path = metamodule_path.join(defs::METAMODULE_METAINSTALL_SCRIPT);

                if metainstall_path.exists() {
                    info!("Using metainstall.sh from metamodule");
                    let metamodule_content = std::fs::read_to_string(&metainstall_path)
                        .with_context(|| "Failed to read metamodule metainstall.sh")?;
                    format!("{}\n{}\nexit 0\n", INSTALLER_CONTENT, metamodule_content)
                } else {
                    info!("Metamodule exists but has no metainstall.sh, using default installer");
                    INSTALL_MODULE_SCRIPT.to_string()
                }
            }
        } else {
            info!("No metamodule found, using default installer");
            INSTALL_MODULE_SCRIPT.to_string()
        }
    } else {
        info!("Installing metamodule, using default installer");
        INSTALL_MODULE_SCRIPT.to_string()
    };

    let result = Command::new(assets::BUSYBOX_PATH)
        .args(["sh", "-c", &install_script])
        .env("ASH_STANDALONE", "1")
        .env(
            "PATH",
            format!(
                "{}:{}",
                env_var("PATH").unwrap(),
                defs::BINARY_DIR.trim_end_matches('/')
            ),
        )
        .env("KSU", "true")
        .env("KSU_KERNEL_VER_CODE", ksucalls::get_version().to_string())
        .env("KSU_VER", defs::VERSION_NAME)
        .env("KSU_VER_CODE", defs::VERSION_CODE)
        .env("OUTFD", "1")
        .env("ZIPFILE", realpath)
        .status()?;
    ensure!(result.success(), "Failed to install module script");
    Ok(())
}

// Check if Android boot is completed before installing modules
fn ensure_boot_completed() -> Result<()> {
    // ensure getprop sys.boot_completed == 1
    if getprop("sys.boot_completed").as_deref() != Some("1") {
        bail!("Android is Booting!");
    }
    Ok(())
}

/// Get metamodule path if it exists
pub fn get_metamodule_path() -> Option<PathBuf> {
    let path = Path::new(defs::METAMODULE_DIR);
    if path.exists() && path.is_dir() {
        Some(path.to_path_buf())
    } else {
        None
    }
}

/// Check if metamodule exists
pub fn has_metamodule() -> bool {
    get_metamodule_path().is_some()
}

/// Get metamodule's module.prop info
pub fn get_metamodule_info() -> Result<Option<HashMap<String, String>>> {
    let Some(metamodule_path) = get_metamodule_path() else {
        return Ok(None);
    };

    let module_prop = metamodule_path.join("module.prop");
    if !module_prop.exists() {
        bail!("Metamodule exists but has no module.prop");
    }

    let content = std::fs::read(&module_prop)?;
    let mut props = HashMap::new();
    PropertiesIter::new_with_encoding(Cursor::new(content), encoding_rs::UTF_8).read_into(
        |k, v| {
            props.insert(k, v);
        },
    )?;

    Ok(Some(props))
}

/// Uninstall metamodule
pub fn uninstall_metamodule() -> Result<()> {
    ensure_boot_completed()?;

    let Some(metamodule_path) = get_metamodule_path() else {
        bail!("No metamodule installed");
    };

    println!("- Uninstalling metamodule");

    // Execute uninstall script if exists
    let uninstall_script = metamodule_path.join("uninstall.sh");
    if uninstall_script.exists() {
        info!("Executing metamodule uninstall script");
        if let Err(e) = exec_script(&uninstall_script, true) {
            warn!("Metamodule uninstall script failed: {}", e);
        }
    }

    // Remove the entire directory
    remove_dir_all(&metamodule_path).with_context(|| "Failed to remove metamodule directory")?;

    println!("- Metamodule uninstalled successfully");
    Ok(())
}

fn foreach_module(active_only: bool, mut f: impl FnMut(&Path) -> Result<()>) -> Result<()> {
    let modules_dir = Path::new(defs::MODULE_DIR);
    let dir = std::fs::read_dir(modules_dir)?;
    for entry in dir.flatten() {
        let path = entry.path();
        if !path.is_dir() {
            warn!("{} is not a directory, skip", path.display());
            continue;
        }

        if active_only && path.join(defs::DISABLE_FILE_NAME).exists() {
            info!("{} is disabled, skip", path.display());
            continue;
        }
        if active_only && path.join(defs::REMOVE_FILE_NAME).exists() {
            warn!("{} is removed, skip", path.display());
            continue;
        }

        f(&path)?;
    }

    Ok(())
}

fn foreach_active_module(f: impl FnMut(&Path) -> Result<()>) -> Result<()> {
    foreach_module(true, f)
}

pub fn load_sepolicy_rule() -> Result<()> {
    foreach_active_module(|path| {
        let rule_file = path.join("sepolicy.rule");
        if !rule_file.exists() {
            return Ok(());
        }
        info!("load policy: {}", &rule_file.display());

        if sepolicy::apply_file(&rule_file).is_err() {
            warn!("Failed to load sepolicy.rule for {}", &rule_file.display());
        }
        Ok(())
    })?;

    Ok(())
}

pub fn exec_script<T: AsRef<Path>>(path: T, wait: bool) -> Result<()> {
    info!("exec {}", path.as_ref().display());

    let mut command = &mut Command::new(assets::BUSYBOX_PATH);
    #[cfg(unix)]
    {
        command = command.process_group(0);
        command = unsafe {
            command.pre_exec(|| {
                // ignore the error?
                switch_cgroups();
                Ok(())
            })
        };
    }
    command = command
        .current_dir(path.as_ref().parent().unwrap())
        .arg("sh")
        .arg(path.as_ref())
        .env("ASH_STANDALONE", "1")
        .env("KSU", "true")
        .env("KSU_KERNEL_VER_CODE", ksucalls::get_version().to_string())
        .env("KSU_VER_CODE", defs::VERSION_CODE)
        .env("KSU_VER", defs::VERSION_NAME)
        .env(
            "PATH",
            format!(
                "{}:{}",
                env_var("PATH").unwrap(),
                defs::BINARY_DIR.trim_end_matches('/')
            ),
        );

    let result = if wait {
        command.status().map(|_| ())
    } else {
        command.spawn().map(|_| ())
    };
    result.map_err(|err| anyhow!("Failed to exec {}: {}", path.as_ref().display(), err))
}

pub fn exec_stage_script(stage: &str, block: bool) -> Result<()> {
    foreach_active_module(|module| {
        let script_path = module.join(format!("{stage}.sh"));
        if !script_path.exists() {
            return Ok(());
        }

        exec_script(&script_path, block)
    })?;

    Ok(())
}

pub fn exec_common_scripts(dir: &str, wait: bool) -> Result<()> {
    let script_dir = Path::new(defs::ADB_DIR).join(dir);
    if !script_dir.exists() {
        info!("{} not exists, skip", script_dir.display());
        return Ok(());
    }

    let dir = std::fs::read_dir(&script_dir)?;
    for entry in dir.flatten() {
        let path = entry.path();

        if !is_executable(&path) {
            warn!("{} is not executable, skip", path.display());
            continue;
        }

        exec_script(path, wait)?;
    }

    Ok(())
}

pub fn load_system_prop() -> Result<()> {
    foreach_active_module(|module| {
        let system_prop = module.join("system.prop");
        if !system_prop.exists() {
            return Ok(());
        }
        info!("load {} system.prop", module.display());

        // resetprop -n --file system.prop
        Command::new(assets::RESETPROP_PATH)
            .arg("-n")
            .arg("--file")
            .arg(&system_prop)
            .status()
            .with_context(|| format!("Failed to exec {}", system_prop.display()))?;

        Ok(())
    })?;

    Ok(())
}

pub fn prune_modules() -> Result<()> {
    foreach_module(false, |module| {
        if !module.join(defs::REMOVE_FILE_NAME).exists() {
            return Ok(());
        }

        info!("remove module: {}", module.display());

        let uninstaller = module.join("uninstall.sh");
        if uninstaller.exists()
            && let Err(e) = exec_script(uninstaller, true)
        {
            warn!("Failed to exec uninstaller: {e}");
        }

        if let Err(e) = remove_dir_all(module) {
            warn!("Failed to remove {}: {}", module.display(), e);
        }

        Ok(())
    })?;

    // collect remaining modules, if none, clean up metamodule record
    let remaining_modules: Vec<_> = std::fs::read_dir(defs::MODULE_DIR)?
        .filter_map(|entry| entry.ok())
        .filter(|entry| entry.path().join("module.prop").exists())
        .collect();

    if remaining_modules.is_empty() {
        info!("no remaining modules.");
    }

    Ok(())
}

fn _install_module(zip: &str) -> Result<()> {
    ensure_boot_completed()?;

    // print banner
    println!(include_str!("banner"));

    assets::ensure_binaries(false).with_context(|| "Failed to extract assets")?;

    // first check if working dir is usable
    ensure_dir_exists(defs::WORKING_DIR).with_context(|| "Failed to create working dir")?;
    ensure_dir_exists(defs::BINARY_DIR).with_context(|| "Failed to create bin dir")?;

    // read the module_id from zip, if failed it will return early.
    let mut buffer: Vec<u8> = Vec::new();
    let entry_path = PathBuf::from_str("module.prop")?;
    let zip_path = PathBuf::from_str(zip)?;
    let zip_path = zip_path.canonicalize()?;
    zip_extract_file_to_memory(&zip_path, &entry_path, &mut buffer)?;

    let mut module_prop = HashMap::new();
    PropertiesIter::new_with_encoding(Cursor::new(buffer), encoding_rs::UTF_8).read_into(
        |k, v| {
            module_prop.insert(k, v);
        },
    )?;
    info!("module prop: {module_prop:?}");

    let Some(module_id) = module_prop.get("id") else {
        bail!("module id not found in module.prop!");
    };
    let module_id = module_id.trim();

    // Check if this module is a metamodule
    let is_metamodule = module_prop
        .get("metamodule")
        .map(|s| s.trim().to_lowercase())
        .map(|s| s == "true" || s == "1")
        .unwrap_or(false);

    // Determine target directory based on module type
    let target_dir = if is_metamodule {
        info!("Installing metamodule: {}", module_id);

        // Check if there's already a metamodule installed
        if has_metamodule() {
            let existing = get_metamodule_info()?
                .and_then(|m| m.get("id").cloned())
                .unwrap_or_else(|| "unknown".to_string());

            bail!(
                "A metamodule is already installed: {}\n\
                 Please uninstall it first using: ksud module uninstall {}",
                existing,
                existing
            );
        }

        Path::new(defs::METAMODULE_DIR).to_path_buf()
    } else {
        Path::new(defs::MODULE_DIR).join(module_id)
    };

    let zip_uncompressed_size = get_zip_uncompressed_size(zip)?;
    info!(
        "zip uncompressed size: {}",
        humansize::format_size(zip_uncompressed_size, humansize::DECIMAL)
    );
    println!(
        "- Module size: {}",
        humansize::format_size(zip_uncompressed_size, humansize::DECIMAL)
    );

    // Ensure parent directory exists and set SELinux context
    if is_metamodule {
        ensure_dir_exists(defs::METAMODULE_DIR)?;
        setsyscon(defs::METAMODULE_DIR)?;
    } else {
        ensure_dir_exists(defs::MODULE_DIR)?;
        setsyscon(defs::MODULE_DIR)?;
    }

    // Prepare target directory
    println!("- Installing to {}", target_dir.display());
    ensure_clean_dir(&target_dir)?;
    info!("target dir: {}", target_dir.display());

    // Extract zip to target directory
    println!("- Extracting module files");
    let file = File::open(zip)?;
    let mut archive = zip::ZipArchive::new(file)?;
    archive.extract(&target_dir)?;

    // Set permission and selinux context for $MOD/system
    let module_system_dir = target_dir.join("system");
    if module_system_dir.exists() {
        #[cfg(unix)]
        set_permissions(&module_system_dir, Permissions::from_mode(0o755))?;
        restore_syscon(&module_system_dir)?;
    }

    // Execute install script
    println!("- Running module installer");
    exec_install_script(zip, is_metamodule)?;

    println!("- Module installed successfully!");
    info!("Module {} installed successfully!", module_id);

    Ok(())
}

pub fn install_module(zip: &str) -> Result<()> {
    let result = _install_module(zip);
    if let Err(ref e) = result {
        println!("- Error: {e}");
    }
    result
}

pub fn uninstall_module(id: &str) -> Result<()> {
    // First check if trying to uninstall the metamodule
    if has_metamodule()
        && let Ok(Some(metamodule_info)) = get_metamodule_info()
        && let Some(metamodule_id) = metamodule_info.get("id")
        && metamodule_id.trim() == id
    {
        // Uninstall metamodule directly using uninstall_metamodule()
        return uninstall_metamodule();
    }

    // Regular module uninstall - mark for removal
    let module_path = Path::new(defs::MODULE_DIR).join(id);
    ensure!(module_path.exists(), "Module {} not found", id);

    // Mark for removal
    let remove_file = module_path.join(defs::REMOVE_FILE_NAME);
    File::create(remove_file).with_context(|| "Failed to create remove file")?;

    info!("Module {} marked for removal", id);

    Ok(())
}

pub fn run_action(id: &str) -> Result<()> {
    let action_script_path = format!("/data/adb/modules/{id}/action.sh");
    exec_script(&action_script_path, true)
}

pub fn enable_module(id: &str) -> Result<()> {
    // Check if enabling the metamodule
    if has_metamodule()
        && let Ok(Some(metamodule_info)) = get_metamodule_info()
        && let Some(metamodule_id) = metamodule_info.get("id")
        && metamodule_id.trim() == id
    {
        info!("Enabling metamodule: {}", id);
        let metamodule_path = Path::new(defs::METAMODULE_DIR);
        let disable_path = metamodule_path.join(defs::DISABLE_FILE_NAME);
        if disable_path.exists() {
            std::fs::remove_file(&disable_path).with_context(|| {
                format!("Failed to remove disable file: {}", disable_path.display())
            })?;
            info!("Metamodule {} enabled", id);
        }
        return Ok(());
    }

    // Regular module
    let module_path = Path::new(defs::MODULE_DIR).join(id);
    ensure!(module_path.exists(), "Module {} not found", id);

    let disable_path = module_path.join(defs::DISABLE_FILE_NAME);
    if disable_path.exists() {
        std::fs::remove_file(&disable_path).with_context(|| {
            format!("Failed to remove disable file: {}", disable_path.display())
        })?;
        info!("Module {} enabled", id);
    }

    Ok(())
}

pub fn disable_module(id: &str) -> Result<()> {
    // Check if disabling the metamodule
    if has_metamodule()
        && let Ok(Some(metamodule_info)) = get_metamodule_info()
        && let Some(metamodule_id) = metamodule_info.get("id")
        && metamodule_id.trim() == id
    {
        warn!(
            "Disabling metamodule: {}. Module mounting will not work until re-enabled.",
            id
        );
        let metamodule_path = Path::new(defs::METAMODULE_DIR);
        let disable_path = metamodule_path.join(defs::DISABLE_FILE_NAME);
        ensure_file_exists(disable_path)?;
        info!("Metamodule {} disabled", id);
        return Ok(());
    }

    // Regular module
    let module_path = Path::new(defs::MODULE_DIR).join(id);
    ensure!(module_path.exists(), "Module {} not found", id);

    let disable_path = module_path.join(defs::DISABLE_FILE_NAME);
    ensure_file_exists(disable_path)?;

    info!("Module {} disabled", id);

    Ok(())
}

pub fn disable_all_modules() -> Result<()> {
    // Disable metamodule if exists
    if has_metamodule() {
        if let Ok(Some(metamodule_info)) = get_metamodule_info()
            && let Some(metamodule_id) = metamodule_info.get("id")
        {
            warn!(
                "Disabling all modules including metamodule: {}. Module mounting will not work.",
                metamodule_id
            );
        }
        let metamodule_path = Path::new(defs::METAMODULE_DIR);
        let disable_path = metamodule_path.join(defs::DISABLE_FILE_NAME);
        if let Err(e) = ensure_file_exists(disable_path) {
            warn!("Failed to disable metamodule: {}", e);
        }
    }

    mark_all_modules(defs::DISABLE_FILE_NAME)
}

pub fn uninstall_all_modules() -> Result<()> {
    info!("Uninstalling all modules");

    // Uninstall metamodule if exists
    if has_metamodule() {
        info!("Uninstalling metamodule");
        if let Err(e) = uninstall_metamodule() {
            warn!("Failed to uninstall metamodule: {}", e);
        }
    }

    mark_all_modules(defs::REMOVE_FILE_NAME)
}

fn mark_all_modules(flag_file: &str) -> Result<()> {
    // we assume the module dir is already mounted
    let dir = std::fs::read_dir(defs::MODULE_DIR)?;
    for entry in dir.flatten() {
        let path = entry.path();
        let flag = path.join(flag_file);
        if let Err(e) = ensure_file_exists(flag) {
            warn!("Failed to mark module: {}: {}", path.display(), e);
        }
    }

    Ok(())
}

/// Read module.prop from the given module path and return as a HashMap
pub fn read_module_prop(module_path: &Path) -> Result<HashMap<String, String>> {
    let module_prop = module_path.join("module.prop");
    ensure!(
        module_prop.exists(),
        "module.prop not found in {}",
        module_path.display()
    );

    let content = std::fs::read(&module_prop)
        .with_context(|| format!("Failed to read module.prop: {}", module_prop.display()))?;

    let mut prop_map: HashMap<String, String> = HashMap::new();
    PropertiesIter::new_with_encoding(Cursor::new(content), encoding_rs::UTF_8)
        .read_into(|k, v| {
            prop_map.insert(k, v);
        })
        .with_context(|| format!("Failed to parse module.prop: {}", module_prop.display()))?;

    Ok(prop_map)
}

fn _list_modules(path: &str) -> Vec<HashMap<String, String>> {
    // first check enabled modules
    let dir = std::fs::read_dir(path);
    let Ok(dir) = dir else {
        return Vec::new();
    };

    let mut modules: Vec<HashMap<String, String>> = Vec::new();

    for entry in dir.flatten() {
        let path = entry.path();
        info!("path: {}", path.display());

        if !path.join("module.prop").exists() {
            continue;
        }

        let mut module_prop_map = match read_module_prop(&path) {
            Ok(prop) => prop,
            Err(e) => {
                warn!("Failed to read module.prop for {}: {}", path.display(), e);
                continue;
            }
        };

        // If id is missing or empty, use directory name as fallback
        if !module_prop_map.contains_key("id") || module_prop_map["id"].is_empty() {
            match entry.file_name().to_str() {
                Some(id) => {
                    info!("Use dir name as module id: {id}");
                    module_prop_map.insert("id".to_owned(), id.to_owned());
                }
                _ => {
                    info!("Failed to get module id from dir name");
                    continue;
                }
            }
        }

        // Add enabled, remove, web, action flags
        let enabled = !path.join(defs::DISABLE_FILE_NAME).exists();
        let remove = path.join(defs::REMOVE_FILE_NAME).exists();
        let web = path.join(defs::MODULE_WEB_DIR).exists();
        let action = path.join(defs::MODULE_ACTION_SH).exists();

        module_prop_map.insert("enabled".to_owned(), enabled.to_string());
        module_prop_map.insert("remove".to_owned(), remove.to_string());
        module_prop_map.insert("web".to_owned(), web.to_string());
        module_prop_map.insert("action".to_owned(), action.to_string());

        modules.push(module_prop_map);
    }

    modules
}

pub fn list_modules() -> Result<()> {
    let mut modules = _list_modules(defs::MODULE_DIR);

    // Add metamodule if exists
    if let Ok(Some(mut metamodule_info)) = get_metamodule_info() {
        let metamodule_path = Path::new(defs::METAMODULE_DIR);

        // Add enabled, web, action flags
        let enabled = !metamodule_path.join(defs::DISABLE_FILE_NAME).exists();
        let web = metamodule_path.join(defs::MODULE_WEB_DIR).exists();
        let action = metamodule_path.join(defs::MODULE_ACTION_SH).exists();

        metamodule_info.insert("enabled".to_owned(), enabled.to_string());
        metamodule_info.insert("remove".to_owned(), "false".to_string());
        metamodule_info.insert("web".to_owned(), web.to_string());
        metamodule_info.insert("action".to_owned(), action.to_string());
        metamodule_info.insert("metamodule".to_owned(), "true".to_string());

        // Insert metamodule at the beginning of the list
        modules.insert(0, metamodule_info);
    }

    println!("{}", serde_json::to_string_pretty(&modules)?);
    Ok(())
}

/// Get all managed features from active modules
/// Modules can specify managedFeatures in their module.prop
/// Format: managedFeatures=feature1,feature2,feature3
/// Returns: HashMap<ModuleId, Vec<ManagedFeature>>
pub fn get_managed_features() -> Result<HashMap<String, Vec<String>>> {
    let mut managed_features_map: HashMap<String, Vec<String>> = HashMap::new();

    foreach_active_module(|module_path| {
        let prop_map = match read_module_prop(module_path) {
            Ok(prop) => prop,
            Err(e) => {
                warn!(
                    "Failed to read module.prop for {}: {}",
                    module_path.display(),
                    e
                );
                return Ok(());
            }
        };

        if let Some(features_str) = prop_map.get("managedFeatures") {
            let module_id = prop_map
                .get("id")
                .map(|s| s.to_string())
                .unwrap_or_else(|| "unknown".to_string());

            info!("Module {} manages features: {}", module_id, features_str);

            let mut feature_list = Vec::new();
            for feature in features_str.split(',') {
                let feature = feature.trim();
                if !feature.is_empty() {
                    info!("  - Adding managed feature: {}", feature);
                    feature_list.push(feature.to_string());
                }
            }

            if !feature_list.is_empty() {
                managed_features_map.insert(module_id, feature_list);
            }
        }

        Ok(())
    })?;

    Ok(managed_features_map)
}
