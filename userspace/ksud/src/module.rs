#[allow(clippy::wildcard_imports)]
use crate::utils::*;
use crate::{
    assets, defs, ksucalls,
    restorecon::{restore_syscon, setsyscon},
    sepolicy,
};

use anyhow::{anyhow, bail, ensure, Context, Result};
use const_format::concatcp;
use is_executable::is_executable;
use java_properties::PropertiesIter;
use log::{info, warn};

use std::fs::{copy, rename};
use std::{
    collections::HashMap,
    env::var as env_var,
    fs::{remove_dir_all, remove_file, set_permissions, File, Permissions},
    io::Cursor,
    path::{Path, PathBuf},
    process::Command,
    str::FromStr,
};
use zip_extensions::zip_extract_file_to_memory;

use crate::defs::{MODULE_DIR, MODULE_UPDATE_DIR, UPDATE_FILE_NAME};
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

fn exec_install_script(module_file: &str) -> Result<()> {
    let realpath = std::fs::canonicalize(module_file)
        .with_context(|| format!("realpath: {module_file} failed"))?;

    let result = Command::new(assets::BUSYBOX_PATH)
        .args(["sh", "-c", INSTALL_MODULE_SCRIPT])
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

// becuase we use something like A-B update
// we need to update the module state after the boot_completed
// if someone(such as the module) install a module before the boot_completed
// then it may cause some problems, just forbid it
fn ensure_boot_completed() -> Result<()> {
    // ensure getprop sys.boot_completed == 1
    if getprop("sys.boot_completed").as_deref() != Some("1") {
        bail!("Android is Booting!");
    }
    Ok(())
}

fn mark_module_state(module: &str, flag_file: &str, create: bool) -> Result<()> {
    let module_state_file = Path::new(MODULE_DIR).join(module).join(flag_file);
    if create {
        ensure_file_exists(module_state_file)
    } else {
        if module_state_file.exists() {
            remove_file(module_state_file)?;
        }
        Ok(())
    }
}

#[derive(PartialEq, Eq)]
enum ModuleType {
    All,
    Active,
    Updated,
}

fn foreach_module(module_type: ModuleType, mut f: impl FnMut(&Path) -> Result<()>) -> Result<()> {
    let modules_dir = Path::new(match module_type {
        ModuleType::Updated => MODULE_UPDATE_DIR,
        _ => defs::MODULE_DIR,
    });
    if !modules_dir.is_dir() {
        warn!("{} is not a directory, skip", modules_dir.display());
        return Ok(());
    }
    let dir = std::fs::read_dir(modules_dir)?;
    for entry in dir.flatten() {
        let path = entry.path();
        if !path.is_dir() {
            warn!("{} is not a directory, skip", path.display());
            continue;
        }

        if module_type == ModuleType::Active && path.join(defs::DISABLE_FILE_NAME).exists() {
            info!("{} is disabled, skip", path.display());
            continue;
        }
        if module_type == ModuleType::Active && path.join(defs::REMOVE_FILE_NAME).exists() {
            warn!("{} is removed, skip", path.display());
            continue;
        }

        f(&path)?;
    }

    Ok(())
}

fn foreach_active_module(f: impl FnMut(&Path) -> Result<()>) -> Result<()> {
    foreach_module(ModuleType::Active, f)
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

fn exec_script<T: AsRef<Path>>(path: T, wait: bool) -> Result<()> {
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
    foreach_module(ModuleType::All, |module| {
        if module.join(defs::REMOVE_FILE_NAME).exists() {
            info!("remove module: {}", module.display());

            let uninstaller = module.join("uninstall.sh");
            if uninstaller.exists() {
                if let Err(e) = exec_script(uninstaller, true) {
                    warn!("Failed to exec uninstaller: {}", e);
                }
            }

            if let Err(e) = remove_dir_all(module) {
                warn!("Failed to remove {}: {}", module.display(), e);
            }
        } else {
            remove_file(module.join(defs::UPDATE_FILE_NAME)).ok();
        }
        Ok(())
    })?;

    Ok(())
}

pub fn handle_updated_modules() -> Result<()> {
    let modules_root = Path::new(MODULE_DIR);
    foreach_module(ModuleType::Updated, |module| {
        if !module.is_dir() {
            return Ok(());
        }

        if let Some(name) = module.file_name() {
            let old_dir = modules_root.join(name);
            if old_dir.exists() {
                if let Err(e) = remove_dir_all(&old_dir) {
                    log::error!("Failed to remove old {}: {}", old_dir.display(), e);
                }
            }
            if let Err(e) = rename(module, &old_dir) {
                log::error!("Failed to move new module {}: {}", module.display(), e);
            }
        }
        Ok(())
    })?;
    Ok(())
}

pub fn install_module(zip: &str) -> Result<()> {
    fn inner(zip: &str) -> Result<()> {
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
        info!("module prop: {:?}", module_prop);

        let Some(module_id) = module_prop.get("id") else {
            bail!("module id not found in module.prop!");
        };
        let module_id = module_id.trim();

        let zip_uncompressed_size = get_zip_uncompressed_size(zip)?;

        info!(
            "zip uncompressed size: {}",
            humansize::format_size(zip_uncompressed_size, humansize::DECIMAL)
        );

        println!("- Preparing Zip");
        println!(
            "- Module size: {}",
            humansize::format_size(zip_uncompressed_size, humansize::DECIMAL)
        );

        // ensure modules_update exists
        ensure_dir_exists(MODULE_UPDATE_DIR)?;
        setsyscon(MODULE_UPDATE_DIR)?;

        let update_module_dir = Path::new(MODULE_UPDATE_DIR).join(module_id);
        ensure_clean_dir(&update_module_dir)?;
        info!("module dir: {}", update_module_dir.display());

        let do_install = || -> Result<()> {
            // unzip the image and move it to modules_update/<id> dir
            let file = File::open(zip)?;
            let mut archive = zip::ZipArchive::new(file)?;
            archive.extract(&update_module_dir)?;

            // set permission and selinux context for $MOD/system
            let module_system_dir = update_module_dir.join("system");
            if module_system_dir.exists() {
                #[cfg(unix)]
                set_permissions(&module_system_dir, Permissions::from_mode(0o755))?;
                restore_syscon(&module_system_dir)?;
            }

            exec_install_script(zip)?;

            let module_dir = Path::new(MODULE_DIR).join(module_id);
            ensure_dir_exists(&module_dir)?;
            copy(
                update_module_dir.join("module.prop"),
                module_dir.join("module.prop"),
            )?;
            ensure_file_exists(module_dir.join(UPDATE_FILE_NAME))?;

            info!("Module install successfully!");

            Ok(())
        };
        let result = do_install();
        if result.is_err() {
            remove_dir_all(&update_module_dir).ok();
        }
        result
    }
    let result = inner(zip);
    if let Err(ref e) = result {
        println!("- Error: {e}");
    }
    result
}

pub fn uninstall_module(id: &str) -> Result<()> {
    mark_module_state(id, defs::REMOVE_FILE_NAME, true)
}

pub fn run_action(id: &str) -> Result<()> {
    let action_script_path = format!("/data/adb/modules/{}/action.sh", id);
    exec_script(&action_script_path, true)
}

pub fn enable_module(id: &str) -> Result<()> {
    mark_module_state(id, defs::DISABLE_FILE_NAME, false)
}

pub fn disable_module(id: &str) -> Result<()> {
    mark_module_state(id, defs::DISABLE_FILE_NAME, true)
}

pub fn disable_all_modules() -> Result<()> {
    mark_all_modules(defs::DISABLE_FILE_NAME)
}

pub fn uninstall_all_modules() -> Result<()> {
    mark_all_modules(defs::REMOVE_FILE_NAME)
}

fn mark_all_modules(flag_file: &str) -> Result<()> {
    let dir = std::fs::read_dir(MODULE_DIR)?;
    for entry in dir.flatten() {
        let path = entry.path();
        let flag = path.join(flag_file);
        if let Err(e) = ensure_file_exists(flag) {
            warn!("Failed to mark module: {}: {}", path.display(), e);
        }
    }

    Ok(())
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
        let module_prop = path.join("module.prop");
        if !module_prop.exists() {
            continue;
        }
        let content = std::fs::read(&module_prop);
        let Ok(content) = content else {
            warn!("Failed to read file: {}", module_prop.display());
            continue;
        };
        let mut module_prop_map: HashMap<String, String> = HashMap::new();
        let encoding = encoding_rs::UTF_8;
        let result =
            PropertiesIter::new_with_encoding(Cursor::new(content), encoding).read_into(|k, v| {
                module_prop_map.insert(k, v);
            });

        if !module_prop_map.contains_key("id") || module_prop_map["id"].is_empty() {
            if let Some(id) = entry.file_name().to_str() {
                info!("Use dir name as module id: {}", id);
                module_prop_map.insert("id".to_owned(), id.to_owned());
            } else {
                info!("Failed to get module id: {:?}", module_prop);
                continue;
            }
        }

        // Add enabled, update, remove flags
        let enabled = !path.join(defs::DISABLE_FILE_NAME).exists();
        let update = path.join(defs::UPDATE_FILE_NAME).exists();
        let remove = path.join(defs::REMOVE_FILE_NAME).exists();
        let web = path.join(defs::MODULE_WEB_DIR).exists();
        let action = path.join(defs::MODULE_ACTION_SH).exists();

        module_prop_map.insert("enabled".to_owned(), enabled.to_string());
        module_prop_map.insert("update".to_owned(), update.to_string());
        module_prop_map.insert("remove".to_owned(), remove.to_string());
        module_prop_map.insert("web".to_owned(), web.to_string());
        module_prop_map.insert("action".to_owned(), action.to_string());

        if result.is_err() {
            warn!("Failed to parse module.prop: {}", module_prop.display());
            continue;
        }
        modules.push(module_prop_map);
    }

    modules
}

pub fn list_modules() -> Result<()> {
    let modules = _list_modules(defs::MODULE_DIR);
    println!("{}", serde_json::to_string_pretty(&modules)?);
    Ok(())
}
