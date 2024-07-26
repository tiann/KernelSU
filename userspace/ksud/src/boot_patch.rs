#[cfg(unix)]
use std::os::unix::fs::PermissionsExt;
use std::path::Path;
use std::path::PathBuf;
use std::process::Command;
use std::process::Stdio;

use anyhow::anyhow;
use anyhow::bail;
use anyhow::ensure;
use anyhow::Context;
use anyhow::Result;
use regex_lite::Regex;
use which::which;

use crate::defs;
use crate::defs::BACKUP_FILENAME;
use crate::defs::{KSU_BACKUP_DIR, KSU_BACKUP_FILE_PREFIX};
use crate::{assets, utils};

#[cfg(target_os = "android")]
fn ensure_gki_kernel() -> Result<()> {
    let version = get_kernel_version()?;
    let is_gki = version.0 == 5 && version.1 >= 10 || version.2 > 5;
    ensure!(is_gki, "only support GKI kernel");
    Ok(())
}

#[cfg(target_os = "android")]
pub fn get_kernel_version() -> Result<(i32, i32, i32)> {
    let uname = rustix::system::uname();
    let version = uname.release().to_string_lossy();
    let re = Regex::new(r"(\d+)\.(\d+)\.(\d+)")?;
    if let Some(captures) = re.captures(&version) {
        let major = captures
            .get(1)
            .and_then(|m| m.as_str().parse::<i32>().ok())
            .ok_or_else(|| anyhow!("Major version parse error"))?;
        let minor = captures
            .get(2)
            .and_then(|m| m.as_str().parse::<i32>().ok())
            .ok_or_else(|| anyhow!("Minor version parse error"))?;
        let patch = captures
            .get(3)
            .and_then(|m| m.as_str().parse::<i32>().ok())
            .ok_or_else(|| anyhow!("Patch version parse error"))?;
        Ok((major, minor, patch))
    } else {
        Err(anyhow!("Invalid kernel version string"))
    }
}

#[cfg(target_os = "android")]
fn parse_kmi(version: &str) -> Result<String> {
    let re = Regex::new(r"(.* )?(\d+\.\d+)(\S+)?(android\d+)(.*)")?;
    let cap = re
        .captures(version)
        .ok_or_else(|| anyhow::anyhow!("Failed to get KMI from boot/modules"))?;
    let android_version = cap.get(4).map_or("", |m| m.as_str());
    let kernel_version = cap.get(2).map_or("", |m| m.as_str());
    Ok(format!("{android_version}-{kernel_version}"))
}

#[cfg(target_os = "android")]
fn parse_kmi_from_uname() -> Result<String> {
    let uname = rustix::system::uname();
    let version = uname.release().to_string_lossy();
    parse_kmi(&version)
}

#[cfg(target_os = "android")]
fn parse_kmi_from_modules() -> Result<String> {
    use std::io::BufRead;
    // find a *.ko in /vendor/lib/modules
    let modfile = std::fs::read_dir("/vendor/lib/modules")?
        .filter_map(Result::ok)
        .find(|entry| entry.path().extension().map_or(false, |ext| ext == "ko"))
        .map(|entry| entry.path())
        .ok_or_else(|| anyhow!("No kernel module found"))?;
    let output = Command::new("modinfo").arg(modfile).output()?;
    for line in output.stdout.lines().map_while(Result::ok) {
        if line.starts_with("vermagic") {
            return parse_kmi(&line);
        }
    }
    anyhow::bail!("Parse KMI from modules failed")
}

#[cfg(target_os = "android")]
pub fn get_current_kmi() -> Result<String> {
    parse_kmi_from_uname().or_else(|_| parse_kmi_from_modules())
}

#[cfg(not(target_os = "android"))]
pub fn get_current_kmi() -> Result<String> {
    bail!("Unsupported platform")
}

fn parse_kmi_from_kernel(kernel: &PathBuf, workdir: &Path) -> Result<String> {
    use std::fs::{copy, File};
    use std::io::{BufReader, Read};
    let kernel_path = workdir.join("kernel");
    copy(&kernel, &kernel_path).context("Failed to copy kernel")?;

    let file = File::open(&kernel_path).context("Failed to open kernel file")?;
    let mut reader = BufReader::new(file);
    let mut buffer = Vec::new();
    reader
        .read_to_end(&mut buffer)
        .context("Failed to read kernel file")?;

    let printable_strings: Vec<&str> = buffer
        .split(|&b| b == 0)
        .filter_map(|slice| std::str::from_utf8(slice).ok())
        .filter(|s| s.chars().all(|c| c.is_ascii_graphic() || c == ' '))
        .collect();

    let re =
        Regex::new(r"(?:.* )?(\d+\.\d+)(?:\S+)?(android\d+)").context("Failed to compile regex")?;
    for s in printable_strings {
        if let Some(caps) = re.captures(s) {
            if let (Some(kernel_version), Some(android_version)) = (caps.get(1), caps.get(2)) {
                let kmi = format!("{}-{}", android_version.as_str(), kernel_version.as_str());
                return Ok(kmi);
            }
        }
    }
    println!("- Failed to get KMI version");
    bail!("Try to choose LKM manually")
}

fn parse_kmi_from_boot(magiskboot: &Path, image: &PathBuf, workdir: &Path) -> Result<String> {
    let image_path = workdir.join("image");

    std::fs::copy(&image, &image_path).context("Failed to copy image")?;

    let status = Command::new(magiskboot)
        .current_dir(workdir)
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .arg("unpack")
        .arg(&image_path)
        .status()
        .context("Failed to execute magiskboot command")?;

    if !status.success() {
        bail!(
            "magiskboot unpack failed with status: {:?}",
            status.code().unwrap()
        );
    }

    parse_kmi_from_kernel(&image_path, workdir)
}

fn do_cpio_cmd(magiskboot: &Path, workdir: &Path, cmd: &str) -> Result<()> {
    let status = Command::new(magiskboot)
        .current_dir(workdir)
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .arg("cpio")
        .arg("ramdisk.cpio")
        .arg(cmd)
        .status()?;

    ensure!(status.success(), "magiskboot cpio {} failed", cmd);
    Ok(())
}

fn is_magisk_patched(magiskboot: &Path, workdir: &Path) -> Result<bool> {
    let status = Command::new(magiskboot)
        .current_dir(workdir)
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .args(["cpio", "ramdisk.cpio", "test"])
        .status()?;

    // 0: stock, 1: magisk
    Ok(status.code() == Some(1))
}

fn is_kernelsu_patched(magiskboot: &Path, workdir: &Path) -> Result<bool> {
    let status = Command::new(magiskboot)
        .current_dir(workdir)
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .args(["cpio", "ramdisk.cpio", "exists kernelsu.ko"])
        .status()?;

    Ok(status.success())
}

fn dd<P: AsRef<Path>, Q: AsRef<Path>>(ifile: P, ofile: Q) -> Result<()> {
    let status = Command::new("dd")
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .arg(format!("if={}", ifile.as_ref().display()))
        .arg(format!("of={}", ofile.as_ref().display()))
        .status()?;
    ensure!(
        status.success(),
        "dd if={:?} of={:?} failed",
        ifile.as_ref(),
        ofile.as_ref()
    );
    Ok(())
}

pub fn restore(
    image: Option<PathBuf>,
    magiskboot_path: Option<PathBuf>,
    flash: bool,
) -> Result<()> {
    let tmpdir = tempdir::TempDir::new("KernelSU").context("create temp dir failed")?;
    let workdir = tmpdir.path();
    let magiskboot = find_magiskboot(magiskboot_path, workdir)?;

    let kmi = get_current_kmi().unwrap_or_else(|_| String::from(""));

    let skip_init = kmi.starts_with("android12-");

    let (bootimage, bootdevice) = find_boot_image(&image, skip_init, false, false, workdir)?;

    println!("- Unpacking boot image");
    let status = Command::new(&magiskboot)
        .current_dir(workdir)
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .arg("unpack")
        .arg(bootimage.display().to_string())
        .status()?;
    ensure!(status.success(), "magiskboot unpack failed");

    let is_kernelsu_patched = is_kernelsu_patched(&magiskboot, workdir)?;
    ensure!(is_kernelsu_patched, "boot image is not patched by KernelSU");

    let mut new_boot = None;
    let mut from_backup = false;

    #[cfg(target_os = "android")]
    if do_cpio_cmd(&magiskboot, workdir, &format!("exists {BACKUP_FILENAME}")).is_ok() {
        do_cpio_cmd(
            &magiskboot,
            workdir,
            &format!("extract {0} {0}", BACKUP_FILENAME),
        )?;
        let sha = std::fs::read(workdir.join(BACKUP_FILENAME))?;
        let sha = String::from_utf8(sha)?;
        let sha = sha.trim();
        let backup_path =
            PathBuf::from(KSU_BACKUP_DIR).join(format!("{KSU_BACKUP_FILE_PREFIX}{sha}"));
        if backup_path.is_file() {
            new_boot = Some(backup_path);
            from_backup = true;
        } else {
            println!("- Warning: no backup {backup_path:?} found!");
        }

        if let Err(e) = clean_backup(sha) {
            println!("- Warning: Cleanup backup image failed: {e}");
        }
    } else {
        println!("- Backup info is absent!");
    }

    if new_boot.is_none() {
        // remove kernelsu.ko
        do_cpio_cmd(&magiskboot, workdir, "rm kernelsu.ko")?;

        // if init.real exists, restore it
        let status = do_cpio_cmd(&magiskboot, workdir, "exists init.real").is_ok();
        if status {
            do_cpio_cmd(&magiskboot, workdir, "mv init.real init")?;
        } else {
            let ramdisk = workdir.join("ramdisk.cpio");
            std::fs::remove_file(ramdisk)?;
        }

        println!("- Repacking boot image");
        let status = Command::new(&magiskboot)
            .current_dir(workdir)
            .stdout(Stdio::null())
            .stderr(Stdio::null())
            .arg("repack")
            .arg(bootimage.display().to_string())
            .status()?;
        ensure!(status.success(), "magiskboot repack failed");
        new_boot = Some(workdir.join("new-boot.img"));
    }

    let new_boot = new_boot.unwrap();

    if image.is_some() {
        // if image is specified, write to output file
        let output_dir = std::env::current_dir()?;
        let now = chrono::Utc::now();
        let output_image = output_dir.join(format!(
            "kernelsu_restore_{}.img",
            now.format("%Y%m%d_%H%M%S")
        ));

        if from_backup || std::fs::rename(&new_boot, &output_image).is_err() {
            std::fs::copy(&new_boot, &output_image).context("copy out new boot failed")?;
        }
        println!("- Output file is written to");
        println!("- {}", output_image.display().to_string().trim_matches('"'));
    }
    if flash {
        if from_backup {
            println!("- Flashing new boot image from {}", new_boot.display());
        } else {
            println!("- Flashing new boot image");
        }
        flash_boot(&bootdevice, new_boot)?;
    }
    println!("- Done!");
    Ok(())
}

#[allow(clippy::too_many_arguments)]
pub fn patch(
    image: Option<PathBuf>,
    kernel: Option<PathBuf>,
    kmod: Option<PathBuf>,
    init: Option<PathBuf>,
    ota: bool,
    flash: bool,
    out: Option<PathBuf>,
    magiskboot: Option<PathBuf>,
    kmi: Option<String>,
) -> Result<()> {
    let result = do_patch(image, kernel, kmod, init, ota, flash, out, magiskboot, kmi);
    if let Err(ref e) = result {
        println!("- Install Error: {e}");
    }
    result
}

#[allow(clippy::too_many_arguments)]
fn do_patch(
    image: Option<PathBuf>,
    kernel: Option<PathBuf>,
    kmod: Option<PathBuf>,
    init: Option<PathBuf>,
    ota: bool,
    flash: bool,
    out: Option<PathBuf>,
    magiskboot_path: Option<PathBuf>,
    kmi: Option<String>,
) -> Result<()> {
    println!(include_str!("banner"));

    let patch_file = image.is_some();

    #[cfg(target_os = "android")]
    if !patch_file {
        ensure_gki_kernel()?;
    }

    let is_replace_kernel = kernel.is_some();

    if is_replace_kernel {
        ensure!(
            init.is_none() && kmod.is_none(),
            "init and module must not be specified."
        );
    }

    let tmpdir = tempdir::TempDir::new("KernelSU").context("create temp dir failed")?;
    let workdir = tmpdir.path();

    // extract magiskboot
    let magiskboot = find_magiskboot(magiskboot_path, workdir)?;

    let kmi = if let Some(kmi) = kmi {
        kmi
    } else {
        let kmi = match get_current_kmi() {
            Ok(value) => value,
            Err(e) => {
                println!("- {}", e);
                if let Some(image_path) = &image {
                    println!(
                        "- Trying to auto detect KMI version for {}",
                        image_path.to_str().unwrap()
                    );
                    parse_kmi_from_boot(&magiskboot, image_path, tmpdir.path())?
                } else if let Some(kernel_path) = &kernel {
                    println!(
                        "- Trying to auto detect KMI version for {}",
                        kernel_path.to_str().unwrap()
                    );
                    parse_kmi_from_kernel(kernel_path, tmpdir.path())?
                } else {
                    "".to_string()
                }
            }
        };
        kmi
    };

    let skip_init = kmi.starts_with("android12-");

    let (bootimage, bootdevice) =
        find_boot_image(&image, skip_init, ota, is_replace_kernel, workdir)?;

    let bootimage = bootimage.display().to_string();

    // try extract magiskboot/bootctl
    let _ = assets::ensure_binaries(false);

    if let Some(kernel) = kernel {
        std::fs::copy(kernel, workdir.join("kernel")).context("copy kernel from failed")?;
    }

    println!("- Preparing assets");

    let kmod_file = workdir.join("kernelsu.ko");
    if let Some(kmod) = kmod {
        std::fs::copy(kmod, kmod_file).context("copy kernel module failed")?;
    } else {
        // If kmod is not specified, extract from assets
        println!("- KMI: {kmi}");
        let name = format!("{kmi}_kernelsu.ko");
        assets::copy_assets_to_file(&name, kmod_file)
            .with_context(|| format!("Failed to copy {name}"))?;
    };

    let init_file = workdir.join("init");
    if let Some(init) = init {
        std::fs::copy(init, init_file).context("copy init failed")?;
    } else {
        assets::copy_assets_to_file("ksuinit", init_file).context("copy ksuinit failed")?;
    }

    // magiskboot unpack boot.img
    // magiskboot cpio ramdisk.cpio 'cp init init.real'
    // magiskboot cpio ramdisk.cpio 'add 0755 ksuinit init'
    // magiskboot cpio ramdisk.cpio 'add 0755 <kmod> kernelsu.ko'

    println!("- Unpacking boot image");
    let status = Command::new(&magiskboot)
        .current_dir(workdir)
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .arg("unpack")
        .arg(&bootimage)
        .status()?;
    ensure!(status.success(), "magiskboot unpack failed");

    let no_ramdisk = !workdir.join("ramdisk.cpio").exists();
    let is_magisk_patched = is_magisk_patched(&magiskboot, workdir)?;
    ensure!(
        no_ramdisk || !is_magisk_patched,
        "Cannot work with Magisk patched image"
    );

    println!("- Adding KernelSU LKM");
    let is_kernelsu_patched = is_kernelsu_patched(&magiskboot, workdir)?;

    let mut need_backup = false;
    if !is_kernelsu_patched {
        // kernelsu.ko is not exist, backup init if necessary
        let status = do_cpio_cmd(&magiskboot, workdir, "exists init");
        if status.is_ok() {
            do_cpio_cmd(&magiskboot, workdir, "mv init init.real")?;
        }

        need_backup = flash;
    }

    do_cpio_cmd(&magiskboot, workdir, "add 0755 init init")?;
    do_cpio_cmd(&magiskboot, workdir, "add 0755 kernelsu.ko kernelsu.ko")?;

    #[cfg(target_os = "android")]
    if need_backup {
        if let Err(e) = do_backup(&magiskboot, workdir, &bootimage) {
            println!("- Backup stock image failed: {e}");
        }
    }

    println!("- Repacking boot image");
    // magiskboot repack boot.img
    let status = Command::new(&magiskboot)
        .current_dir(workdir)
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .arg("repack")
        .arg(&bootimage)
        .status()?;
    ensure!(status.success(), "magiskboot repack failed");
    let new_boot = workdir.join("new-boot.img");

    if patch_file {
        // if image is specified, write to output file
        let output_dir = out.unwrap_or(std::env::current_dir()?);
        let now = chrono::Utc::now();
        let output_image = output_dir.join(format!(
            "kernelsu_patched_{}.img",
            now.format("%Y%m%d_%H%M%S")
        ));

        if std::fs::rename(&new_boot, &output_image).is_err() {
            std::fs::copy(&new_boot, &output_image).context("copy out new boot failed")?;
        }
        println!("- Output file is written to");
        println!("- {}", output_image.display().to_string().trim_matches('"'));
    }

    if flash {
        println!("- Flashing new boot image");
        flash_boot(&bootdevice, new_boot)?;

        if ota {
            post_ota()?;
        }
    }

    println!("- Done!");
    Ok(())
}

#[cfg(target_os = "android")]
fn calculate_sha1(file_path: impl AsRef<Path>) -> Result<String> {
    use sha1::Digest;
    use std::io::Read;
    let mut file = std::fs::File::open(file_path.as_ref())?;
    let mut hasher = sha1::Sha1::new();
    let mut buffer = [0; 1024];

    loop {
        let n = file.read(&mut buffer)?;
        if n == 0 {
            break;
        }
        hasher.update(&buffer[..n]);
    }

    let result = hasher.finalize();
    Ok(format!("{:x}", result))
}

#[cfg(target_os = "android")]
fn do_backup(magiskboot: &Path, workdir: &Path, image: &str) -> Result<()> {
    let sha1 = calculate_sha1(image)?;
    let filename = format!("{KSU_BACKUP_FILE_PREFIX}{sha1}");

    println!("- Backup stock boot image");
    // magiskboot cpio ramdisk.cpio 'add 0755 $BACKUP_FILENAME'
    let target = format!("{KSU_BACKUP_DIR}{filename}");
    std::fs::copy(image, &target).with_context(|| format!("backup to {target}"))?;
    std::fs::write(workdir.join(BACKUP_FILENAME), sha1.as_bytes()).context("write sha1")?;
    do_cpio_cmd(
        magiskboot,
        workdir,
        &format!("add 0755 {0} {0}", BACKUP_FILENAME),
    )?;
    println!("- Stock image has been backup to");
    println!("- {target}");
    Ok(())
}

#[cfg(target_os = "android")]
fn clean_backup(sha1: &str) -> Result<()> {
    println!("- Clean up backup");
    let backup_name = format!("{}{}", KSU_BACKUP_FILE_PREFIX, sha1);
    let dir = std::fs::read_dir(defs::KSU_BACKUP_DIR)?;
    for entry in dir.flatten() {
        let path = entry.path();
        if !path.is_file() {
            continue;
        }
        if let Some(name) = path.file_name() {
            let name = name.to_string_lossy().to_string();
            if name != backup_name
                && name.starts_with(KSU_BACKUP_FILE_PREFIX)
                && std::fs::remove_file(path).is_ok()
            {
                println!("- removed {name}");
            }
        }
    }
    Ok(())
}

fn flash_boot(bootdevice: &Option<String>, new_boot: PathBuf) -> Result<()> {
    let Some(bootdevice) = bootdevice else {
        bail!("boot device not found")
    };
    let status = Command::new("blockdev")
        .arg("--setrw")
        .arg(bootdevice)
        .status()?;
    ensure!(status.success(), "set boot device rw failed");
    dd(new_boot, bootdevice).context("flash boot failed")?;
    Ok(())
}

fn find_magiskboot(magiskboot_path: Option<PathBuf>, workdir: &Path) -> Result<PathBuf> {
    let magiskboot = {
        if which("magiskboot").is_ok() {
            let _ = assets::ensure_binaries(true);
            "magiskboot".into()
        } else {
            // magiskboot is not in $PATH, use builtin or specified one
            let magiskboot = if let Some(magiskboot_path) = magiskboot_path {
                std::fs::canonicalize(magiskboot_path)?
            } else {
                let magiskboot_path = workdir.join("magiskboot");
                assets::copy_assets_to_file("magiskboot", &magiskboot_path)
                    .context("copy magiskboot failed")?;
                magiskboot_path
            };
            ensure!(magiskboot.exists(), "{magiskboot:?} is not exist");
            #[cfg(unix)]
            let _ = std::fs::set_permissions(&magiskboot, std::fs::Permissions::from_mode(0o755));
            magiskboot
        }
    };
    Ok(magiskboot)
}

fn find_boot_image(
    image: &Option<PathBuf>,
    skip_init: bool,
    ota: bool,
    is_replace_kernel: bool,
    workdir: &Path,
) -> Result<(PathBuf, Option<String>)> {
    let bootimage;
    let mut bootdevice = None;
    if let Some(ref image) = *image {
        ensure!(image.exists(), "boot image not found");
        bootimage = std::fs::canonicalize(image)?;
    } else {
        if cfg!(not(target_os = "android")) {
            println!("- Current OS is not android, refusing auto bootimage/bootdevice detection");
            bail!("Please specify a boot image");
        }
        let mut slot_suffix =
            utils::getprop("ro.boot.slot_suffix").unwrap_or_else(|| String::from(""));

        if !slot_suffix.is_empty() && ota {
            if slot_suffix == "_a" {
                slot_suffix = "_b".to_string()
            } else {
                slot_suffix = "_a".to_string()
            }
        };

        let init_boot_exist =
            Path::new(&format!("/dev/block/by-name/init_boot{slot_suffix}")).exists();
        let boot_partition = if !is_replace_kernel && init_boot_exist && !skip_init {
            format!("/dev/block/by-name/init_boot{slot_suffix}")
        } else {
            format!("/dev/block/by-name/boot{slot_suffix}")
        };

        println!("- Bootdevice: {boot_partition}");
        let tmp_boot_path = workdir.join("boot.img");

        dd(&boot_partition, &tmp_boot_path)?;

        ensure!(tmp_boot_path.exists(), "boot image not found");

        bootimage = tmp_boot_path;
        bootdevice = Some(boot_partition);
    };
    Ok((bootimage, bootdevice))
}

fn post_ota() -> Result<()> {
    use crate::defs::ADB_DIR;
    use assets::BOOTCTL_PATH;
    let status = Command::new(BOOTCTL_PATH).arg("hal-info").status()?;
    if !status.success() {
        return Ok(());
    }

    let current_slot = Command::new(BOOTCTL_PATH)
        .arg("get-current-slot")
        .output()?
        .stdout;
    let current_slot = String::from_utf8(current_slot)?;
    let current_slot = current_slot.trim();
    let target_slot = if current_slot == "0" { 1 } else { 0 };

    Command::new(BOOTCTL_PATH)
        .arg(format!("set-active-boot-slot {target_slot}"))
        .status()?;

    let post_fs_data = std::path::Path::new(ADB_DIR).join("post-fs-data.d");
    utils::ensure_dir_exists(&post_fs_data)?;
    let post_ota_sh = post_fs_data.join("post_ota.sh");

    let sh_content = format!(
        r###"
{BOOTCTL_PATH} mark-boot-successful
rm -f {BOOTCTL_PATH}
rm -f /data/adb/post-fs-data.d/post_ota.sh
"###
    );

    std::fs::write(&post_ota_sh, sh_content)?;
    #[cfg(unix)]
    std::fs::set_permissions(post_ota_sh, std::fs::Permissions::from_mode(0o755))?;

    Ok(())
}
