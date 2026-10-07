use crate::{
    defs, ksucalls,
    utils::{self, umask},
};
use anyhow::{Context, Ok, Result, bail};
use libc::c_int;
use log::error;
use std::os::unix::process::CommandExt;
use std::path::PathBuf;
use std::{env, io};

use crate::su_options::SuOptions;
use std::{
    ffi::{CStr, CString},
    process::Command,
};

use crate::ksucalls::get_wrapped_fd;
use rustix::{
    process::getuid,
    thread::{Gid, Uid, set_thread_res_gid, set_thread_res_uid},
};

pub fn grant_root(global_mnt: bool, shell_command: Option<&str>) -> Result<()> {
    crate::ksucalls::grant_root()?;

    wrap_tty(0);
    wrap_tty(1);
    wrap_tty(2);

    let mut command = Command::new("sh");
    if let Some(shell_command) = shell_command {
        command.args(["-c", shell_command]);
    }
    let command = unsafe {
        command.pre_exec(move || {
            if global_mnt {
                let _ = utils::switch_mnt_ns(1);
            }
            Result::Ok(())
        })
    };
    // add /data/adb/ksu/bin to PATH
    add_path_to_env(defs::BINARY_DIR)?;
    Err(command.exec().into())
}

fn print_usage() {
    println!(
        "KernelSU\n\nUsage: su [options] [--] [user [argument...]]\n
  -s, --shell SHELL          Shell to execute (default /system/bin/sh)
  -g, --group GROUP          Primary group
  -G, --supp-group GROUP     Supplementary group (first is default primary)
  -Z, --context CONTEXT      SELinux context for the executed shell
  -t, --target PID           Mount namespace of PID (0 means global)
  -M, --mount-master         Use the global mount namespace
  -m, -p, --preserve-environment  Preserve the environment
  -l, --login                Start a login shell
  -i, --interactive          Inherit the caller's terminal (always done by KSU)
  -W, --no-wrapper           Do not use the KSU TTY fd wrapper
  --ksu-no-new-privs         Prevent KernelSU privilege re-escalation
  -v, --version             Print version
  -V                        Print version code
  -h, --help                Show this help

Parsing stops at --, a user, or an unknown option.
Remaining arguments are passed to the shell."
    );
}

fn set_identity(uid: u32, gid: u32, groups: &[u32]) -> Result<()> {
    rustix::thread::set_thread_groups(
        groups
            .iter()
            .map(|g| Gid::from_raw(*g))
            .collect::<Vec<_>>()
            .as_ref(),
    )
    .with_context(|| format!("setgroups {groups:?}"))?;
    let gid = Gid::from_raw(gid);
    let uid = Uid::from_raw(uid);
    set_thread_res_gid(gid, gid, gid).with_context(|| format!("setresgid {gid}"))?;
    set_thread_res_uid(uid, uid, uid).with_context(|| format!("setresuid {uid}"))?;
    Ok(())
}

fn resolve_uid(user: &str) -> Result<u32> {
    let c_user = CString::new(user).with_context(|| format!("Invalid user: {user}"))?;
    let pw = unsafe { libc::getpwnam(c_user.as_ptr()).as_ref() };

    if let Some(pw) = pw {
        return Ok(pw.pw_uid);
    }

    user.parse::<u32>()
        .map_err(|_| anyhow::anyhow!("Unknown user: {user}"))
}

fn set_selinux_context(context: &str) -> Result<()> {
    std::fs::write("/proc/thread-self/attr/exec", context)?;
    Ok(())
}

fn wrap_tty(fd: c_int) {
    let inner_fn = move || -> Result<()> {
        if unsafe { libc::isatty(fd) != 1 }
            && io::Error::last_os_error().raw_os_error() != Some(libc::EACCES)
        {
            return Ok(());
        }

        // The root profile is already active here, so its SELinux domain may
        // return EACCES while querying the original terminal. In that case,
        // check the wrapped fd instead, since that descriptor is intended to
        // bypass this restriction.
        let new_fd = get_wrapped_fd(fd).context("get_wrapped_fd")?;
        if unsafe { libc::isatty(new_fd) != 1 } {
            unsafe { libc::close(new_fd) };
            return Ok(());
        }
        let dup_result = unsafe { libc::dup2(new_fd, fd) };
        let dup_errno = unsafe { *libc::__errno() };
        unsafe { libc::close(new_fd) };
        if dup_result == -1 {
            bail!("dup {new_fd} -> {fd} errno: {dup_errno}");
        }
        Ok(())
    };

    if let Err(e) = inner_fn() {
        error!("wrap tty {fd}: {e:?}");
    }
}

#[allow(clippy::similar_names)]
pub fn root_shell() -> Result<()> {
    // The kernel has already applied the selected root profile.

    // A su-session driver fd deliberately survives the exec into ksud. Claim
    // it before handling any arguments and restore FD_CLOEXEC so it cannot
    // leak into the target shell, including when fd wrapping is disabled.
    ksucalls::claim_inherited_driver_fd().context("claim inherited KernelSU driver fd")?;

    let args = env::args().skip(1).collect::<Vec<_>>();
    let options = SuOptions::parse(&args, |user| resolve_uid(user).ok())?;
    if options.help {
        print_usage();
        return Ok(());
    }
    if options.version {
        println!("{}:KernelSU", defs::VERSION_NAME);
        return Ok(());
    }
    if options.version_code {
        println!("{}", defs::VERSION_CODE);
        return Ok(());
    }
    let shell = options.shell.as_deref().unwrap_or("/system/bin/sh");
    // An unspecified identity keeps the root profile installed by the kernel.
    let identity_requested = options.uid.is_some() || !options.gids.is_empty();
    let uid = options.uid.unwrap_or_else(|| getuid().as_raw());
    let gid = options.gids.first().copied().unwrap_or(uid);
    let executable = shell;
    let arg0 = if options.login { "-" } else { executable };

    let mut command = Command::new(executable);

    if !options.preserve_env {
        // This is actually incorrect, i don't know why.
        // command = command.env_clear();

        let pw = unsafe { libc::getpwuid(uid).as_ref() };

        if let Some(pw) = pw {
            let home = unsafe { CStr::from_ptr(pw.pw_dir) };
            let pw_name = unsafe { CStr::from_ptr(pw.pw_name) };

            let home = home.to_string_lossy();
            let pw_name = pw_name.to_string_lossy();

            command
                .env("HOME", home.as_ref())
                .env("USER", pw_name.as_ref())
                .env("LOGNAME", pw_name.as_ref())
                .env("SHELL", shell);
        }
    }

    // add /data/adb/ksu/bin to PATH
    add_path_to_env(defs::BINARY_DIR)?;

    // when KSURC_PATH exists and ENV is not set, set ENV to KSURC_PATH
    if PathBuf::from(defs::KSURC_PATH).exists() && env::var("ENV").is_err() {
        command.env("ENV", defs::KSURC_PATH);
    }

    if options.no_new_privs {
        ksucalls::set_ksu_no_new_privs().context("set KSU_NO_NEW_PRIVS")?;
    }

    // escape from the current cgroup and become session leader
    // WARNING!!! This cause some root shell hang forever!
    // command = command.process_group(0);

    command.args(&options.shell_args).arg0(arg0);
    umask(0o22);
    utils::switch_cgroups();

    // switch to global mount namespace
    if let Some(pid) = options.target_pid {
        let pid = if pid == 0 { 1 } else { pid };
        utils::switch_mnt_ns(pid)
            .with_context(|| format!("switch mount namespace to PID {pid}"))?;
    }

    if !options.no_wrapper {
        wrap_tty(0);
        wrap_tty(1);
        wrap_tty(2);
    }

    if identity_requested {
        set_identity(uid, gid, &options.gids)?;
    }
    if let Some(context) = options.context.as_deref() {
        set_selinux_context(context).with_context(|| format!("setcontext {context}"))?;
    }
    Err(command.exec().into())
}

fn add_path_to_env(path: &str) -> Result<()> {
    let mut paths =
        env::var_os("PATH").map_or(Vec::new(), |val| env::split_paths(&val).collect::<Vec<_>>());
    let new_path = PathBuf::from(path.trim_end_matches('/'));
    paths.push(new_path);
    let new_path_env = env::join_paths(paths)?;
    unsafe { env::set_var("PATH", new_path_env) };
    Ok(())
}
