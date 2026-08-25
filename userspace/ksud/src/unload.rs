use anyhow::{Context, Result, anyhow, ensure};
use log::{info, warn};
use std::fs;
use std::os::fd::AsRawFd;
use std::process::Command;
use std::thread;
use std::time::Duration;

use crate::{ksucalls, utils};

const PREPARE_UNLOAD_RETRIES: usize = 20;
const DELETE_MODULE_RETRIES: usize = 20;
const UNLOAD_RETRY_DELAY: Duration = Duration::from_millis(50);
const ZYGOTE_STOP_RETRIES: usize = 100;
const ZYGOTE_STOP_RETRY_DELAY: Duration = Duration::from_millis(50);

fn find_su_domain_pids() -> Vec<i32> {
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

        let attr_path = format!("/proc/{pid}/attr/current");
        if let Ok(context) = fs::read_to_string(&attr_path) {
            let context = context.trim().trim_end_matches('\0');
            if context == "u:r:ksu:s0" {
                pids.push(pid);
            }
        }
    }

    pids
}

fn is_ksu_fd_target(target: &str) -> bool {
    target.contains("[ksu_driver]")
        || target.contains("[ksu_fdwrapper]")
        || target.contains("[ksu_sulog]")
}

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
            if let Ok(target) = fs::read_link(&link_path)
                && is_ksu_fd_target(&target.to_string_lossy())
            {
                pids.push(pid);
                break;
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

fn close_fd(fd: i32) {
    if fd >= 0 {
        unsafe {
            libc::close(fd);
        }
    }
}

fn close_ksu_fds_except(keep_fd: Option<i32>) {
    let Ok(entries) = fs::read_dir("/proc/self/fd") else {
        return;
    };

    for entry in entries.flatten() {
        let Ok(fd) = entry.file_name().to_string_lossy().parse::<i32>() else {
            continue;
        };
        if keep_fd == Some(fd) {
            continue;
        }

        if let Ok(target) = fs::read_link(entry.path()) {
            let target_str = target.to_string_lossy();
            if !is_ksu_fd_target(&target_str) {
                continue;
            }

            info!("unload: closing fd {fd} -> {target_str}");
            close_fd(fd);
        }
    }
}

fn run_android_service_control(command: &str) -> Result<()> {
    let status = Command::new(command)
        .status()
        .with_context(|| format!("failed to execute Android {command}"))?;
    ensure!(status.success(), "Android {command} exited with {status}");
    Ok(())
}

fn zygote_services_stopped() -> bool {
    matches!(
        utils::getprop("init.svc.zygote").as_deref(),
        Some("stopped")
    ) && utils::getprop("init.svc.zygote_secondary").is_none_or(|state| state == "stopped")
}

fn wait_for_zygote_services_stopped() -> Result<()> {
    for _ in 0..ZYGOTE_STOP_RETRIES {
        if zygote_services_stopped() {
            return Ok(());
        }
        thread::sleep(ZYGOTE_STOP_RETRY_DELAY);
    }

    Err(anyhow!(
        "zygote services did not stop before KernelSU unload teardown"
    ))
}

fn prepare_unload_with_retry(fd: i32) -> std::io::Result<()> {
    for attempt in 0..PREPARE_UNLOAD_RETRIES {
        match ksucalls::prepare_unload_on(fd) {
            Ok(()) => return Ok(()),
            Err(err)
                if err.raw_os_error() == Some(libc::EBUSY)
                    && attempt + 1 < PREPARE_UNLOAD_RETRIES =>
            {
                thread::sleep(UNLOAD_RETRY_DELAY);
            }
            Err(err) => return Err(err),
        }
    }

    unreachable!()
}

fn commit_unload_with_retry(fd: i32) -> std::io::Result<()> {
    for attempt in 0..PREPARE_UNLOAD_RETRIES {
        match ksucalls::commit_unload_on(fd) {
            Ok(()) => return Ok(()),
            Err(err)
                if err.raw_os_error() == Some(libc::EBUSY)
                    && attempt + 1 < PREPARE_UNLOAD_RETRIES =>
            {
                thread::sleep(UNLOAD_RETRY_DELAY);
            }
            Err(err) => return Err(err),
        }
    }

    unreachable!()
}

fn delete_module_with_retry() -> Result<()> {
    for attempt in 0..DELETE_MODULE_RETRIES {
        match rustix::system::delete_module(c"kernelsu", libc::O_NONBLOCK) {
            Ok(()) => return Ok(()),
            Err(err) if err == rustix::io::Errno::NOENT => return Ok(()),
            Err(err)
                if (err == rustix::io::Errno::AGAIN || err == rustix::io::Errno::BUSY)
                    && attempt + 1 < DELETE_MODULE_RETRIES =>
            {
                thread::sleep(UNLOAD_RETRY_DELAY);
            }
            Err(err) => return Err(err).context("delete_module kernelsu failed"),
        }
    }

    unreachable!()
}

fn abort_unload_with_fd(fd: i32) -> Result<()> {
    ksucalls::abort_unload_on(fd).context("abort KernelSU unload failed")
}

fn recover_after_delete_failure() -> Result<()> {
    let recovery_fd =
        ksucalls::install_driver_fd().context("KernelSU recovery driver fd unavailable")?;
    let result = abort_unload_with_fd(recovery_fd.as_raw_fd());

    // Keep the recovery descriptor owned by the shared cache regardless of the
    // abort result. It pins a still-loaded module and prevents a stale raw-fd
    // cache if the caller continues running after this failed unload attempt.
    ksucalls::restore_driver_fd(recovery_fd);
    result
}

pub fn unload() -> Result<()> {
    ensure!(
        ksucalls::is_lkm(),
        "KernelSU unload is only supported in LKM mode"
    );

    info!("unload: starting KernelSU unload sequence");

    utils::switch_cgroups();

    info!("unload: stopping Android services...");
    let stop_result =
        run_android_service_control("stop").and_then(|()| wait_for_zygote_services_stopped());
    if let Err(stop_err) = stop_result {
        warn!("unload: Android stop failed, restoring service state...");
        if let Err(start_err) = run_android_service_control("start") {
            return Err(stop_err.context(format!(
                "Android service stop failed and recovery start also failed: {start_err:#}"
            )));
        }
        return Err(stop_err).context("Android service stop failed; teardown not started");
    }

    let mut hooks_prepared = false;
    let mut runtime_recovered = false;
    let mut unsafe_prepare_failure = false;

    let result = (|| -> Result<()> {
        info!("unload: killing su domain processes...");
        let su_pids = find_su_domain_pids();
        if !su_pids.is_empty() {
            info!(
                "unload: found {} su domain processes, sending SIGKILL",
                su_pids.len()
            );
            kill_pids(&su_pids, libc::SIGKILL);
        }

        info!("unload: killing processes holding ksu fds...");
        let fd_pids = find_ksu_fd_holders();
        if !fd_pids.is_empty() {
            info!(
                "unload: found {} processes holding ksu fds, sending SIGKILL",
                fd_pids.len()
            );
            kill_pids(&fd_pids, libc::SIGKILL);
        }

        let mut control_fd =
            Some(ksucalls::take_driver_fd().context("KernelSU driver fd unavailable")?);
        let control_raw = control_fd.as_ref().unwrap().as_raw_fd();
        close_ksu_fds_except(Some(control_raw));

        info!("unload: preparing kernel hooks for removal...");
        if let Err(err) = prepare_unload_with_retry(control_raw) {
            unsafe_prepare_failure = err.raw_os_error() == Some(libc::EUCLEAN);
            ksucalls::restore_driver_fd(control_fd.take().unwrap());
            return Err(err).context("prepare KernelSU unload failed");
        }
        hooks_prepared = true;

        info!("unload: committing kernel hook removal...");
        if let Err(commit_err) = commit_unload_with_retry(control_raw) {
            let abort_result = abort_unload_with_fd(control_raw);
            ksucalls::restore_driver_fd(control_fd.take().unwrap());

            match abort_result {
                Ok(()) => {
                    runtime_recovered = true;
                    return Err(commit_err)
                        .context("commit KernelSU unload failed; runtime restored");
                }
                Err(abort_err) => {
                    return Err(anyhow!(
                        "commit KernelSU unload failed: {commit_err}; recovery failed: {abort_err:#}"
                    ));
                }
            }
        }

        // COMMIT released the custom hook guard. Drop the owned control FD so
        // its fops module reference is also gone before delete_module(). The
        // shared cache is already empty, so it cannot retain this stale number.
        drop(control_fd.take());
        close_ksu_fds_except(None);

        info!("unload: removing kernelsu module...");
        if let Err(delete_err) = delete_module_with_retry() {
            warn!("unload: module removal failed after commit, restoring runtime hooks...");
            match recover_after_delete_failure() {
                Ok(()) => {
                    runtime_recovered = true;
                    return Err(delete_err)
                        .context("KernelSU module removal failed; runtime restored");
                }
                Err(recovery_err) => {
                    return Err(delete_err.context(format!(
                        "KernelSU module removal failed and runtime recovery failed: {recovery_err:#}"
                    )));
                }
            }
        }

        Ok(())
    })();

    let restart_safe =
        result.is_ok() || runtime_recovered || (!hooks_prepared && !unsafe_prepare_failure);
    let restart_result = if restart_safe {
        info!("unload: restarting Android services...");
        Some(run_android_service_control("start"))
    } else {
        warn!(
            "unload: kernel teardown did not reach a safe running state; leaving Android services stopped"
        );
        None
    };

    if let Some(Err(restart_err)) = restart_result {
        return match result {
            Ok(()) => {
                Err(restart_err.context("KernelSU unloaded but Android service restart failed"))
            }
            Err(unload_err) => Err(unload_err.context(format!(
                "Android service restart also failed: {restart_err:#}"
            ))),
        };
    }

    result?;
    info!("unload: done");
    Ok(())
}
