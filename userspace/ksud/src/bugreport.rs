use anyhow::Result;
use log::warn;
use std::fmt::Write as FmtWrite;
use std::path::Path;
use std::process::Command;

use crate::{defs, ksucalls, module, utils};

/// Run a shell command, returning stdout as bytes. Failures are logged and return empty.
fn run_cmd(args: &[&str]) -> Vec<u8> {
    let Some((program, cmd_args)) = args.split_first() else {
        return Vec::new();
    };
    match Command::new(program).args(cmd_args).output() {
        Ok(output) => output.stdout,
        Err(e) => {
            warn!("bugreport: failed to run {}: {e}", args.join(" "));
            Vec::new()
        }
    }
}

/// Run a shell command via `sh -c` for pipelines/redirections. Returns stdout.
fn run_sh(cmd: &str) -> Vec<u8> {
    match Command::new("sh").arg("-c").arg(cmd).output() {
        Ok(output) => output.stdout,
        Err(e) => {
            warn!("bugreport: failed to run sh -c '{cmd}': {e}");
            Vec::new()
        }
    }
}

/// Write bytes to a file inside the bugreport directory. Failures are logged.
fn write_file(dir: &Path, name: &str, data: &[u8]) {
    let path = dir.join(name);
    if let Err(e) = std::fs::write(&path, data) {
        warn!("bugreport: failed to write {}: {e}", path.display());
    }
}

/// Copy a file into the bugreport directory. Missing sources are silently skipped.
fn copy_file(dir: &Path, src: &str, dest_name: &str) {
    let src_path = Path::new(src);
    if !src_path.exists() {
        warn!("bugreport: source not found, skipping: {src}");
        return;
    }
    let dest = dir.join(dest_name);
    if let Err(e) = std::fs::copy(src_path, &dest) {
        warn!("bugreport: failed to copy {src} -> {}: {e}", dest.display());
    }
}

/// Tar-compress a directory into the bugreport dir. Missing sources are silently skipped.
fn tar_dir(bugreport_dir: &Path, src_dir: &str, dest_name: &str, extra_args: &[&str]) {
    if !Path::new(src_dir).exists() {
        warn!("bugreport: tar source not found, skipping: {src_dir}");
        return;
    }
    let dest = bugreport_dir.join(dest_name);
    let mut cmd = Command::new("tar");
    cmd.arg("-czf").arg(&dest).arg("-C").arg(src_dir).arg(".");
    for arg in extra_args {
        cmd.arg(arg);
    }
    if let Err(e) = cmd.status() {
        warn!("bugreport: failed to tar {src_dir}: {e}");
    }
}

/// Build basic.txt content with system and KernelSU information.
fn build_basic_info(manager_version: Option<&str>) -> String {
    let mut info = String::new();

    // Kernel info via uname
    let uname = run_cmd(&["uname", "-r"]);
    let kernel_release = String::from_utf8_lossy(&uname).trim().to_string();
    let uname_v = run_cmd(&["uname", "-v"]);
    let kernel_version = String::from_utf8_lossy(&uname_v).trim().to_string();
    let uname_m = run_cmd(&["uname", "-m"]);
    let machine = String::from_utf8_lossy(&uname_m).trim().to_string();
    let uname_n = run_cmd(&["uname", "-n"]);
    let nodename = String::from_utf8_lossy(&uname_n).trim().to_string();
    let uname_s = run_cmd(&["uname", "-s"]);
    let sysname = String::from_utf8_lossy(&uname_s).trim().to_string();

    let _ = writeln!(info, "Kernel: {kernel_release}");

    // Device info from props
    let brand = utils::getprop("ro.product.brand").unwrap_or_default();
    let model = utils::getprop("ro.product.model").unwrap_or_default();
    let product = utils::getprop("ro.product.name").unwrap_or_default();
    let manufacturer = utils::getprop("ro.product.manufacturer").unwrap_or_default();
    let sdk = utils::getprop("ro.build.version.sdk").unwrap_or_default();
    let preview_sdk = utils::getprop("ro.build.version.preview_sdk").unwrap_or_default();
    let fingerprint = utils::getprop("ro.build.fingerprint").unwrap_or_default();
    let device = utils::getprop("ro.product.device").unwrap_or_default();

    let _ = writeln!(info, "BRAND: {brand}");
    let _ = writeln!(info, "MODEL: {model}");
    let _ = writeln!(info, "PRODUCT: {product}");
    let _ = writeln!(info, "MANUFACTURER: {manufacturer}");
    let _ = writeln!(info, "SDK: {sdk}");
    let _ = writeln!(info, "PREVIEW_SDK: {preview_sdk}");
    let _ = writeln!(info, "FINGERPRINT: {fingerprint}");
    let _ = writeln!(info, "DEVICE: {device}");

    // Manager version
    if let Some(ver) = manager_version {
        let _ = writeln!(info, "Manager: {ver}");
    }

    // SELinux
    let selinux = run_cmd(&["getenforce"]);
    let selinux_str = String::from_utf8_lossy(&selinux).trim().to_string();
    let _ = writeln!(info, "SELinux: {selinux_str}");

    let _ = writeln!(info, "KernelRelease: {kernel_release}");
    let _ = writeln!(info, "KernelVersion: {kernel_version}");
    let _ = writeln!(info, "Machine: {machine}");
    let _ = writeln!(info, "Nodename: {nodename}");
    let _ = writeln!(info, "Sysname: {sysname}");

    // KernelSU info
    let ksu_version = ksucalls::get_version();
    let _ = writeln!(info, "KernelSU: {ksu_version}");
    let safe_mode = ksucalls::check_kernel_safemode();
    let _ = writeln!(info, "SafeMode: {safe_mode}");
    let late_load = ksucalls::is_late_load();
    let _ = writeln!(info, "LKM: {late_load}");
    let _ = writeln!(info, "ksud: {}", defs::VERSION_NAME);

    info
}

/// Collect all bugreport data and package into a tar.gz at the given output path.
pub fn collect_bugreport(output: &Path, manager_version: Option<&str>) -> Result<()> {
    let tmp_dir = tempfile::tempdir()?;
    let dir = tmp_dir.path();

    log::info!("bugreport: collecting into {}", dir.display());

    // Process list
    write_file(
        dir,
        "process.txt",
        &run_cmd(&[
            "toybox",
            "ps",
            "-T",
            "-A",
            "-w",
            "-o",
            "PID,TID,UID,COMM,CMDLINE,CMD,LABEL,STAT,WCHAN",
        ]),
    );

    // Kernel log
    write_file(dir, "dmesg.txt", &run_cmd(&["dmesg", "-r"]));

    // System log
    write_file(
        dir,
        "logcat.txt",
        &run_cmd(&["logcat", "-b", "all", "-v", "uid", "-d"]),
    );

    // Compressed archives from system directories
    tar_dir(dir, "/data/tombstones", "tombstones.tar.gz", &[]);
    tar_dir(dir, "/data/system/dropbox", "dropbox.tar.gz", &[]);
    tar_dir(dir, "/sys/fs/pstore", "pstore.tar.gz", &[]);
    tar_dir(
        dir,
        "/data/vendor/diag",
        "diag.tar.gz",
        &["--exclude=./minidump.gz"],
    );
    tar_dir(
        dir,
        "/mnt/oplus/op2/media/log/boot_log",
        "oplus.tar.gz",
        &[],
    );
    tar_dir(dir, defs::LOG_DIR, "bootlog.tar.gz", &[]);

    // System info files
    write_file(
        dir,
        "mounts.txt",
        &std::fs::read("/proc/1/mountinfo").unwrap_or_default(),
    );
    write_file(
        dir,
        "filesystems.txt",
        &std::fs::read("/proc/filesystems").unwrap_or_default(),
    );

    // ADB directory info
    write_file(
        dir,
        "adb_tree.txt",
        &run_cmd(&["busybox", "tree", "/data/adb"]),
    );
    write_file(
        dir,
        "adb_details.txt",
        &run_cmd(&["ls", "-alRZ", "/data/adb"]),
    );
    write_file(dir, "ksu_size.txt", &run_sh("du -sh /data/adb/ksu/*"));

    // Copy system files
    copy_file(dir, "/data/system/packages.list", "packages.txt");
    write_file(dir, "props.txt", &run_cmd(&["getprop"]));
    copy_file(dir, "/data/adb/ksu/.allowlist", "allowlist.bin");
    copy_file(dir, "/proc/modules", "proc_modules.txt");
    copy_file(dir, "/proc/bootconfig", "boot_config.txt");
    copy_file(dir, "/proc/config.gz", "defconfig.gz");

    // Modules list (reuse existing module listing logic)
    let modules = module::list_module_info(defs::MODULE_DIR);
    let modules_json = serde_json::to_string_pretty(&modules).unwrap_or_else(|_| "[]".to_string());
    write_file(dir, "modules.json", modules_json.as_bytes());

    // Basic info
    let basic = build_basic_info(manager_version);
    write_file(dir, "basic.txt", basic.as_bytes());

    // Package everything into the output tar.gz
    let status = Command::new("tar")
        .arg("czf")
        .arg(output)
        .arg("-C")
        .arg(dir)
        .arg(".")
        .status()?;

    if !status.success() {
        anyhow::bail!("bugreport: tar packaging failed with {status}");
    }

    log::info!("bugreport: saved to {}", output.display());
    Ok(())
}
