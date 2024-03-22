use anyhow::{Ok, Result};
use getopts::Options;
use std::env;
#[cfg(unix)]
use std::os::unix::process::CommandExt;
use std::path::PathBuf;
use std::{ffi::CStr, process::Command};

use crate::{
    defs,
    utils::{self, umask},
};

#[cfg(any(target_os = "linux", target_os = "android"))]
use rustix::{
    process::getuid,
    thread::{set_thread_res_gid, set_thread_res_uid, Gid, Uid},
};

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn grant_root(global_mnt: bool) -> Result<()> {
    const KERNEL_SU_OPTION: u32 = 0xDEAD_BEEF;
    const CMD_GRANT_ROOT: u64 = 0;

    let mut result: u32 = 0;
    unsafe {
        #[allow(clippy::cast_possible_wrap)]
        libc::prctl(
            KERNEL_SU_OPTION as i32, // supposed to overflow
            CMD_GRANT_ROOT,
            0,
            0,
            std::ptr::addr_of_mut!(result).cast::<libc::c_void>(),
        );
    }

    anyhow::ensure!(result == KERNEL_SU_OPTION, "grant root failed");
    let mut command = std::process::Command::new("sh");
    let command = unsafe {
        command.pre_exec(move || {
            if global_mnt {
                let _ = utils::switch_mnt_ns(1);
                let _ = utils::unshare_mnt_ns();
            }
            std::result::Result::Ok(())
        })
    };
    // add /data/adb/ksu/bin to PATH
    #[cfg(any(target_os = "linux", target_os = "android"))]
    add_path_to_env(defs::BINARY_DIR)?;
    Err(command.exec().into())
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
pub fn grant_root(_global_mnt: bool) -> Result<()> {
    unimplemented!("grant_root is only available on android");
}

fn print_usage(program: &str, opts: Options) {
    let brief = format!("KernelSU\n\nUsage: {program} [options] [-] [user [argument...]]");
    print!("{}", opts.usage(&brief));
}

fn set_identity(uid: u32, gid: u32, groups: &[u32]) {
    #[cfg(any(target_os = "linux", target_os = "android"))]
    {
        rustix::process::set_groups(
            groups
                .iter()
                .map(|g| unsafe { Gid::from_raw(*g) })
                .collect::<Vec<_>>()
                .as_ref(),
        )
        .ok();
        let gid = unsafe { Gid::from_raw(gid) };
        let uid = unsafe { Uid::from_raw(uid) };
        set_thread_res_gid(gid, gid, gid).ok();
        set_thread_res_uid(uid, uid, uid).ok();
    }
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
pub fn root_shell() -> Result<()> {
    unimplemented!()
}

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn root_shell() -> Result<()> {
    // we are root now, this was set in kernel!

    use anyhow::anyhow;
    let env_args: Vec<String> = std::env::args().collect();
    let program = env_args[0].clone();
    let args = env_args
        .iter()
        .position(|arg| arg == "-c")
        .map(|i| {
            let rest = env_args[i + 1..].to_vec();
            let mut new_args = env_args[..i].to_vec();
            new_args.push("-c".to_string());
            if !rest.is_empty() {
                new_args.push(rest.join(" "));
            }
            new_args
        })
        .unwrap_or_else(|| env_args.clone());

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
    opts.optflag(
        "s",
        "shell",
        "use SHELL instead of the default /system/bin/sh",
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
        std::result::Result::Ok(m) => m,
        Err(f) => {
            println!("{f}");
            print_usage(&program, opts);
            std::process::exit(-1);
        }
    };

    if matches.opt_present("h") {
        print_usage(&program, opts);
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

    let shell = matches.opt_str("s").unwrap_or("/system/bin/sh".to_string());
    let mut is_login = matches.opt_present("l");
    let preserve_env = matches.opt_present("p");
    let mount_master = matches.opt_present("M");

    let groups = matches
        .opt_strs("G")
        .into_iter()
        .map(|g| g.parse::<u32>().map_err(|_| anyhow!("Invalid GID: {}", g)))
        .collect::<Result<Vec<_>, _>>()?;

    // if -g provided, use it.
    let mut gid = matches
        .opt_str("g")
        .map(|g| g.parse::<u32>().map_err(|_| anyhow!("Invalid GID: {}", g)))
        .transpose()?;

    // otherwise, use the first gid of groups.
    if gid.is_none() && !groups.is_empty() {
        gid = Some(groups[0]);
    }

    // we've make sure that -c is the last option and it already contains the whole command, no need to construct it again
    let args = matches
        .opt_str("c")
        .map(|cmd| vec!["-c".to_string(), cmd])
        .unwrap_or_default();

    let mut free_idx = 0;
    if !matches.free.is_empty() && matches.free[free_idx] == "-" {
        is_login = true;
        free_idx += 1;
    }

    // use current uid if no user specified, these has been done in kernel!
    let mut uid = getuid().as_raw();
    if free_idx < matches.free.len() {
        let name = &matches.free[free_idx];
        uid = unsafe {
            #[cfg(target_arch = "aarch64")]
            let pw = libc::getpwnam(name.as_ptr()).as_ref();
            #[cfg(target_arch = "x86_64")]
            let pw = libc::getpwnam(name.as_ptr() as *const i8).as_ref();

            match pw {
                Some(pw) => pw.pw_uid,
                None => name.parse::<u32>().unwrap_or(0),
            }
        }
    }

    // if there is no gid provided, use uid.
    let gid = gid.unwrap_or(uid);
    // https://github.com/topjohnwu/Magisk/blob/master/native/src/su/su_daemon.cpp#L408
    let arg0 = if is_login { "-" } else { &shell };

    let mut command = &mut Command::new(&shell);

    if !preserve_env {
        // This is actually incorrect, i don't know why.
        // command = command.env_clear();

        let pw = unsafe { libc::getpwuid(uid).as_ref() };

        if let Some(pw) = pw {
            let home = unsafe { CStr::from_ptr(pw.pw_dir) };
            let pw_name = unsafe { CStr::from_ptr(pw.pw_name) };

            let home = home.to_string_lossy();
            let pw_name = pw_name.to_string_lossy();

            command = command
                .env("HOME", home.as_ref())
                .env("USER", pw_name.as_ref())
                .env("LOGNAME", pw_name.as_ref())
                .env("SHELL", &shell);
        }
    }

    // add /data/adb/ksu/bin to PATH
    #[cfg(any(target_os = "linux", target_os = "android"))]
    add_path_to_env(defs::BINARY_DIR)?;

    // when KSURC_PATH exists and ENV is not set, set ENV to KSURC_PATH
    if PathBuf::from(defs::KSURC_PATH).exists() && env::var("ENV").is_err() {
        command = command.env("ENV", defs::KSURC_PATH);
    }

    // escape from the current cgroup and become session leader
    // WARNING!!! This cause some root shell hang forever!
    // command = command.process_group(0);
    command = unsafe {
        command.pre_exec(move || {
            umask(0o22);
            utils::switch_cgroups();

            // switch to global mount namespace
            #[cfg(any(target_os = "linux", target_os = "android"))]
            if mount_master {
                let _ = utils::switch_mnt_ns(1);
                let _ = utils::unshare_mnt_ns();
            }

            set_identity(uid, gid, &groups);

            std::result::Result::Ok(())
        })
    };

    command = command.args(args).arg0(arg0);
    Err(command.exec().into())
}

fn add_path_to_env(path: &str) -> Result<()> {
    let mut paths =
        env::var_os("PATH").map_or(Vec::new(), |val| env::split_paths(&val).collect::<Vec<_>>());
    let new_path = PathBuf::from(path.trim_end_matches('/'));
    paths.push(new_path);
    let new_path_env = env::join_paths(paths)?;
    env::set_var("PATH", new_path_env);
    Ok(())
}
