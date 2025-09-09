use anyhow::{Result, Context};
use serde_json::json;

#[cfg(any(target_os = "linux", target_os = "android"))]
fn check_loop() -> Result<()> {
    loopdev::LoopControl::open().context("open loop-control failed")?;
    Ok(())
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
fn check_loop() -> Result<()> { Ok(()) }

#[cfg(any(target_os = "linux", target_os = "android"))]
fn check_fsopen_overlay() -> Result<()> {
    use rustix::mount::{fsopen, FsOpenFlags};
    let _ = fsopen("overlay", FsOpenFlags::FSOPEN_CLOEXEC)
        .context("fsopen overlay not supported")?;
    Ok(())
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
fn check_fsopen_overlay() -> Result<()> { Ok(()) }

/// Check if this modsys implementation is supported
pub fn check_supported() -> Result<()> {
    let mut code = 0;
    let mut msg = "Supported".to_string();

    if let Err(e) = check_loop() {
        code = 1;
        msg = format!("Loop device not available: {e:#}");
    } else if let Err(e) = check_fsopen_overlay() {
        code = 2;
        msg = format!("overlay fsopen not available: {e:#}");
    }

    let response = json!({ "code": code, "msg": msg });
    println!("{}", response);
    if code == 0 { Ok(()) } else { anyhow::bail!(response.to_string()) }
}
