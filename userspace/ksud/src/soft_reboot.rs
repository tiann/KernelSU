use std::{
    ffi::CString,
    fs::File,
    io::Write,
    os::{
        fd::{AsRawFd, FromRawFd, OwnedFd},
        unix::ffi::OsStrExt,
    },
    process::Command,
    ptr,
    time::{Duration, Instant},
};

use anyhow::{Context, Result, bail, ensure};
use libc::_exit;
use log::{error, info, warn};
use prop_rs_android::{resetprop::ResetProp, sys_prop};
use rustix::process::chdir;

use crate::{
    assets,
    init_event::{on_boot_completed, on_post_data_fs, on_services, run_stage},
    ksucalls,
    utils::{self, switch_mnt_ns},
};

const WAITSYS_FD_ENV: &str = "KSU_WAITSYS_FD";
const WAITSYS_READY_TIMEOUT: Duration = Duration::from_secs(2);
const WAITSYS_STOP_TIMEOUT: Duration = Duration::from_secs(5);

struct Waitsys {
    pid: libc::pid_t,
    read_fd: OwnedFd,
}

impl Waitsys {
    fn spawn() -> Result<Self> {
        let waitsys = assets::get_asset_data("waitsys").context("waitsys is not embedded")?;
        let name = CString::new("waitsys").expect("waitsys contains no NUL bytes");
        let executable_fd = unsafe {
            libc::syscall(libc::SYS_memfd_create, name.as_ptr(), libc::MFD_CLOEXEC) as libc::c_int
        };
        if executable_fd < 0 {
            return Err(std::io::Error::last_os_error()).context("failed to create waitsys memfd");
        }
        let mut executable = unsafe { File::from_raw_fd(executable_fd) };
        executable
            .write_all(&waitsys)
            .context("failed to write waitsys to memfd")?;

        let mut pipe_fds = [0; 2];
        if unsafe { libc::pipe2(pipe_fds.as_mut_ptr(), libc::O_CLOEXEC) } < 0 {
            return Err(std::io::Error::last_os_error()).context("failed to create waitsys pipe");
        }
        let read_fd = unsafe { OwnedFd::from_raw_fd(pipe_fds[0]) };
        let write_fd = unsafe { OwnedFd::from_raw_fd(pipe_fds[1]) };

        let mut environment = Vec::new();
        for (key, value) in std::env::vars_os() {
            if key.as_bytes() == WAITSYS_FD_ENV.as_bytes() {
                continue;
            }
            let mut entry = key.as_bytes().to_vec();
            entry.push(b'=');
            entry.extend_from_slice(value.as_bytes());
            environment.push(
                CString::new(entry).context("environment variable contains an embedded NUL")?,
            );
        }
        environment.push(
            CString::new(format!("{WAITSYS_FD_ENV}={}", write_fd.as_raw_fd()))
                .expect("waitsys fd environment variable contains no NUL bytes"),
        );
        let environment_pointers = environment
            .iter()
            .map(|entry| entry.as_ptr())
            .chain(std::iter::once(ptr::null()))
            .collect::<Vec<_>>();
        let arguments = [name.as_ptr(), ptr::null()];

        let pid = unsafe { libc::fork() };
        if pid < 0 {
            return Err(std::io::Error::last_os_error()).context("failed to fork waitsys");
        }
        if pid == 0 {
            unsafe {
                libc::close(read_fd.as_raw_fd());
                let flags = libc::fcntl(write_fd.as_raw_fd(), libc::F_GETFD);
                if flags < 0
                    || libc::fcntl(
                        write_fd.as_raw_fd(),
                        libc::F_SETFD,
                        flags & !libc::FD_CLOEXEC,
                    ) < 0
                {
                    _exit(127);
                }
                libc::syscall(
                    libc::SYS_execveat,
                    executable.as_raw_fd(),
                    c"".as_ptr(),
                    arguments.as_ptr(),
                    environment_pointers.as_ptr(),
                    libc::AT_EMPTY_PATH,
                );
                _exit(127);
            }
        }

        drop(write_fd);
        drop(executable);
        Ok(Self { pid, read_fd })
    }

    fn wait_for_signal(&self, expected: u8, timeout: Duration) -> Result<()> {
        let deadline = Instant::now() + timeout;
        loop {
            let remaining = deadline.saturating_duration_since(Instant::now());
            if remaining.is_zero() {
                bail!("timed out waiting for signal {expected}");
            }
            let timeout_ms = i32::try_from(remaining.as_millis().saturating_add(1))
                .unwrap_or(i32::MAX)
                .max(1);
            let mut poll_fd = libc::pollfd {
                fd: self.read_fd.as_raw_fd(),
                events: libc::POLLIN,
                revents: 0,
            };
            let ready = unsafe { libc::poll(&raw mut poll_fd, 1, timeout_ms) };
            if ready == 0 {
                bail!("timed out waiting for signal {expected}");
            }
            if ready < 0 {
                let error = std::io::Error::last_os_error();
                if error.kind() == std::io::ErrorKind::Interrupted {
                    continue;
                }
                return Err(error).context("failed to poll waitsys pipe");
            }

            let mut signal = 0_u8;
            let bytes_read = loop {
                let result =
                    unsafe { libc::read(self.read_fd.as_raw_fd(), (&raw mut signal).cast(), 1) };
                if result < 0
                    && std::io::Error::last_os_error().kind() == std::io::ErrorKind::Interrupted
                {
                    continue;
                }
                break result;
            };
            if bytes_read < 0 {
                return Err(std::io::Error::last_os_error()).context("failed to read waitsys pipe");
            }
            ensure!(
                bytes_read == 1,
                "waitsys pipe closed before signal {expected}"
            );
            ensure!(
                signal == expected,
                "unexpected waitsys signal {signal}, expected {expected}"
            );
            return Ok(());
        }
    }

    fn terminate(&mut self) -> Result<()> {
        if self.pid <= 0 {
            return Ok(());
        }

        let kill_error = if unsafe { libc::kill(self.pid, libc::SIGKILL) } < 0 {
            let error = std::io::Error::last_os_error();
            (error.raw_os_error() != Some(libc::ESRCH)).then_some(error)
        } else {
            None
        };

        let mut status = 0;
        loop {
            if unsafe { libc::waitpid(self.pid, &raw mut status, 0) } >= 0 {
                break;
            }
            let error = std::io::Error::last_os_error();
            if error.kind() == std::io::ErrorKind::Interrupted {
                continue;
            }
            if error.raw_os_error() != Some(libc::ECHILD) {
                self.pid = 0;
                return Err(error).context("failed to wait for waitsys");
            }
            break;
        }
        self.pid = 0;

        if let Some(error) = kill_error {
            return Err(error).context("failed to kill waitsys");
        }
        Ok(())
    }
}

impl Drop for Waitsys {
    fn drop(&mut self) {
        if let Err(error) = self.terminate() {
            warn!("failed to clean up waitsys: {error:#}");
        }
    }
}

fn terminate_waitsys(waitsys: &mut Option<Waitsys>) {
    if let Some(mut waitsys) = waitsys.take()
        && let Err(error) = waitsys.terminate()
    {
        warn!("failed to clean up waitsys: {error:#}");
    }
}

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

fn reset_boot_completed() -> Result<()> {
    sys_prop::init().context("Failed to initialize system property API")?;
    let rp = resetprop();
    // Set prop value to 0 in advance to ensure resetprop -w works
    info!("reset boot complete prop to 0");
    rp.set("sys.boot_completed", "0")
        .context("Failed to set sys.boot_completed to 0")?;
    Ok(())
}

fn wait_for_boot_completed() -> Result<()> {
    sys_prop::init().context("Failed to initialize system property API")?;
    let rp = resetprop();
    info!("waiting for boot complete");
    rp.wait("sys.boot_completed", Some("0"), None)
        .context("wait for sys.boot_completed failed")?;
    Ok(())
}

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

    let mut waitsys = match Waitsys::spawn() {
        Ok(waitsys) => Some(waitsys),
        Err(error) => {
            warn!("failed to start waitsys: {error:#}");
            None
        }
    };
    let wait_after_stop = waitsys.as_ref().is_some_and(|waitsys| {
        if let Err(error) = waitsys.wait_for_signal(1, WAITSYS_READY_TIMEOUT) {
            warn!("waitsys failed to collect services: {error:#}");
            false
        } else {
            true
        }
    });
    if !wait_after_stop {
        terminate_waitsys(&mut waitsys);
    }

    info!("stop");
    let status = Command::new("stop").status().context("stop failed")?;
    if !status.success() {
        warn!("stop exited with status: {status}");
    }

    if let Some(waitsys) = waitsys.as_ref()
        && let Err(error) = waitsys.wait_for_signal(2, WAITSYS_STOP_TIMEOUT)
    {
        warn!("waitsys failed while waiting for services to stop: {error:#}");
    }
    terminate_waitsys(&mut waitsys);

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
