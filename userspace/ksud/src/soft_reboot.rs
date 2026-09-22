use std::{
    collections::HashSet,
    process::{Command, Output},
};

#[cfg(target_os = "android")]
use std::{
    thread,
    time::{Duration, Instant},
};

use anyhow::{Context, Result, bail, ensure};
#[cfg(target_os = "android")]
use libc::_exit;
#[cfg(target_os = "android")]
use log::{error, info, warn};
#[cfg(target_os = "android")]
use prop_rs_android::{resetprop::ResetProp, sys_prop};
use regex_lite::Regex;
#[cfg(target_os = "android")]
use rustix::process::chdir;

#[cfg(target_os = "android")]
use crate::{
    init_event::{on_boot_completed, on_post_data_fs, on_services, run_stage},
    ksucalls,
    utils::{self, switch_mnt_ns},
};

const SERVICE_PATH: &str = "/system/bin/service";
const GET_SERVICE_PID_TRANSACTION: &str = "1599097156";
const SYSTEM_SERVER_FALLBACK_SERVICES: [&str; 3] = ["activity", "package", "user"];
#[cfg(target_os = "android")]
const SERVICE_POLL_INTERVAL: Duration = Duration::from_millis(200);
#[cfg(target_os = "android")]
const SERVICE_STOP_TIMEOUT: Duration = Duration::from_secs(5);

fn command_stdout(output: Output, description: &str) -> Result<String> {
    ensure!(
        output.status.success(),
        "{description} failed with {}: {}",
        output.status,
        String::from_utf8_lossy(&output.stderr).trim()
    );

    String::from_utf8(output.stdout).with_context(|| format!("{description} output is not UTF-8"))
}

fn parse_service_list(output: &str) -> Result<Vec<String>> {
    let mut lines = output.lines();
    let header = lines.next().context("service list output is empty")?;
    let service_count = header
        .strip_prefix("Found ")
        .and_then(|header| header.strip_suffix(" services:"))
        .context("invalid service list header")?
        .parse::<usize>()
        .context("invalid service count")?;

    let service_pattern = Regex::new(r"^\d+\s+([^\s:]+):\s+\[[^\]]*\]$")?;
    let services = lines
        .filter(|line| !line.trim().is_empty())
        .map(|line| {
            service_pattern
                .captures(line)
                .and_then(|captures| captures.get(1))
                .map(|service| service.as_str().to_owned())
                .with_context(|| format!("invalid service list entry: {line}"))
        })
        .collect::<Result<Vec<_>>>()?;

    ensure!(
        services.len() == service_count,
        "service list declared {service_count} services but contained {}",
        services.len()
    );

    Ok(services)
}

fn parse_service_pid(output: &str) -> Result<u32> {
    let parcel_pattern =
        Regex::new(r"^Result:\s+Parcel\(\s*([0-9A-Fa-f]{8})\s+'[^'\r\n]{4}'\s*\)\s*$")?;
    let Some(pid) = parcel_pattern
        .captures(output)
        .and_then(|captures| captures.get(1))
    else {
        bail!("invalid service PID response: {}", output.trim());
    };

    u32::from_str_radix(pid.as_str(), 16).context("invalid PID in service response")
}

pub fn list_services() -> Result<Vec<String>> {
    let output = Command::new(SERVICE_PATH)
        .arg("list")
        .output()
        .context("failed to execute service list")?;
    let stdout = command_stdout(output, "service list")?;
    parse_service_list(&stdout)
}

pub fn get_service_pid(service: &str) -> Result<u32> {
    let output = Command::new(SERVICE_PATH)
        .args(["call", service, GET_SERVICE_PID_TRANSACTION])
        .output()
        .with_context(|| format!("failed to query service {service}"))?;
    let stdout = command_stdout(output, "service PID query")?;
    parse_service_pid(&stdout)
}

fn is_fallback_service(service: &str) -> bool {
    SYSTEM_SERVER_FALLBACK_SERVICES.contains(&service)
}

fn find_system_server_services<F>(services: Vec<String>, mut get_pid: F) -> Vec<String>
where
    F: FnMut(&str) -> Option<u32>,
{
    let Some(activity_pid) = get_pid("activity") else {
        return services
            .into_iter()
            .filter(|service| is_fallback_service(service))
            .collect();
    };

    services
        .into_iter()
        .filter(|service| {
            if service == "activity" {
                return true;
            }

            get_pid(service).map_or_else(|| is_fallback_service(service), |pid| pid == activity_pid)
        })
        .collect()
}

#[cfg(target_os = "android")]
fn collect_system_server_services() -> Result<Vec<String>> {
    let services = list_services()?;
    let system_server_services =
        find_system_server_services(services, |service| match get_service_pid(service) {
            Ok(pid) => Some(pid),
            Err(err) => {
                log::debug!("failed to get PID for service {service}: {err:#}");
                None
            }
        });

    info!(
        "tracking {} system_server services",
        system_server_services.len()
    );
    Ok(system_server_services)
}

fn remaining_services(tracked: &[String], running: &[String]) -> Vec<String> {
    let running: HashSet<&str> = running.iter().map(String::as_str).collect();
    tracked
        .iter()
        .filter(|service| running.contains(service.as_str()))
        .cloned()
        .collect()
}

#[cfg(target_os = "android")]
fn wait_for_system_server_services(system_server_services: &[String]) {
    if system_server_services.is_empty() {
        return;
    }

    let deadline = Instant::now() + SERVICE_STOP_TIMEOUT;
    let mut remaining = system_server_services.to_vec();
    let mut list_error_logged = false;

    loop {
        match list_services() {
            Ok(running) => remaining = remaining_services(system_server_services, &running),
            Err(err) if !list_error_logged => {
                warn!("failed to list services while waiting for system_server: {err:#}");
                list_error_logged = true;
            }
            Err(_) => {}
        }

        if remaining.is_empty() {
            info!("system_server services stopped");
            return;
        }

        let remaining_time = deadline.saturating_duration_since(Instant::now());
        if remaining_time.is_zero() {
            warn!(
                "timed out waiting for system_server services to stop: {}",
                remaining.join(", ")
            );
            return;
        }

        thread::sleep(SERVICE_POLL_INTERVAL.min(remaining_time));
    }
}

#[cfg(target_os = "android")]
const fn resetprop() -> ResetProp {
    ResetProp {
        skip_svc: true,
        persistent: false,
        persist_only: false,
        verbose: false,
        show_context: false,
        rebuild: false,
    }
}

#[cfg(target_os = "android")]
fn reset_boot_completed() -> Result<()> {
    sys_prop::init().context("Failed to initialize system property API")?;
    let rp = resetprop();
    // Set prop value to 0 in advance to ensure resetprop -w works
    info!("reset boot complete prop to 0");
    rp.set("sys.boot_completed", "0")
        .context("Failed to set sys.boot_completed to 0")?;
    Ok(())
}

#[cfg(target_os = "android")]
fn wait_for_boot_completed() -> Result<()> {
    sys_prop::init().context("Failed to initialize system property API")?;
    let rp = resetprop();
    info!("waiting for boot complete");
    rp.wait("sys.boot_completed", Some("0"), None)
        .context("wait for sys.boot_completed failed")?;
    Ok(())
}

#[cfg(target_os = "android")]
pub fn soft_reboot() -> Result<()> {
    // check it avoid user click "soft_reboot" in manager when version mismatch
    if let Err(e) = ksucalls::ensure_uapi_version_matched() {
        error!("{e:#}, skip soft_reboot");
        return Ok(());
    }

    utils::daemonize_with(true, || -> Result<()> {
        switch_mnt_ns(1)?;
        chdir("/")?;
        Ok(())
    })?;

    info!("emulating soft_reboot!");
    if let Err(e) = reset_boot_completed() {
        warn!("reset boot completed failed: {e}");
    }
    run_stage("emulated-soft-reboot", true);

    let system_server_services = collect_system_server_services()?;

    info!("stop");
    let status = Command::new("stop").status().context("stop failed")?;
    if !status.success() {
        warn!("stop exited with status: {status}");
    }

    wait_for_system_server_services(&system_server_services);

    info!("post-fs-data");
    on_post_data_fs()?;
    info!("start");
    let status = Command::new("start").status().context("start failed")?;
    if !status.success() {
        warn!("start exited with status: {status}");
    }
    info!("services");
    on_services();
    if let Err(e) = wait_for_boot_completed() {
        warn!("wait for boot completed failed: {e}");
    }
    on_boot_completed();

    unsafe {
        _exit(0);
    }
}

#[cfg(test)]
mod tests {
    use std::collections::HashMap;

    use super::{
        find_system_server_services, parse_service_list, parse_service_pid, remaining_services,
    };

    #[test]
    fn parses_service_names() {
        let output = "Found 4 services:\n\
0\tAmlConnectivityService: [android.net.IAmlConnectivityManager]\n\
1\tDfcNativeService: [miui.dfc.IDfc]\n\
2\tDockObserver: []\n\
3\tFramerateFineControlService: [com.miui.IFramerateFineController]\n";

        assert_eq!(
            parse_service_list(output).unwrap(),
            [
                "AmlConnectivityService",
                "DfcNativeService",
                "DockObserver",
                "FramerateFineControlService"
            ]
        );
    }

    #[test]
    fn rejects_incorrect_service_count() {
        let output = "Found 2 services:\n0\tDockObserver: []\n";

        assert!(parse_service_list(output).is_err());
    }

    #[test]
    fn parses_service_pid() {
        assert_eq!(
            parse_service_pid("Result: Parcel( 00000e08    '....')\n").unwrap(),
            3592
        );
    }

    #[test]
    fn rejects_error_parcel() {
        let output = "Result: Parcel(Error: 0xffffffffffffffb6 \"Not a data message\")\n";

        assert!(parse_service_pid(output).is_err());
    }

    #[test]
    fn finds_services_hosted_by_system_server() {
        let services = ["activity", "audio", "package", "surfaceflinger"]
            .map(str::to_owned)
            .to_vec();
        let pids = HashMap::from([
            ("activity", 100),
            ("audio", 100),
            ("package", 100),
            ("surfaceflinger", 200),
        ]);

        assert_eq!(
            find_system_server_services(services, |service| pids.get(service).copied()),
            ["activity", "audio", "package"]
        );
    }

    #[test]
    fn falls_back_to_core_services_when_pid_lookup_fails() {
        let services = ["activity", "audio", "package", "user"]
            .map(str::to_owned)
            .to_vec();

        assert_eq!(
            find_system_server_services(services, |_| None),
            ["activity", "package", "user"]
        );
    }

    #[test]
    fn reports_only_services_that_are_still_running() {
        let tracked = ["activity", "audio", "package"].map(str::to_owned);
        let running = ["audio", "surfaceflinger"].map(str::to_owned);

        assert_eq!(remaining_services(&tracked, &running), ["audio"]);
    }
}
