use crate::{
    assets, defs, mount,
    restorecon::{restore_syscon, setsyscon},
    sepolicy,
    utils::*,
};

use const_format::concatcp;
use java_properties::PropertiesIter;
use log::{debug, info, warn};
use std::{
    collections::HashMap,
    env::var as env_var,
    fs::{remove_dir_all, set_permissions, File, OpenOptions, Permissions},
    io::{Cursor, Read, Write},
    os::unix::{prelude::PermissionsExt, process::CommandExt},
    path::{Path, PathBuf},
    process::{Command, Stdio},
    str::FromStr,
};
use zip_extensions::zip_extract_file_to_memory;

use anyhow::{bail, ensure, Context, Result};

const UTIL_FUNCTIONS: &str = include_str!("./installer.sh");
const INSTALL_MODULE_SCRIPT: &str =
    concatcp!(UTIL_FUNCTIONS, "\n", "install_module", "\n", "exit 0", "\n");

fn exec_install_script(module_file: &str) -> Result<()> {
    let realpath = std::fs::canonicalize(module_file)
        .with_context(|| format!("realpath: {module_file} failed"))?;

    let result = Command::new(assets::BUSYBOX_PATH)
        .args(["sh", "-c", INSTALL_MODULE_SCRIPT])
        .env("ASH_STANDALONE", "1")
        .env(
            "PATH",
            format!("{}:{}", env_var("PATH").unwrap(), defs::BINARY_DIR),
        )
        .env("OUTFD", "1")
        .env("ZIPFILE", realpath)
        .stderr(Stdio::null())
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

fn get_minimal_image_size(img: &str) -> Result<u64> {
    let output = Command::new("resize2fs")
        .args(["-P", img])
        .stdout(Stdio::piped())
        .output()?;

    let output = String::from_utf8_lossy(&output.stdout);
    println!("- {}", output.trim());
    let regex = regex::Regex::new(r"filesystem: (\d+)")?;
    let result = regex
        .captures(&output)
        .ok_or(anyhow::anyhow!("regex not match"))?;
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
        .with_context(|| format!("Failed to exec e2fsck {img}"))?;
    let code = result.code();
    // 0 or 1 is ok
    // 0: no error
    // 1: file system errors corrected
    // https://man7.org/linux/man-pages/man8/e2fsck.8.html
    ensure!(
        code == Some(0) || code == Some(1),
        "Failed to check image, e2fsck exit code: {}",
        code.unwrap_or(-1)
    );
    Ok(())
}

fn grow_image_size(img: &str, extra_size: u64) -> Result<()> {
    let minimal_size = get_minimal_image_size(img)?; // the minimal size is in KB
    let target_size = minimal_size * 1024 + extra_size;

    // check image
    check_image(img)?;

    println!(
        "- Target image size: {}",
        humansize::format_size(target_size, humansize::DECIMAL)
    );
    let target_size = target_size / 1024 + 1024;

    let result = Command::new("resize2fs")
        .args([img, &format!("{target_size}K")])
        .stdout(Stdio::null())
        .status()
        .with_context(|| format!("Failed to exec resize2fs {img}"))?;
    ensure!(result.success(), "Failed to resize2fs: {}", result);

    check_image(img)?;

    Ok(())
}

fn switch_cgroup(grp: &str, pid: u32) {
    let path = Path::new(grp).join("cgroup.procs");
    if !path.exists() {
        return;
    }

    let fp = OpenOptions::new().append(true).open(path);
    if let Ok(mut fp) = fp {
        let _ = writeln!(fp, "{pid}");
    }
}

fn switch_cgroups() {
    let pid = std::process::id();
    switch_cgroup("/acct", pid);
    switch_cgroup("/dev/cg2_bpf", pid);
    switch_cgroup("/sys/fs/cgroup", pid);

    if getprop("ro.config.per_app_memcg")
        .filter(|prop| prop == "false")
        .is_none()
    {
        switch_cgroup("/dev/memcg/apps", pid);
    }
}

fn is_executable(path: &Path) -> bool {
    let mut buffer = [0u8; 2];
    is_executable::is_executable(path)
        && File::open(path).unwrap().read_exact(&mut buffer).is_ok()
        && (
            buffer == [0x23, 0x21] // shebang #!
            || buffer == [0x7f, 0x45]
            // ELF magic number 0x7F 'E'
        )
}

pub fn load_sepolicy_rule() -> Result<()> {
    let modules_dir = Path::new(defs::MODULE_DIR);
    let dir = std::fs::read_dir(modules_dir)?;
    for entry in dir.flatten() {
        let path = entry.path();
        let disabled = path.join(defs::DISABLE_FILE_NAME);
        if disabled.exists() {
            println!("{} is disabled, skip", path.display());
            continue;
        }

        let rule_file = path.join("sepolicy.rule");
        if !rule_file.exists() {
            continue;
        }
        println!("load policy: {}", &rule_file.display());

        if sepolicy::apply_file(&rule_file).is_err() {
            println!("Failed to load sepolicy.rule for {}", &rule_file.display());
        }
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
            warn!("{} is disabled, skip", path.display());
            continue;
        }

        let post_fs_data = path.join("post-fs-data.sh");
        if !post_fs_data.exists() {
            continue;
        }
        info!("exec {} post-fs-data.sh", path.display());

        let mut command = Command::new(assets::BUSYBOX_PATH);
        let command = command.arg("sh");
        let command = command.arg(&post_fs_data);

        let command = command
            .process_group(0)
            .current_dir(path)
            .env("ASH_STANDALONE", "1")
            .env(
                "PATH",
                format!("{}:{}", env_var("PATH").unwrap(), defs::BINARY_DIR),
            )
            .env("KSU", "true");
        let command = unsafe {
            command.pre_exec(|| {
                // ignore the error?
                switch_cgroups();
                Ok(())
            })
        };
        command
            .status()
            .with_context(|| format!("Failed to exec {}", post_fs_data.display()))?;
    }

    Ok(())
}

pub fn exec_common_scripts(dir: &str, wait: bool) -> Result<()> {
    let script_dir = Path::new(defs::WORKING_DIR).join(dir);
    if !script_dir.exists() {
        info!("{} not exists, skip", script_dir.display());
        return Ok(());
    }

    let dir = std::fs::read_dir(&script_dir)?;
    for entry in dir.flatten() {
        let path = entry.path();
        if !path.ends_with(".sh") {
            warn!("{} is not a shell script, skip", path.display());
            continue;
        }

        if !is_executable(&path) {
            warn!("{} is not executable, skip", path.display());
            continue;
        }

        info!("exec {}", path.display());

        let mut command = Command::new(assets::BUSYBOX_PATH);
        let command = command.arg("sh");
        let command = command.arg(&path);
        let command = command
            .process_group(0)
            .current_dir(&script_dir)
            .env("ASH_STANDALONE", "1")
            .env(
                "PATH",
                format!("{}:{}", env_var("PATH").unwrap(), defs::BINARY_DIR),
            )
            .env("KSU", "true");

        let command = unsafe {
            command.pre_exec(|| {
                switch_cgroups();
                Ok(())
            })
        };

        if !wait {
            command
                .spawn() // don't wait
                .with_context(|| format!("Failed to exec {}", path.display()))?;
        } else {
            command
                .status()
                .with_context(|| format!("Failed to exec {}", path.display()))?;
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
            warn!("{} is disabled, skip", path.display());
            continue;
        }

        let service = path.join("service.sh");
        if !service.exists() {
            continue;
        }
        info!("exec {} service.sh", path.display());

        let mut command = Command::new(assets::BUSYBOX_PATH);
        let command = command.arg("sh");
        let command = command.arg(&service);

        let command = command
            .process_group(0)
            .current_dir(path)
            .env("ASH_STANDALONE", "1")
            .env(
                "PATH",
                format!("{}:{}", env_var("PATH").unwrap(), defs::BINARY_DIR),
            )
            .env("KSU", "true");
        let command = unsafe {
            command.pre_exec(|| {
                // ignore the error?
                switch_cgroups();
                Ok(())
            })
        };
        command
            .spawn() // don't wait
            .with_context(|| format!("Failed to exec {}", service.display()))?;
    }

    Ok(())
}

pub fn load_system_prop() -> Result<()> {
    let modules_dir = Path::new(defs::MODULE_DIR);
    let dir = std::fs::read_dir(modules_dir)?;
    for entry in dir.flatten() {
        let path = entry.path();
        let disabled = path.join(defs::DISABLE_FILE_NAME);
        if disabled.exists() {
            info!("{} is disabled, skip", path.display());
            continue;
        }

        let system_prop = path.join("system.prop");
        if !system_prop.exists() {
            continue;
        }
        info!("load {} system.prop", path.display());

        // resetprop -n --file system.prop
        Command::new(assets::RESETPROP_PATH)
            .arg("-n")
            .arg("--file")
            .arg(&system_prop)
            .status()
            .with_context(|| format!("Failed to exec {}", system_prop.display()))?;
    }

    Ok(())
}

fn do_install_module(zip: String) -> Result<()> {
    ensure_boot_completed()?;

    // print banner
    println!(include_str!("banner"));

    assets::ensure_bin_assets().with_context(|| "Failed to extract assets")?;

    // first check if workding dir is usable
    ensure_dir_exists(defs::WORKING_DIR).with_context(|| "Failed to create working dir")?;
    ensure_dir_exists(defs::BINARY_DIR).with_context(|| "Failed to create bin dir")?;

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
    info!("module prop: {:?}", module_prop);

    let Some(module_id) = module_prop.get("id") else {
        bail!("module id not found in module.prop!");
    };

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

    info!(
        "zip uncompressed size: {}",
        humansize::format_size(zip_uncompressed_size, humansize::DECIMAL)
    );
    info!(
        "grow size: {}",
        humansize::format_size(grow_size, humansize::DECIMAL)
    );

    println!("- Preparing image");
    println!(
        "- Module size: {}",
        humansize::format_size(zip_uncompressed_size, humansize::DECIMAL)
    );

    if !modules_img_exist && !modules_update_img_exist {
        // if no modules and modules_update, it is brand new installation, we should create a new img
        // create a tmp module img and mount it to modules_update
        info!("Creating brand new module image");
        File::create(tmp_module_img)
            .context("Failed to create ext4 image file")?
            .set_len(grow_size)
            .context("Failed to extend ext4 image")?;

        // format the img to ext4 filesystem
        let result = Command::new("mkfs.ext4")
            .arg(tmp_module_img)
            .stdout(Stdio::null())
            .output()?;
        ensure!(
            result.status.success(),
            "Failed to format ext4 image: {}",
            String::from_utf8(result.stderr).unwrap()
        );

        check_image(tmp_module_img)?;
    } else if modules_update_img_exist {
        // modules_update.img exists, we should use it as tmp img
        info!("Using existing modules_update.img as tmp image");
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
        info!("Using existing modules.img as tmp image");
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

    let _dontdrop = mount::AutoMountExt4::try_new(tmp_module_img, module_update_tmp_dir, true)?;

    info!("mounted {} to {}", tmp_module_img, module_update_tmp_dir);

    setsyscon(module_update_tmp_dir)?;

    let module_dir = format!("{module_update_tmp_dir}/{module_id}");
    ensure_clean_dir(&module_dir)?;
    info!("module dir: {}", module_dir);

    // unzip the image and move it to modules_update/<id> dir
    let file = File::open(&zip)?;
    let mut archive = zip::ZipArchive::new(file)?;
    archive.extract(&module_dir)?;

    exec_install_script(&zip)?;

    // set permission and selinux context for $MOD/system
    let module_system_dir = PathBuf::from(module_dir).join("system");
    if module_system_dir.exists() {
        let path = module_system_dir.to_str().unwrap();
        set_permissions(&module_system_dir, Permissions::from_mode(0o755))?;
        restore_syscon(path)?;
    }

    info!("rename {tmp_module_img} to {}", defs::MODULE_UPDATE_IMG);
    // all done, rename the tmp image to modules_update.img
    if std::fs::rename(tmp_module_img, defs::MODULE_UPDATE_IMG).is_err() {
        warn!("Rename image failed, try copy it.");
        std::fs::copy(tmp_module_img, defs::MODULE_UPDATE_IMG)
            .with_context(|| "Failed to copy image.".to_string())?;
        let _ = std::fs::remove_file(tmp_module_img);
    }

    mark_update()?;

    info!("Module install successfully!");

    Ok(())
}

pub fn install_module(zip: String) -> Result<()> {
    let result = do_install_module(zip);
    if let Err(ref e) = result {
        // error happened, do some cleanup!
        let _ = std::fs::remove_file(defs::MODULE_UPDATE_TMP_IMG);
        let _ = mount::umount_dir(defs::MODULE_UPDATE_TMP_DIR);
        println!("- Error: {e}");
    }
    result
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
    let _dontdrop = mount::AutoMountExt4::try_new(defs::MODULE_UPDATE_TMP_IMG, update_dir, true)?;

    // call the operation func
    let result = func(id, update_dir);

    let _ = remove_dir_all(update_dir);

    std::fs::rename(modules_update_tmp_img, defs::MODULE_UPDATE_IMG)?;

    mark_update()?;

    result
}

pub fn uninstall_module(id: String) -> Result<()> {
    do_module_update(defs::MODULE_UPDATE_TMP_DIR, &id, |mid, update_dir| {
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
        let target_module_path = format!("{update_dir}/{mid}");
        let target_module = Path::new(&target_module_path);
        if target_module.exists() {
            remove_dir_all(target_module)?;
        }

        let _ = mark_module_state(&id, defs::REMOVE_FILE_NAME, true);

        Ok(())
    })
}

fn do_enable_module(module_dir: &str, mid: &str, enable: bool) -> Result<()> {
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
