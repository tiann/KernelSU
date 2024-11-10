#[allow(clippy::wildcard_imports)]
use crate::utils::*;
use crate::{
    assets, defs, ksucalls, mount,
    restorecon::{restore_syscon, setsyscon},
    sepolicy, utils,
};

use anyhow::{anyhow, bail, ensure, Context, Result};
use const_format::concatcp;
use is_executable::is_executable;
use java_properties::PropertiesIter;
use log::{info, warn};

use std::fs::OpenOptions;
use std::{
    collections::HashMap,
    env::var as env_var,
    fs::{remove_dir_all, remove_file, set_permissions, File, Permissions},
    io::Cursor,
    path::{Path, PathBuf},
    process::{Command, Stdio},
    str::FromStr,
};
use zip_extensions::zip_extract_file_to_memory;

#[cfg(unix)]
use std::os::unix::{fs::MetadataExt, prelude::PermissionsExt, process::CommandExt};

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

fn mark_update() -> Result<()> {
    ensure_file_exists(concatcp!(defs::WORKING_DIR, defs::UPDATE_FILE_NAME))
}

fn mark_module_state(module: &str, flag_file: &str, create_or_delete: bool) -> Result<()> {
    let module_state_file = Path::new(defs::MODULE_DIR).join(module).join(flag_file);
    if create_or_delete {
        ensure_file_exists(module_state_file)
    } else {
        if module_state_file.exists() {
            std::fs::remove_file(module_state_file)?;
        }
        Ok(())
    }
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

fn check_image(img: &str) -> Result<()> {
    let result = Command::new("e2fsck")
        .args(["-yf", img])
        .stdout(Stdio::piped())
        .status()
        .with_context(|| format!("Failed to exec e2fsck {img}"))?;
    let code = result.code();
    // 0 or 1 is ok
    // 0: no error
    // 1: file system errors corrected
    // https://man7.org/linux/man-pages/man8/e2fsck.8.html
    // ensure!(
    //     code == Some(0) || code == Some(1),
    //     "Failed to check image, e2fsck exit code: {}",
    //     code.unwrap_or(-1)
    // );
    info!("e2fsck exit code: {}", code.unwrap_or(-1));
    Ok(())
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
    foreach_module(false, |module| {
        remove_file(module.join(defs::UPDATE_FILE_NAME)).ok();

        if !module.join(defs::REMOVE_FILE_NAME).exists() {
            return Ok(());
        }

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

        Ok(())
    })?;

    Ok(())
}

fn create_module_image(image: &str, image_size: u64, journal_size: u64) -> Result<()> {
    File::create(image)
        .context("Failed to create ext4 image file")?
        .set_len(image_size)
        .context("Failed to truncate ext4 image")?;

    // format the img to ext4 filesystem
    let result = Command::new("mkfs.ext4")
        .arg("-J")
        .arg(format!("size={journal_size}"))
        .arg(image)
        .stdout(Stdio::piped())
        .output()?;
    ensure!(
        result.status.success(),
        "Failed to format ext4 image: {}",
        String::from_utf8(result.stderr).unwrap()
    );
    check_image(image)?;
    Ok(())
}
fn _install_module(zip: &str) -> Result<()> {
    ensure_boot_completed()?;

    // print banner
    println!(include_str!("banner"));

    assets::ensure_binaries(false).with_context(|| "Failed to extract assets")?;

    // first check if workding dir is usable
    ensure_dir_exists(defs::WORKING_DIR).with_context(|| "Failed to create working dir")?;
    ensure_dir_exists(defs::BINARY_DIR).with_context(|| "Failed to create bin dir")?;

    // read the module_id from zip, if faild if will return early.
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

    let modules_img = Path::new(defs::MODULE_IMG);
    let modules_update_img = Path::new(defs::MODULE_UPDATE_IMG);
    let module_update_tmp_dir = defs::MODULE_UPDATE_TMP_DIR;

    let modules_img_exist = modules_img.exists();
    let modules_update_img_exist = modules_update_img.exists();

    // prepare the tmp module img
    let tmp_module_img = defs::MODULE_UPDATE_TMP_IMG;
    let tmp_module_path = Path::new(tmp_module_img);
    if tmp_module_path.exists() {
        std::fs::remove_file(tmp_module_path)?;
    }

    let zip_uncompressed_size = get_zip_uncompressed_size(zip)?;

    info!(
        "zip uncompressed size: {}",
        humansize::format_size(zip_uncompressed_size, humansize::DECIMAL)
    );

    println!("- Preparing image");
    println!(
        "- Module size: {}",
        humansize::format_size(zip_uncompressed_size, humansize::DECIMAL)
    );

    let sparse_image_size = 1 << 40; // 1T
    let journal_size = 8; // 8M
    if !modules_img_exist && !modules_update_img_exist {
        // if no modules and modules_update, it is brand new installation, we should create a new img
        // create a tmp module img and mount it to modules_update
        info!("Creating brand new module image");
        create_module_image(tmp_module_img, sparse_image_size, journal_size)?;
    } else if modules_update_img_exist {
        // modules_update.img exists, we should use it as tmp img
        info!("Using existing modules_update.img as tmp image");
        utils::copy_sparse_file(modules_update_img, tmp_module_img, true).with_context(|| {
            format!(
                "Failed to copy {} to {}",
                modules_update_img.display(),
                tmp_module_img
            )
        })?;
    } else {
        // modules.img exists, we should use it as tmp img
        info!("Using existing modules.img as tmp image");

        #[cfg(unix)]
        let blksize = std::fs::metadata(defs::MODULE_DIR)?.blksize();
        #[cfg(not(unix))]
        let blksize = 0;
        // legacy image, it's block size is 1024 with unlimited journal size
        if blksize == 1024 {
            println!("- Legacy image, migrating to new format, please be patient...");
            create_module_image(tmp_module_img, sparse_image_size, journal_size)?;
            let _dontdrop =
                mount::AutoMountExt4::try_new(tmp_module_img, module_update_tmp_dir, true)
                    .with_context(|| format!("Failed to mount {tmp_module_img}"))?;
            utils::copy_module_files(defs::MODULE_DIR, module_update_tmp_dir)
                .with_context(|| "Failed to migrate module files".to_string())?;
        } else {
            utils::copy_sparse_file(modules_img, tmp_module_img, true)
                .with_context(|| "Failed to copy module image".to_string())?;

            if std::fs::metadata(tmp_module_img)?.len() < sparse_image_size {
                // truncate the file to new size
                OpenOptions::new()
                    .write(true)
                    .open(tmp_module_img)
                    .context("Failed to open ext4 image")?
                    .set_len(sparse_image_size)
                    .context("Failed to truncate ext4 image")?;

                // resize the image to new size
                check_image(tmp_module_img)?;
                Command::new("resize2fs")
                    .arg(tmp_module_img)
                    .stdout(Stdio::piped())
                    .status()?;
            }
        }
    }

    // ensure modules_update exists
    ensure_dir_exists(module_update_tmp_dir)?;

    // mount the modules_update.img to mountpoint
    println!("- Mounting image");

    let _dontdrop = mount::AutoMountExt4::try_new(tmp_module_img, module_update_tmp_dir, true)?;

    info!("mounted {} to {}", tmp_module_img, module_update_tmp_dir);

    setsyscon(module_update_tmp_dir)?;

    let module_dir = format!("{module_update_tmp_dir}/{module_id}");
    ensure_clean_dir(&module_dir)?;
    info!("module dir: {}", module_dir);

    // unzip the image and move it to modules_update/<id> dir
    let file = File::open(zip)?;
    let mut archive = zip::ZipArchive::new(file)?;
    archive.extract(&module_dir)?;

    // set permission and selinux context for $MOD/system
    let module_system_dir = PathBuf::from(module_dir).join("system");
    if module_system_dir.exists() {
        #[cfg(unix)]
        set_permissions(&module_system_dir, Permissions::from_mode(0o755))?;
        restore_syscon(&module_system_dir)?;
    }

    exec_install_script(zip)?;

    info!("rename {tmp_module_img} to {}", defs::MODULE_UPDATE_IMG);
    // all done, rename the tmp image to modules_update.img
    if std::fs::rename(tmp_module_img, defs::MODULE_UPDATE_IMG).is_err() {
        warn!("Rename image failed, try copy it.");
        utils::copy_sparse_file(tmp_module_img, defs::MODULE_UPDATE_IMG, true)
            .with_context(|| "Failed to copy image.".to_string())?;
        let _ = std::fs::remove_file(tmp_module_img);
    }

    mark_update()?;

    info!("Module install successfully!");

    Ok(())
}

pub fn install_module(zip: &str) -> Result<()> {
    let result = _install_module(zip);
    if let Err(ref e) = result {
        // error happened, do some cleanup!
        let _ = std::fs::remove_file(defs::MODULE_UPDATE_TMP_IMG);
        let _ = mount::umount_dir(defs::MODULE_UPDATE_TMP_DIR);
        println!("- Error: {e}");
    }
    result
}

fn update_module<F>(update_dir: &str, id: &str, func: F) -> Result<()>
where
    F: Fn(&str, &str) -> Result<()>,
{
    ensure_boot_completed()?;

    let modules_img = Path::new(defs::MODULE_IMG);
    let modules_update_img = Path::new(defs::MODULE_UPDATE_IMG);
    let modules_update_tmp_img = Path::new(defs::MODULE_UPDATE_TMP_IMG);
    if !modules_update_img.exists() && !modules_img.exists() {
        bail!("Please install module first!");
    } else if modules_update_img.exists() {
        info!(
            "copy {} to {}",
            modules_update_img.display(),
            modules_update_tmp_img.display()
        );
        utils::copy_sparse_file(modules_update_img, modules_update_tmp_img, true)?;
    } else {
        info!(
            "copy {} to {}",
            modules_img.display(),
            modules_update_tmp_img.display()
        );
        utils::copy_sparse_file(modules_img, modules_update_tmp_img, true)?;
    }

    // ensure modules_update dir exist
    ensure_clean_dir(update_dir)?;

    // mount the modules_update img
    let _dontdrop = mount::AutoMountExt4::try_new(defs::MODULE_UPDATE_TMP_IMG, update_dir, true)?;

    // call the operation func
    let result = func(id, update_dir);

    if let Err(e) = std::fs::rename(modules_update_tmp_img, defs::MODULE_UPDATE_IMG) {
        warn!("Rename image failed: {e}, try copy it.");
        utils::copy_sparse_file(modules_update_tmp_img, defs::MODULE_UPDATE_IMG, true)
            .with_context(|| "Failed to copy image.".to_string())?;
        let _ = std::fs::remove_file(modules_update_tmp_img);
    }

    mark_update()?;

    result
}

pub fn uninstall_module(id: &str) -> Result<()> {
    update_module(defs::MODULE_UPDATE_TMP_DIR, id, |mid, update_dir| {
        let dir = Path::new(update_dir);
        ensure!(dir.exists(), "No module installed");

        // iterate the modules_update dir, find the module to be removed
        let dir = std::fs::read_dir(dir)?;
        for entry in dir.flatten() {
            let path = entry.path();
            let module_prop = path.join("module.prop");
            if !module_prop.exists() {
                continue;
            }
            let content = std::fs::read(module_prop)?;
            let mut module_id: String = String::new();
            PropertiesIter::new_with_encoding(Cursor::new(content), encoding_rs::UTF_8).read_into(
                |k, v| {
                    if k.eq("id") {
                        module_id = v;
                    }
                },
            )?;
            if module_id.eq(mid) {
                let remove_file = path.join(defs::REMOVE_FILE_NAME);
                File::create(remove_file).with_context(|| "Failed to create remove file.")?;
                break;
            }
        }

        // santity check
        let target_module_path = format!("{update_dir}/{mid}");
        let target_module = Path::new(&target_module_path);
        if target_module.exists() {
            let remove_file = target_module.join(defs::REMOVE_FILE_NAME);
            if !remove_file.exists() {
                File::create(remove_file).with_context(|| "Failed to create remove file.")?;
            }
        }

        let _ = mark_module_state(id, defs::REMOVE_FILE_NAME, true);

        Ok(())
    })
}

pub fn run_action(id: &str) -> Result<()> {
    let action_script_path = format!("/data/adb/modules/{}/action.sh", id);
    exec_script(&action_script_path, true)
}

fn _enable_module(module_dir: &str, mid: &str, enable: bool) -> Result<()> {
    let src_module_path = format!("{module_dir}/{mid}");
    let src_module = Path::new(&src_module_path);
    ensure!(src_module.exists(), "module: {} not found!", mid);

    let disable_path = src_module.join(defs::DISABLE_FILE_NAME);
    if enable {
        if disable_path.exists() {
            std::fs::remove_file(&disable_path).with_context(|| {
                format!("Failed to remove disable file: {}", &disable_path.display())
            })?;
        }
    } else {
        ensure_file_exists(disable_path)?;
    }

    let _ = mark_module_state(mid, defs::DISABLE_FILE_NAME, !enable);

    Ok(())
}

pub fn enable_module(id: &str) -> Result<()> {
    update_module(defs::MODULE_UPDATE_TMP_DIR, id, |mid, update_dir| {
        _enable_module(update_dir, mid, true)
    })
}

pub fn disable_module(id: &str) -> Result<()> {
    update_module(defs::MODULE_UPDATE_TMP_DIR, id, |mid, update_dir| {
        _enable_module(update_dir, mid, false)
    })
}

pub fn disable_all_modules() -> Result<()> {
    mark_all_modules(defs::DISABLE_FILE_NAME)
}

pub fn uninstall_all_modules() -> Result<()> {
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

pub fn shrink_image(img: &str) -> Result<()> {
    check_image(img)?;
    Command::new("resize2fs")
        .arg("-M")
        .arg(img)
        .stdout(Stdio::piped())
        .status()?;
    Ok(())
}

pub fn shrink_ksu_images() -> Result<()> {
    shrink_image(defs::MODULE_IMG)?;
    if Path::new(defs::MODULE_UPDATE_IMG).exists() {
        shrink_image(defs::MODULE_UPDATE_IMG)?;
    }
    Ok(())
}
