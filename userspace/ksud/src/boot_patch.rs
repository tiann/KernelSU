#[cfg(unix)]
use std::os::unix::fs::PermissionsExt;

use anyhow::bail;
use anyhow::ensure;
use anyhow::Context;
use anyhow::Result;
use is_executable::IsExecutable;
use std::path::Path;
use std::path::PathBuf;
use std::process::Command;
use std::process::Stdio;

use crate::utils;

#[cfg(unix)]
fn ensure_gki_kernel() -> Result<()> {
    let version =
        procfs::sys::kernel::Version::current().with_context(|| "get kernel version failed")?;
    let is_gki = version.major == 5 && version.minor >= 10 || version.major > 5;
    ensure!(is_gki, "only support GKI kernel");
    Ok(())
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
    magiskboot_path: Option<PathBuf>,
) -> Result<()> {
    if image.is_none() {
        #[cfg(unix)]
        ensure_gki_kernel()?;
    }

    let is_replace_kernel = kernel.is_some();

    if is_replace_kernel {
        ensure!(
            init.is_none() && kmod.is_none(),
            "init and module must not be specified."
        );
    } else {
        ensure!(
            init.is_some() && kmod.is_some(),
            "init and module must be specified"
        );
    }

    let workding_dir = tempdir::TempDir::new("KernelSU")?;

    let bootimage;

    let mut bootdevice = None;

    if let Some(image) = image {
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

        println!("bootdevice: {boot_partition}");
        let tmp_boot_path = workding_dir.path().join("boot.img");

        dd(&boot_partition, &tmp_boot_path)?;

        ensure!(tmp_boot_path.exists(), "boot image not found");

        bootimage = tmp_boot_path;
        bootdevice = Some(boot_partition);
    };

    println!("boot image: {bootimage:?}");

    let magiskboot = magiskboot_path
        .map(std::fs::canonicalize)
        .transpose()?
        .unwrap_or_else(|| "magiskboot".into());

    if !magiskboot.is_executable() {
        #[cfg(unix)]
        std::fs::set_permissions(&magiskboot, std::fs::Permissions::from_mode(0o755))
            .with_context(|| "set magiskboot executable failed".to_string())?;
    }

    ensure!(magiskboot.exists(), "magiskboot not found");

    if let Some(kernel) = kernel {
        std::fs::copy(kernel, workding_dir.path().join("kernel"))
            .with_context(|| "copy kernel from failed".to_string())?;
    }

    if let (Some(kmod), Some(init)) = (kmod, init) {
        std::fs::copy(kmod, workding_dir.path().join("kernelsu.ko"))
            .with_context(|| "copy kernel module failed".to_string())?;
        std::fs::copy(init, workding_dir.path().join("init"))
            .with_context(|| "copy init failed".to_string())?;

        // magiskboot unpack boot.img
        // magiskboot cpio ramdisk.cpio 'cp init init.real'
        // magiskboot cpio ramdisk.cpio 'add 0755 ksuinit init'
        // magiskboot cpio ramdisk.cpio 'add 0755 <kmod> kernelsu.ko'

        let status = Command::new(&magiskboot)
            .current_dir(workding_dir.path())
            .stdout(Stdio::null())
            .stderr(Stdio::null())
            .arg("unpack")
            .arg(bootimage.display().to_string())
            .status()?;
        ensure!(status.success(), "magiskboot unpack failed");

        let status = do_cpio_cmd(&magiskboot, workding_dir.path(), "exists init");
        if status.is_ok() {
            // init exist, backup it.
            do_cpio_cmd(&magiskboot, workding_dir.path(), "mv init init.real")?;
        }

        do_cpio_cmd(&magiskboot, workding_dir.path(), "add 0755 init init")?;
        do_cpio_cmd(
            &magiskboot,
            workding_dir.path(),
            "add 0755 kernelsu.ko kernelsu.ko",
        )?;
    }

    // magiskboot repack boot.img
    let status = Command::new(&magiskboot)
        .current_dir(workding_dir.path())
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .arg("repack")
        .arg(bootimage.display().to_string())
        .status()?;
    ensure!(status.success(), "magiskboot repack failed");

    let out = out.unwrap_or(std::env::current_dir()?);

    let now = chrono::Utc::now();
    let output_image = out.join(format!(
        "kernelsu_patched_boot_{}.img",
        now.format("%Y%m%d_%H%M%S")
    ));
    std::fs::copy(workding_dir.path().join("new-boot.img"), &output_image)
        .with_context(|| "copy out new boot failed".to_string())?;

    if flash {
        let Some(bootdevice) = bootdevice else {
            bail!("boot device not found")
        };
        let status = Command::new("blockdev")
            .arg("--setrw")
            .arg(&bootdevice)
            .status()?;
        ensure!(status.success(), "set boot device rw failed");

        dd(&output_image, &bootdevice).with_context(|| "flash boot failed")?;
    }
    Ok(())
}
