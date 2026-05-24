use anyhow::Result;
use log::{error, info, warn};
use std::fs;
use std::process::Command;

use crate::{ksu_uapi, ksucalls, logger::set_stdio_log_max_level, utils};

/// Find PIDs of processes holding ksu_driver or ksu_fdwrapper file descriptors.
/// Returns a list of PIDs excluding our own.
fn find_ksu_fd_holders() -> Vec<i32> {
    let my_pid = std::process::id() as i32;
    let mut pids = Vec::new();

    let Ok(entries) = fs::read_dir("/proc") else {
        return pids;
    };

    for entry in entries.flatten() {
        let name = entry.file_name();
        let Some(pid) = name.to_str().and_then(|s| s.parse::<i32>().ok()) else {
            continue;
        };
        if pid == my_pid {
            continue;
        }

        let fd_dir = format!("/proc/{pid}/fd");
        let Ok(fds) = fs::read_dir(&fd_dir) else {
            continue;
        };

        for fd_entry in fds.flatten() {
            let link_path = fd_entry.path();
            if let Ok(target) = fs::read_link(&link_path) {
                let target_str = target.to_string_lossy();
                if target_str.contains("[ksu_driver]") || target_str.contains("[ksu_fdwrapper]") {
                    pids.push(pid);
                    break;
                }
            }
        }
    }

    pids
}

fn kill_pids(pids: &[i32], signal: i32) {
    for &pid in pids {
        unsafe {
            libc::kill(pid, signal);
        }
    }
}

/// Close all ksu_driver and ksu_fdwrapper fds held by the current process.
fn close_ksu_fds() {
    let Ok(entries) = fs::read_dir("/proc/self/fd") else {
        return;
    };

    for entry in entries.flatten() {
        let Ok(fd) = entry.file_name().to_string_lossy().parse::<i32>() else {
            continue;
        };
        if let Ok(target) = fs::read_link(entry.path()) {
            let target_str = target.to_string_lossy();
            if target_str.contains("[ksu_driver]") || target_str.contains("[ksu_fdwrapper]") {
                info!("unload: closing fd {fd} -> {target_str}");
                unsafe {
                    libc::close(fd);
                }
            }
        }
    }
}

pub fn unload() -> Result<()> {
    set_stdio_log_max_level(log::LevelFilter::Info);
    if (ksucalls::get_info().flags & ksu_uapi::KSU_GET_INFO_FLAG_UNLOADABLE) == 0 {
        error!("KernelSU is not unloadable!");
        std::process::exit(1);
    }

    if ksucalls::check_wrapper_fd(0).is_ok_and(|x| x)
        || ksucalls::check_wrapper_fd(1).is_ok_and(|x| x)
        || ksucalls::check_wrapper_fd(2).is_ok_and(|x| x)
    {
        error!("Please restart root shell with -W!");
        std::process::exit(1);
    }

    info!("unload: starting KernelSU unload sequence");

    // 0. Switch cgroups so we don't get killed along with our parent shell
    utils::switch_cgroups();

    // 1. stop (Android init stop command - stops all services)
    info!("unload: stopping Android services...");
    let _ = Command::new("stop").status();

    // 2. Kill all processes holding ksu fds (except ourselves)
    // TODO: scan and kill in kernel
    info!("unload: killing processes holding ksu fds...");
    let fd_pids = find_ksu_fd_holders();
    if !fd_pids.is_empty() {
        info!(
            "unload: found {} processes holding ksu fds, sending SIGKILL",
            fd_pids.len()
        );
        kill_pids(&fd_pids, libc::SIGKILL);
    }

    // 3. Close all our own ksu_driver and ksu_fdwrapper fds
    info!("unload: closing all ksu fds...");
    close_ksu_fds();

    // 4. delete_module("kernelsu")
    info!("unload: removing kernelsu module...");
    if let Err(e) = rustix::system::delete_module(c"kernelsu", 0) {
        warn!("unload: delete_module kernelsu failed: {e}");
    }

    // 5. start (Android init start command - restarts all services)
    info!("unload: restarting Android services...");
    let _ = Command::new("start").status();

    // 6. Exit
    info!("unload: done, exiting ksud");
    std::process::exit(0);
}
