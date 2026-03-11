use anyhow::{Context, Result};
use log::info;
use std::net::{Ipv4Addr, SocketAddrV4};

use adb_client::server::ADBServer;
use adb_client::ADBDeviceExt;

pub fn run(port: u16) -> Result<()> {
    let addr = SocketAddrV4::new(Ipv4Addr::LOCALHOST, port);
    info!("Connecting to ADB server at {addr}");

    let mut server = ADBServer::new(addr);
    let mut device = server
        .get_device()
        .context("Failed to get ADB device")?;

    let self_path = std::env::current_exe().context("Failed to get self exe path")?;
    let cmd = format!("{} late-load", self_path.display());
    info!("Executing '{cmd}' via adb shell...");
    let mut stdout = Vec::new();
    let mut stderr = Vec::new();
    let status = device
        .shell_command(
            &cmd,
            Some(&mut stdout),
            Some(&mut stderr),
        )
        .context("Failed to execute ksud late-load via adb shell")?;

    if !stdout.is_empty() {
        let out = String::from_utf8_lossy(&stdout);
        info!("stdout: {out}");
    }
    if !stderr.is_empty() {
        let err = String::from_utf8_lossy(&stderr);
        info!("stderr: {err}");
    }

    match status {
        Some(0) => info!("ksud late-load completed successfully"),
        Some(code) => anyhow::bail!("ksud late-load exited with code {code}"),
        None => info!("ksud late-load completed (no exit code)"),
    }

    Ok(())
}
