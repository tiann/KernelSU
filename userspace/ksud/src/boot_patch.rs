#[cfg(unix)]
use std::os::unix::fs::PermissionsExt;

use anyhow::anyhow;
use anyhow::bail;
use anyhow::ensure;
use anyhow::Context;
use anyhow::Result;
use std::path::Path;
use std::path::PathBuf;
use std::process::Command;
use std::process::Stdio;
use which::which;

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
    use regex::Regex;
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
    use regex::Regex;
    let re = Regex::new(r"(.* )?(\d+\.\d+)(\S+)?(android\d+)(.*)")?;
    let cap = re
        .captures(version)
        .ok_or_else(|| anyhow::anyhow!("No match found"))?;
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
    anyhow::bail!("Unknown KMI, try use --kmi to specify it.")
}

#[cfg(target_os = "android")]
fn get_kmi() -> Result<String> {
    parse_kmi_from_uname().or_else(|_| parse_kmi_from_modules())
}

#[cfg(not(target_os = "android"))]
fn get_kmi() -> Result<String> {
    bail!("Unknown KMI, try use --kmi to specify it.")
}

fn do_cpio_cmd(magiskboot: &Path, workding_dir: &Path, cmd: &str) -> Result<()> {
    let status = Command::new(magiskboot)
        .current_dir(workding_dir)
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .arg("cpio")
        .arg("ramdisk.cpio")
        .arg(cmd)
        .status()?;

    ensure!(status.success(), "magiskboot cpio {} failed", cmd);
    Ok(())
}

fn is_magisk_patched(magiskboot: &Path, workding_dir: &Path) -> Result<bool> {
    let status = Command::new(magiskboot)
        .current_dir(workding_dir)
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .args(["cpio", "ramdisk.cpio", "test"])
        .status()?;

    // 0: stock, 1: magisk
    Ok(status.code() == Some(1))
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

    if image.is_none() {
        #[cfg(target_os = "android")]
        ensure_gki_kernel()?;
    }

    let is_replace_kernel = kernel.is_some();

    if is_replace_kernel {
        ensure!(
            init.is_none() && kmod.is_none(),
            "init and module must not be specified."
        );
    }

    let workding_dir =
        tempdir::TempDir::new("KernelSU").with_context(|| "create temp dir failed")?;

    let bootimage;

    let mut bootdevice = None;

    if let Some(ref image) = image {
        ensure!(image.exists(), "boot image not found");
        bootimage = std::fs::canonicalize(image)?;
    } else {
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
        let boot_partition = if !is_replace_kernel && init_boot_exist {
            format!("/dev/block/by-name/init_boot{slot_suffix}")
        } else {
            format!("/dev/block/by-name/boot{slot_suffix}")
        };

        println!("- Bootdevice: {boot_partition}");
        let tmp_boot_path = workding_dir.path().join("boot.img");

        dd(&boot_partition, &tmp_boot_path)?;

        ensure!(tmp_boot_path.exists(), "boot image not found");

        bootimage = tmp_boot_path;
        bootdevice = Some(boot_partition);
    };

    // try extract magiskboot/bootctl
    let _ = assets::ensure_binaries(false);

    // extract magiskboot
    let magiskboot = {
        if which("magiskboot").is_ok() {
            let _ = assets::ensure_binaries(true);
            "magiskboot".into()
        } else {
            // magiskboot is not in $PATH, use builtin or specified one
            let magiskboot = if let Some(magiskboot_path) = magiskboot_path {
                std::fs::canonicalize(magiskboot_path)?
            } else {
                let magiskboot_path = workding_dir.path().join("magiskboot");
                assets::copy_assets_to_file("magiskboot", &magiskboot_path)
                    .with_context(|| "copy magiskboot failed")?;
                magiskboot_path
            };
            ensure!(magiskboot.exists(), "{magiskboot:?} is not exist");
            #[cfg(unix)]
            let _ = std::fs::set_permissions(&magiskboot, std::fs::Permissions::from_mode(0o755));
            magiskboot
        }
    };

    if let Some(kernel) = kernel {
        std::fs::copy(kernel, workding_dir.path().join("kernel"))
            .with_context(|| "copy kernel from failed".to_string())?;
    }

    println!("- Preparing assets");

    let kmod_file = workding_dir.path().join("kernelsu.ko");
    if let Some(kmod) = kmod {
        std::fs::copy(kmod, kmod_file).with_context(|| "copy kernel module failed".to_string())?;
    } else {
        // If kmod is not specified, extract from assets
        let kmi = if let Some(kmi) = kmi { kmi } else { get_kmi()? };
        println!("- KMI: {kmi}");
        let name = format!("{kmi}_kernelsu.ko");
        assets::copy_assets_to_file(&name, kmod_file)
            .with_context(|| format!("Failed to copy {name}"))?;
    };

    let init_file = workding_dir.path().join("init");
    if let Some(init) = init {
        std::fs::copy(init, init_file).with_context(|| "copy init failed".to_string())?;
    } else {
        assets::copy_assets_to_file("ksuinit", init_file).with_context(|| "copy ksuinit failed")?;
    }

    // magiskboot unpack boot.img
    // magiskboot cpio ramdisk.cpio 'cp init init.real'
    // magiskboot cpio ramdisk.cpio 'add 0755 ksuinit init'
    // magiskboot cpio ramdisk.cpio 'add 0755 <kmod> kernelsu.ko'

    println!("- Unpacking boot image");
    let status = Command::new(&magiskboot)
        .current_dir(workding_dir.path())
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .arg("unpack")
        .arg(bootimage.display().to_string())
        .status()?;
    ensure!(status.success(), "magiskboot unpack failed");

    let no_ramdisk = !workding_dir.path().join("ramdisk.cpio").exists();
    let is_magisk_patched = is_magisk_patched(&magiskboot, workding_dir.path())?;
    ensure!(
        no_ramdisk || !is_magisk_patched,
        "Cannot work with Magisk patched image"
    );

    println!("- Adding KernelSU LKM");
    let is_kernelsu_patched =
        do_cpio_cmd(&magiskboot, workding_dir.path(), "exists kernelsu.ko").is_ok();
    if !is_kernelsu_patched {
        // kernelsu.ko is not exist, backup init if necessary
        let status = do_cpio_cmd(&magiskboot, workding_dir.path(), "exists init");
        if status.is_ok() {
            do_cpio_cmd(&magiskboot, workding_dir.path(), "mv init init.real")?;
        }
    }

    do_cpio_cmd(&magiskboot, workding_dir.path(), "add 0755 init init")?;
    do_cpio_cmd(
        &magiskboot,
        workding_dir.path(),
        "add 0755 kernelsu.ko kernelsu.ko",
    )?;

    println!("- Repacking boot image");
    // magiskboot repack boot.img
    let status = Command::new(&magiskboot)
        .current_dir(workding_dir.path())
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .arg("repack")
        .arg(bootimage.display().to_string())
        .status()?;
    ensure!(status.success(), "magiskboot repack failed");
    let new_boot = workding_dir.path().join("new-boot.img");

    if image.is_some() {
        // if image is specified, write to output file
        let output_dir = out.unwrap_or(std::env::current_dir()?);
        let now = chrono::Utc::now();
        let output_image =
            output_dir.join(format!("kernelsu_boot_{}.img", now.format("%Y%m%d_%H%M%S")));

        if std::fs::rename(&new_boot, &output_image).is_err() {
            std::fs::copy(&new_boot, &output_image)
                .with_context(|| "copy out new boot failed".to_string())?;
        }
        println!("- Output file is written to");
        println!("- {}", output_image.display().to_string().trim_matches('"'));
    }

    if flash {
        println!("- Flashing new boot image");
        let Some(bootdevice) = bootdevice else {
            bail!("boot device not found")
        };
        let status = Command::new("blockdev")
            .arg("--setrw")
            .arg(&bootdevice)
            .status()?;
        ensure!(status.success(), "set boot device rw failed");

        dd(&new_boot, &bootdevice).with_context(|| "flash boot failed")?;

        if ota {
            post_ota()?;
        }
    }

    println!("- Done!");
    Ok(())
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
