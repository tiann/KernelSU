use anyhow::{bail, Context, Result};
use log::info;
use std::process::Command;

use crate::assets;

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

    let context = {
        let output = Command::new(resetprop_path)
            .args(["-Z", "ro.debuggable"])
            .output()
            .context("Failed to run resetprop -Z ro.debuggable")?;
        String::from_utf8_lossy(&output.stdout).trim().to_string()
    };
    info!("ro.debuggable context: {context}");

    let props_serial = "/dev/__properties__/properties_serial";
    let props_context = format!("/dev/__properties__/{context}");
    let port_str = port.to_string();

    let commands: &[(&str, &[&str])] = &[
        ("chmod", &["0644", props_serial]),
        ("chmod", &["0644", &props_context]),
        (resetprop_path, &["-n", "ro.debuggable", "1"]),
        ("chmod", &["0444", props_serial]),
        ("chmod", &["0444", &props_context]),
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

fn disable_adb_root() -> Result<()> {
    // We have full root now, no need to chmod
    let resetprop_path = "/dev/resetprop";

    let commands: &[(&str, &[&str])] = &[
        (resetprop_path, &["-n", "ro.debuggable", "0"]),
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

pub fn run(port: u16) -> Result<()> {
    enable_adb_root(port)?;

    info!("Waiting for adbd to restart...");
    std::thread::sleep(std::time::Duration::from_secs(3));

    let self_path = std::env::current_exe().context("Failed to get self exe path")?;
    let cmd = format!("{} late-load", self_path.display());
    info!("Executing '{cmd}'...");
    let status = Command::new(&self_path)
        .arg("late-load")
        .status()
        .context("Failed to execute ksud late-load")?;

    if status.success() {
        info!("ksud late-load completed successfully");
    } else {
        info!("ksud late-load exited with {status}");
    }

    info!("Restoring adb properties...");
    disable_adb_root()?;

    Ok(())
}
