use const_format::concatcp;
use java_properties::PropertiesIter;
use log::{info, warn};
use std::{
    collections::HashMap,
    fs::{create_dir_all, remove_dir_all, File, OpenOptions},
    io::{Cursor, Write},
    os::unix::{prelude::PermissionsExt, process::CommandExt},
    path::{Path, PathBuf},
    process::{Command, Stdio},
    str::FromStr,
};
use subprocess::Exec;
use zip_extensions::*;

use crate::{defs, restorecon};
use crate::{restorecon::setsyscon, utils::*};

use anyhow::{bail, ensure, Context, Result};

const UTIL_FUNCTIONS: &str = include_str!("./installer.sh");
const INSTALL_MODULE_SCRIPT: &str =
    concatcp!(UTIL_FUNCTIONS, "\n", "install_module", "\n", "exit 0", "\n");

fn exec_install_script(module_file: &str) -> Result<()> {
    let realpath = std::fs::canonicalize(module_file)
        .with_context(|| format!("realpath: {} failed", module_file))?;

    let result = Command::new("/system/bin/sh")
        .args(["-c", INSTALL_MODULE_SCRIPT])
        .env("OUTFD", "1")
        .env("ZIPFILE", realpath)
        .stderr(Stdio::null())
        .status()?;
    ensure!(result.success(), "install module script failed!");
    Ok(())
}

// becuase we use something like A-B update
// we need to update the module state after the boot_completed
// if someone(such as the module) install a module before the boot_completed
// then it may cause some problems, just forbid it
fn ensure_boot_completed() -> Result<()> {
    // ensure getprop sys.boot_completed = 1
    let output = Command::new("getprop")
        .arg("sys.boot_completed")
        .stdout(Stdio::piped())
        .output()?;
    let output = String::from_utf8_lossy(&output.stdout);
    if output.trim() != "1" {
        bail!("Android is Booting!");
    }
    Ok(())
}

fn mark_update() -> Result<()> {
    let update_file = Path::new(defs::WORKING_DIR).join(defs::UPDATE_FILE_NAME);
    if update_file.exists() {
        return Ok(());
    }

    std::fs::File::create(update_file)?;
    Ok(())
}

fn mark_module_state(module: &str, flag_file: &str, create_or_delete: bool) -> Result<()> {
    let module_state_file = Path::new(defs::MODULE_DIR).join(module).join(flag_file);
    if create_or_delete {
        if module_state_file.exists() {
            return Ok(());
        }
        std::fs::File::create(module_state_file)?;
    } else {
        if !module_state_file.exists() {
            return Ok(());
        }
        std::fs::remove_file(module_state_file)?;
    }

    Ok(())
}

fn get_minimal_image_size(img: &str) -> Result<u64> {
    let output = Command::new("resize2fs")
        .args(["-P", img])
        .stdout(Stdio::piped())
        .output()?;

    let output = String::from_utf8_lossy(&output.stdout);
    println!("- {}", output.trim());
    let regex = regex::Regex::new(r"filesystem: (\d+)")?;
    let result = regex
        .captures(output.as_ref())
        .ok_or_else(|| anyhow::anyhow!("regex not match"))?;
    let result = &result[1];
    let result = u64::from_str(result)?;
    Ok(result)
}

fn check_image(img: &str) -> Result<()> {
    let result = Command::new("e2fsck")
        .args(["-yf", img])
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .status()
        .with_context(|| format!("Failed exec e2fsck {}", img))?;
    let code = result.code();
    // 0 or 1 is ok
    // 0: no error
    // 1: file system errors corrected
    // https://man7.org/linux/man-pages/man8/e2fsck.8.html
    ensure!(
        code == Some(0) || code == Some(1),
        "check image e2fsck exec failed: {}",
        code.unwrap_or(-1)
    );
    Ok(())
}

fn grow_image_size(img: &str, extra_size: u64) -> Result<()> {
    let minimal_size = get_minimal_image_size(img)?;
    let target_size = minimal_size + extra_size;

    // check image
    check_image(img)?;

    println!(
        "- Target image size: {}",
        humansize::format_size(target_size, humansize::DECIMAL)
    );
    let target_size = target_size / 1024 + 1;

    let result = Exec::shell(format!("resize2fs {} {}K", img, target_size))
        .stdout(subprocess::NullFile)
        .stderr(subprocess::Redirection::Merge)
        .join()
        .with_context(|| format!("Failed to resize2fs {}", img))?;
    ensure!(result.success(), "resize2fs exec failed.");

    Ok(())
}

fn switch_cgroup(grp: &str, pid: u32) {
    let path = Path::new(grp).join("cgroup.procs");
    if !path.exists() {
        return;
    }

    let fp = OpenOptions::new().append(true).open(path);
    if let Ok(mut fp) = fp {
        let _ = writeln!(fp, "{}", pid);
    }
}

fn switch_cgroups() -> Result<()> {
    let pid = std::process::id();
    switch_cgroup("/acct", pid);
    switch_cgroup("/dev/cg2_bpf", pid);
    switch_cgroup("/sys/fs/cgroup", pid);
    if getprop("ro.config.per_app_memcg")? != "false" {
        switch_cgroup("/dev/memcg/apps", pid);
    }

    Ok(())
}

/// execute every modules' post-fs-data.sh
pub fn exec_post_fs_data() -> Result<()> {
    let modules_dir = Path::new(defs::MODULE_DIR);
    let dir = std::fs::read_dir(modules_dir)?;
    for entry in dir.flatten() {
        let path = entry.path();
        let disabled = path.join(defs::DISABLE_FILE_NAME);
        if disabled.exists() {
            println!("{} is disabled, skip", path.display());
            continue;
        }

        let post_fs_data = path.join("post-fs-data.sh");
        if !post_fs_data.exists() {
            continue;
        }
        println!("exec {} post-fs-data.sh", path.display());

        // pre_exec is unsafe!
        unsafe {
            Command::new("/system/bin/sh")
                .arg(&post_fs_data)
                .process_group(0)
                .pre_exec(|| {
                    // ignore the error?
                    let _ = switch_cgroups();
                    Ok(())
                })
                .current_dir(path)
                .env("KSU", "true")
                .status()
                .with_context(|| format!("Failed to exec {}", post_fs_data.display()))?;
        }
    }

    Ok(())
}

/// execute every modules' service.sh
pub fn exec_services() -> Result<()> {
    let modules_dir = Path::new(defs::MODULE_DIR);
    let dir = std::fs::read_dir(modules_dir)?;
    for entry in dir.flatten() {
        let path = entry.path();
        let disabled = path.join(defs::DISABLE_FILE_NAME);
        if disabled.exists() {
            println!("{} is disabled, skip", path.display());
            continue;
        }

        let service = path.join("service.sh");
        if !service.exists() {
            continue;
        }
        println!("exec {} service.sh", path.display());

        // pre_exec is unsafe!
        unsafe {
            Command::new("/system/bin/sh")
                .arg(&service)
                .process_group(0)
                .pre_exec(|| {
                    // ignore the error?
                    let _ = switch_cgroups();
                    Ok(())
                })
                .current_dir(path)
                .env("KSU", "true")
                .spawn() // don't wait
                .with_context(|| format!("Failed to exec {}", service.display()))?;
        }
    }

    Ok(())
}

const RESETPROP: &[u8] = include_bytes!("./resetprop");
const RESETPROP_PATH: &str = concatcp!(defs::WORKING_DIR, "/resetprop");

fn ensure_resetprop() -> Result<()> {
    if Path::new(RESETPROP_PATH).exists() {
        return Ok(());
    }
    std::fs::write(RESETPROP_PATH, RESETPROP)?;
    std::fs::set_permissions(RESETPROP_PATH, std::fs::Permissions::from_mode(0o755))?;
    Ok(())
}

pub fn load_system_prop() -> Result<()> {
    ensure_resetprop()?;

    let modules_dir = Path::new(defs::MODULE_DIR);
    let dir = std::fs::read_dir(modules_dir)?;
    for entry in dir.flatten() {
        let path = entry.path();
        let disabled = path.join(defs::DISABLE_FILE_NAME);
        if disabled.exists() {
            println!("{} is disabled, skip", path.display());
            continue;
        }

        let system_prop = path.join("system.prop");
        if !system_prop.exists() {
            continue;
        }
        println!("load {} system.prop", path.display());

        // resetprop --file system.prop
        Command::new(RESETPROP_PATH)
            .arg("-n")
            .arg("--file")
            .arg(&system_prop)
            .status()
            .with_context(|| format!("Failed to exec {}", system_prop.display()))?;
    }

    Ok(())
}

pub fn install_module(zip: String) -> Result<()> {
    ensure_boot_completed()?;

    // print banner
    println!(include_str!("banner"));

    // first check if workding dir is usable
    let working_dir = Path::new(defs::WORKING_DIR);
    if !working_dir.exists() {
        create_dir_all(working_dir)?;
    }

    ensure!(
        working_dir.is_dir(),
        "working dir exists but it is not a regular directory!"
    );

    // read the module_id from zip, if faild if will return early.
    let mut buffer: Vec<u8> = Vec::new();
    let entry_path = PathBuf::from_str("module.prop")?;
    let zip_path = PathBuf::from_str(&zip)?;
    zip_extract_file_to_memory(&zip_path, &entry_path, &mut buffer)?;

    let mut module_prop = HashMap::new();
    PropertiesIter::new_with_encoding(Cursor::new(buffer), encoding::all::UTF_8).read_into(
        |k, v| {
            module_prop.insert(k, v);
        },
    )?;

    let Some(module_id) = module_prop.get("id") else {
        bail!("module id not found in module.prop!");
    };
    info!("module id: {}", module_id);

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

    let default_reserve_size = 64 * 1024 * 1024;
    let zip_uncompressed_size = get_zip_uncompressed_size(&zip)?;
    let grow_size = default_reserve_size + zip_uncompressed_size;
    let grow_size_per_m = grow_size / 1024 / 1024 + 1;

    println!("- Preparing image");
    println!(
        "- Module size: {}",
        humansize::format_size(zip_uncompressed_size, humansize::DECIMAL)
    );

    if !modules_img_exist && !modules_update_img_exist {
        // if no modules and modules_update, it is brand new installation, we should create a new img
        // create a tmp module img and mount it to modules_update
        let result = Exec::shell(format!(
            "dd if=/dev/zero of={} bs=1M count={}",
            tmp_module_img, grow_size_per_m
        ))
        .stdout(subprocess::NullFile)
        .stderr(subprocess::Redirection::Merge)
        .join()?;
        ensure!(result.success(), "create ext4 image failed!");

        // format the img to ext4 filesystem
        let result = Exec::shell(format!("mkfs.ext4 {}", tmp_module_img))
            .stdout(subprocess::NullFile)
            .stderr(subprocess::Redirection::Merge)
            .join()?;
        ensure!(result.success(), "format ext4 image failed!");

        check_image(tmp_module_img)?;
    } else if modules_update_img_exist {
        // modules_update.img exists, we should use it as tmp img
        std::fs::copy(modules_update_img, tmp_module_img).with_context(|| {
            format!(
                "Failed to copy {} to {}",
                modules_update_img.display(),
                tmp_module_img
            )
        })?;
        // grow size of the tmp image
        grow_image_size(tmp_module_img, grow_size)?;
    } else {
        // modules.img exists, we should use it as tmp img
        std::fs::copy(modules_img, tmp_module_img).with_context(|| {
            format!(
                "Failed to copy {} to {}",
                modules_img.display(),
                tmp_module_img
            )
        })?;
        // grow size of the tmp image
        grow_image_size(tmp_module_img, grow_size)?;
    }

    // ensure modules_update exists
    ensure_clean_dir(module_update_tmp_dir)?;

    // mount the modules_update.img to mountpoint
    println!("- Mounting image");

    mount_image(tmp_module_img, module_update_tmp_dir)?;

    setsyscon(module_update_tmp_dir)?;

    let result = {
        let module_dir = format!("{}/{}", module_update_tmp_dir, module_id);
        ensure_clean_dir(&module_dir)?;
        info!("module dir: {}", module_dir);

        // unzip the image and move it to modules_update/<id> dir
        let file = File::open(&zip)?;
        let mut archive = zip::ZipArchive::new(file)?;
        archive.extract(&module_dir)?;

        // set selinux for module/system dir
        let mut module_system_dir = PathBuf::from(module_dir);
        module_system_dir.push("system");
        let module_system_dir = module_system_dir.as_path();
        if module_system_dir.exists() {
            let path = format!("{}", module_system_dir.display());
            restorecon::restore_syscon(&path)?;
        }

        exec_install_script(&zip)
    };

    // umount the modules_update.img
    let _ = umount_dir(module_update_tmp_dir);

    // remove modules_update dir, ignore the error
    let _ = remove_dir_all(module_update_tmp_dir);

    // return if exec script failed
    result.with_context(|| format!("Failed to execute install script for {}", module_id))?;

    // all done, rename the tmp image to modules_update.img
    if std::fs::rename(tmp_module_img, defs::MODULE_UPDATE_IMG).is_err() {
        let _ = std::fs::remove_file(tmp_module_img);
    }

    mark_update()?;

    Ok(())
}

fn do_module_update<F>(update_dir: &str, id: &str, func: F) -> Result<()>
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
        std::fs::copy(modules_update_img, modules_update_tmp_img)?;
    } else {
        info!(
            "copy {} to {}",
            modules_img.display(),
            modules_update_tmp_img.display()
        );
        std::fs::copy(modules_img, modules_update_tmp_img)?;
    }

    // ensure modules_update dir exist
    ensure_clean_dir(update_dir)?;

    // mount the modules_update img
    mount_image(defs::MODULE_UPDATE_TMP_IMG, update_dir)?;

    // call the operation func
    let result = func(id, update_dir);

    // umount modules_update.img
    let _ = umount_dir(update_dir);
    let _ = remove_dir_all(update_dir);

    std::fs::rename(modules_update_tmp_img, defs::MODULE_UPDATE_IMG)?;

    mark_update()?;

    result
}

pub fn uninstall_module(id: String) -> Result<()> {
    do_module_update(defs::MODULE_UPDATE_TMP_DIR, &id, |mid, update_dir| {
        let dir = Path::new(update_dir);
        if !dir.exists() {
            bail!("No module installed");
        }

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
            PropertiesIter::new_with_encoding(Cursor::new(content), encoding::all::UTF_8)
                .read_into(|k, v| {
                    if k.eq("id") {
                        module_id = v;
                    }
                })?;
            if module_id.eq(mid) {
                remove_dir_all(path)?;
                break;
            }
        }

        // santity check
        let target_module_path = format!("{}/{}", update_dir, mid);
        let target_module = Path::new(&target_module_path);
        if target_module.exists() {
            remove_dir_all(target_module)?;
        }

        let _ = mark_module_state(&id, defs::REMOVE_FILE_NAME, true);

        Ok(())
    })
}

fn do_enable_module(module_dir: &str, mid: &str, enable: bool) -> Result<()> {
    let src_module_path = format!("{}/{}", module_dir, mid);
    let src_module = Path::new(&src_module_path);
    if !src_module.exists() {
        bail!("module: {} not found!", mid);
    }

    let disable_path = src_module.join(defs::DISABLE_FILE_NAME);
    if enable {
        if disable_path.exists() {
            std::fs::remove_file(&disable_path).with_context(|| {
                format!("Failed to remove disable file: {}", &disable_path.display())
            })?;
        }
    } else if !disable_path.exists() {
        std::fs::File::create(disable_path)?;
    }

    let _ = mark_module_state(mid, defs::DISABLE_FILE_NAME, !enable);

    Ok(())
}

pub fn enable_module(id: String) -> Result<()> {
    do_module_update(defs::MODULE_UPDATE_TMP_DIR, &id, |mid, update_dir| {
        do_enable_module(update_dir, mid, true)
    })
}

pub fn disable_module(id: String) -> Result<()> {
    do_module_update(defs::MODULE_UPDATE_TMP_DIR, &id, |mid, update_dir| {
        do_enable_module(update_dir, mid, false)
    })
}

fn do_list_modules(path: &str) -> Vec<HashMap<String, String>> {
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
        let mut module_prop_map = HashMap::new();
        let encoding = encoding::all::UTF_8;
        let result =
            PropertiesIter::new_with_encoding(Cursor::new(content), encoding).read_into(|k, v| {
                module_prop_map.insert(k, v);
            });

        // Add enabled, update, remove flags
        let enabled = !path.join(defs::DISABLE_FILE_NAME).exists();
        let update = path.join(defs::UPDATE_FILE_NAME).exists();
        let remove = path.join(defs::REMOVE_FILE_NAME).exists();

        module_prop_map.insert("enabled".to_string(), enabled.to_string());
        module_prop_map.insert("update".to_string(), update.to_string());
        module_prop_map.insert("remove".to_string(), remove.to_string());

        if result.is_err() {
            warn!("Failed to parse module.prop: {}", module_prop.display());
            continue;
        }
        modules.push(module_prop_map);
    }

    modules
}

pub fn list_modules() -> Result<()> {
    let modules = do_list_modules(defs::MODULE_DIR);
    println!("{}", serde_json::to_string_pretty(&modules)?);
    Ok(())
}
