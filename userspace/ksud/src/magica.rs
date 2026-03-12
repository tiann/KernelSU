use crate::assets;
use adb_client::ADBDeviceExt;
use adb_client::tcp::ADBTcpDevice;
use anyhow::{Context, Result, bail};
use log::{error, info};
use std::net::{IpAddr, Ipv4Addr, SocketAddr};
use std::path::Path;
use std::process::Command;

fn enable_adb_root(port: u16) -> Result<()> {
    // We are in limited root by magica
    anyhow::ensure!(
        rustix::process::getuid().as_raw() == 0,
        "must be run as root"
    );

    let resetprop_path = "/dev/resetprop";
    info!("Extracting resetprop to {resetprop_path}");
    assets::copy_assets_to_file("resetprop", resetprop_path)?;
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        std::fs::set_permissions(resetprop_path, std::fs::Permissions::from_mode(0o755))?;
    }

    let debuggable_context = {
        let output = Command::new(resetprop_path)
            .args(["-Z", "ro.debuggable"])
            .output()
            .context("Failed to run resetprop -Z ro.debuggable")?;
        String::from_utf8_lossy(&output.stdout).trim().to_string()
    };
    info!("ro.debuggable context: {debuggable_context}");

    let adb_secure_context = {
        let output = Command::new(resetprop_path)
            .args(["-Z", "ro.adb.secure"])
            .output()
            .context("Failed to run resetprop -Z ro.adb.secure")?;
        String::from_utf8_lossy(&output.stdout).trim().to_string()
    };
    info!("ro.adb.secure context: {adb_secure_context}");

    let props_serial = "/dev/__properties__/properties_serial";
    let debuggable_context = format!("/dev/__properties__/{debuggable_context}");
    let adb_secure_context = format!("/dev/__properties__/{adb_secure_context}");
    let port_str = port.to_string();

    let commands: &[(&str, &[&str])] = &[
        ("chmod", &["0644", props_serial]),
        ("chmod", &["0644", &debuggable_context]),
        ("chmod", &["0644", &adb_secure_context]),
        (resetprop_path, &["-n", "ro.debuggable", "1"]),
        (resetprop_path, &["-n", "ro.adb.secure", "0"]),
        ("chmod", &["0444", props_serial]),
        ("chmod", &["0444", &debuggable_context]),
        ("chmod", &["0444", &adb_secure_context]),
        ("setprop", &["service.adb.root", "1"]),
        ("setprop", &["service.adb.tcp.port", &port_str]),
        ("setprop", &["ctl.restart", "adbd"]),
    ];

    for (cmd, args) in commands {
        info!("Executing: {cmd} {}", args.join(" "));
        let status = Command::new(cmd)
            .args(*args)
            .status()
            .with_context(|| format!("Failed to execute {cmd}"))?;
        if !status.success() {
            bail!("{cmd} {} exited with {status}", args.join(" "));
        }
    }

    Ok(())
}

pub fn disable_adb_root() -> Result<()> {
    // We have full root now, no need to chmod
    let resetprop_path = "/dev/resetprop";

    let commands: &[(&str, &[&str])] = &[
        (resetprop_path, &["-n", "ro.debuggable", "0"]),
        (resetprop_path, &["-n", "ro.adb.secure", "1"]),
        (resetprop_path, &["--delete", "service.adb.root"]),
        (resetprop_path, &["--delete", "service.adb.tcp.port"]),
        ("rm", &[resetprop_path]),
        ("setprop", &["ctl.restart", "adbd"]),
    ];

    for (cmd, args) in commands {
        info!("Restoring: {cmd} {}", args.join(" "));
        let status = Command::new(cmd)
            .args(*args)
            .status()
            .with_context(|| format!("Failed to execute {cmd}"))?;
        if !status.success() {
            bail!("{cmd} {} exited with {status}", args.join(" "));
        }
    }

    Ok(())
}

fn connect_to_device(port: u16) -> Result<ADBTcpDevice> {
    const MAX_RETRIES: u32 = 30;
    for attempt in 1..=MAX_RETRIES {
        info!("Waiting for adbd to restart... (attempt {attempt}/{MAX_RETRIES})");
        std::thread::sleep(std::time::Duration::from_secs(1));

        let addr = SocketAddr::new(IpAddr::V4(Ipv4Addr::LOCALHOST), port);
        info!("Connecting to ADB device at {addr}");
        unsafe { std::env::set_var("HOME", "/dev") };
        match ADBTcpDevice::new_with_custom_private_key(addr, Path::new("/dev/adbkey"))
            .context("Failed to create ADBTcpDevice")
        {
            Ok(device) => return Ok(device),
            Err(e) => {
                error!("Failed to connect to ADB device: {e:?}, retry after 1s");
            }
        }
    }
    bail!("Failed to connect to ADB device after {MAX_RETRIES} attempts")
}

pub fn run(port: u16) -> Result<()> {
    enable_adb_root(port)?;

    let mut device = connect_to_device(port)?;

    let self_path = std::env::current_exe().context("Failed to get self exe path")?;

    // Execute late-load with --post-magica via adb shell.
    // The late-load process has full root + su domain and will:
    // 1. Load kernelsu.ko, enforce SELinux, run stage scripts
    // 2. Restore adb properties (disable adb root/tcp mode)
    let cmd = format!("{} late-load --post-magica", self_path.display());
    info!("Executing '{cmd}' via adb shell...");
    let mut stdout = Vec::new();
    let mut stderr = Vec::new();
    if let Err(e) = device.shell_command(&cmd, Some(&mut stdout), Some(&mut stderr)) {
        info!("adb shell finished with error (may be expected): {e}");
    }
    if !stdout.is_empty() {
        info!("stdout: {}", String::from_utf8_lossy(&stdout));
    }
    if !stderr.is_empty() {
        info!("stderr: {}", String::from_utf8_lossy(&stderr));
    }

    Ok(())
}
