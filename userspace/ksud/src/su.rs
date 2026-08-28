use crate::{
    defs, ksucalls,
    utils::{self, umask},
};
use anyhow::{Context, Ok, Result, bail};
use getopts::Options;
use libc::c_int;
use log::error;
#[cfg(unix)]
use std::os::unix::process::CommandExt;
use std::path::PathBuf;
use std::{cmp::Ordering, env};
use std::{
    ffi::{CStr, CString},
    process::Command,
};

use crate::ksucalls::get_wrapped_fd;
use rustix::{
    process::getuid,
    thread::{Gid, Uid, set_thread_res_gid, set_thread_res_uid},
};

pub fn grant_root(global_mnt: bool) -> Result<()> {
    crate::ksucalls::grant_root()?;

    let mut command = Command::new("sh");
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

fn print_usage(program: &str, opts: &Options) {
    let brief = format!("KernelSU\n\nUsage: {program} [options] [-] [user [argument...]]");
    print!("{}", opts.usage(&brief));
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
    std::fs::write("/proc/thread-self/attr/current", context)?;
    Ok(())
}

fn wrap_tty(fd: c_int) {
    let inner_fn = move || -> Result<()> {
        if unsafe { libc::isatty(fd) != 1 } {
            return Ok(());
        }
        let new_fd = get_wrapped_fd(fd).context("get_wrapped_fd")?;
        if unsafe { libc::dup2(new_fd, fd) } == -1 {
            bail!("dup {new_fd} -> {fd} errno: {}", unsafe {
                *libc::__errno()
            });
        }
        unsafe { libc::close(new_fd) };
        Ok(())
    };

    if let Err(e) = inner_fn() {
        error!("wrap tty {fd}: {e:?}");
    }
}

#[allow(clippy::similar_names)]
pub fn root_shell() -> Result<()> {
    // we are root now, this was set in kernel!

    use anyhow::anyhow;
    let env_args: Vec<String> = env::args().collect();
    let program = env_args[0].clone();
    let mut executable: Option<String> = None;
    let mut exec_args: Option<Vec<String>> = None;
    let first_option_c = env_args
        .iter()
        .position(|arg| arg == "-c")
        .unwrap_or(usize::MAX);
    let first_non_option = env_args
        .windows(3)
        .position(|arg| {
            !arg[1].starts_with('-')
                && !arg[2].starts_with('-')
                && !(arg[0].starts_with("-g")
                    || arg[0].starts_with("-G")
                    || arg[0].starts_with("-s")
                    || arg[0].starts_with("-Z")
                    || arg[0] == "--group"
                    || arg[0] == "--supp-group="
                    || arg[0] == "--shell="
                    || arg[0] == "--context=")
        })
        .map_or(usize::MAX, |idx| idx + 1);
    let args = match first_non_option.cmp(&first_option_c) {
        Ordering::Equal => env_args,
        Ordering::Less => {
            executable = Some(env_args[first_non_option + 1].clone());
            exec_args = Some(env_args[first_non_option + 2..].to_vec());
            env_args[..=first_non_option].to_vec()
        }
        Ordering::Greater => {
            let rest = env_args[first_option_c + 1..].to_vec();
            let mut new_args = env_args[..first_option_c].to_vec();
            new_args.push("-c".to_string());
            if !rest.is_empty() {
                new_args.push(rest.join(" "));
            }
            new_args
        }
    };

    let mut opts = Options::new();
    opts.optopt(
        "c",
        "command",
        "pass COMMAND to the invoked shell",
        "COMMAND",
    );
    opts.optflag("h", "help", "display this help message and exit");
    opts.optflag("l", "login", "pretend the shell to be a login shell");
    opts.optflag(
        "p",
        "preserve-environment",
        "preserve the entire environment",
    );
    opts.optopt(
        "s",
        "shell",
        "use SHELL instead of the default /system/bin/sh",
        "SHELL",
    );
    opts.optflag("v", "version", "display version number and exit");
    opts.optflag("V", "", "display version code and exit");
    opts.optflag(
        "M",
        "mount-master",
        "force run in the global mount namespace",
    );
    opts.optopt("g", "group", "Specify the primary group", "GROUP");
    opts.optmulti(
        "G",
        "supp-group",
        "Specify a supplementary group. The first specified supplementary group is also used as a primary group if the option -g is not specified.",
        "GROUP",
    );
    opts.optflag("W", "no-wrapper", "don't use ksu fd wrapper");
    opts.optflag(
        "",
        "ksu-no-new-privs",
        "Prevent this process (and its children) from privilege re-escalation via KernelSU",
    );
    opts.optopt("Z", "context", "Specify the SELinux context", "CONTEXT");

    // Replace -cn with -z, -mm with -M for supporting getopt_long
    let args = args
        .into_iter()
        .map(|e| {
            if e == "-mm" {
                "-M".to_string()
            } else if e == "-cn" {
                "-z".to_string()
            } else {
                e
            }
        })
        .collect::<Vec<String>>();

    let matches = match opts.parse(&args[1..]) {
        Result::Ok(m) => m,
        Err(f) => {
            println!("{f}");
            print_usage(&program, &opts);
            std::process::exit(-1);
        }
    };

    if matches.opt_present("h") {
        print_usage(&program, &opts);
        return Ok(());
    }

    if matches.opt_present("v") {
        println!("{}:KernelSU", defs::VERSION_NAME);
        return Ok(());
    }

    if matches.opt_present("V") {
        println!("{}", defs::VERSION_CODE);
        return Ok(());
    }

    let shell = matches
        .opt_str("s")
        .unwrap_or_else(|| "/system/bin/sh".to_string());
    let mut is_login = matches.opt_present("l");
    let preserve_env = matches.opt_present("p");
    let mount_master = matches.opt_present("M");
    let use_fd_wrapper = !matches.opt_present("W");
    let ksu_no_new_privs = matches.opt_present("ksu-no-new-privs");
    let selinux_context = matches.opt_str("Z");

    let groups = matches
        .opt_strs("G")
        .into_iter()
        .map(|g| g.parse::<u32>().map_err(|_| anyhow!("Invalid GID: {g}")))
        .collect::<Result<Vec<_>, _>>()?;

    // if -g provided, use it.
    let mut gid = matches
        .opt_str("g")
        .map(|g| g.parse::<u32>().map_err(|_| anyhow!("Invalid GID: {g}")))
        .transpose()?;

    // otherwise, use the first gid of groups.
    if gid.is_none() && !groups.is_empty() {
        gid = Some(groups[0]);
    }

    // we've make sure that -c is the last option and it already contains the whole command, no need to construct it again
    let args = exec_args.unwrap_or_else(|| {
        matches
            .opt_str("c")
            .map(|cmd| vec!["-c".to_string(), cmd])
            .unwrap_or_default()
    });

    let mut free_idx = 0;
    if !matches.free.is_empty() && matches.free[free_idx] == "-" {
        is_login = true;
        free_idx += 1;
    }

    let identity_requested = free_idx < matches.free.len() || gid.is_some() || !groups.is_empty();

    // use current uid if no user specified, these has been done in kernel!
    let uid = if free_idx < matches.free.len() {
        resolve_uid(&matches.free[free_idx])?
    } else {
        getuid().as_raw()
    };

    // if there is no gid provided, use uid.
    let gid = gid.unwrap_or(uid);
    let executable = executable.as_ref().unwrap_or(&shell);
    // https://github.com/topjohnwu/Magisk/blob/master/native/src/su/su_daemon.cpp#L408
    let arg0 = if is_login { "-" } else { executable };

    let mut command = Command::new(executable);

    if !preserve_env {
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
                .env("SHELL", &shell);
        }
    }

    // add /data/adb/ksu/bin to PATH
    add_path_to_env(defs::BINARY_DIR)?;

    // when KSURC_PATH exists and ENV is not set, set ENV to KSURC_PATH
    if PathBuf::from(defs::KSURC_PATH).exists() && env::var("ENV").is_err() {
        command.env("ENV", defs::KSURC_PATH);
    }

    if ksu_no_new_privs {
        ksucalls::set_ksu_no_new_privs().context("set KSU_NO_NEW_PRIVS")?;
    }

    // escape from the current cgroup and become session leader
    // WARNING!!! This cause some root shell hang forever!
    // command = command.process_group(0);

    command.args(args).arg0(arg0);
    umask(0o22);
    utils::switch_cgroups();

    // switch to global mount namespace
    if mount_master {
        let _ = utils::switch_mnt_ns(1);
    }

    if use_fd_wrapper {
        wrap_tty(0);
        wrap_tty(1);
        wrap_tty(2);
    }

    if identity_requested {
        set_identity(uid, gid, &groups)?;
    }
    if let Some(context) = selinux_context.as_deref() {
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
