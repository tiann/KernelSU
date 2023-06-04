use anyhow::{Ok, Result};

#[cfg(unix)]
use anyhow::ensure;
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

pub const KERNEL_SU_OPTION: u32 = 0xDEAD_BEEF;

const CMD_GRANT_ROOT: u64 = 0;
// const CMD_BECOME_MANAGER: u64 = 1;
const CMD_GET_VERSION: u64 = 2;
// const CMD_ALLOW_SU: u64 = 3;
// const CMD_DENY_SU: u64 = 4;
// const CMD_GET_ALLOW_LIST: u64 = 5;
// const CMD_GET_DENY_LIST: u64 = 6;
const CMD_REPORT_EVENT: u64 = 7;
pub const CMD_SET_SEPOLICY: u64 = 8;
pub const CMD_CHECK_SAFEMODE: u64 = 9;

const EVENT_POST_FS_DATA: u64 = 1;
const EVENT_BOOT_COMPLETED: u64 = 2;

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn grant_root() -> Result<()> {
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

    ensure!(result == KERNEL_SU_OPTION, "grant root failed");
    Err(std::process::Command::new("sh").exec().into())
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
pub fn grant_root() -> Result<()> {
    unimplemented!("grant_root is only available on android");
}

fn print_usage(program: &str, opts: Options) {
    let brief = format!("KernelSU\n\nUsage: {program} [options] [-] [user [argument...]]");
    print!("{}", opts.usage(&brief));
}

fn set_identity(uid: u32, gid: u32) {
    #[cfg(any(target_os = "linux", target_os = "android"))]
    unsafe {
        libc::seteuid(uid);
        libc::setresgid(gid, gid, gid);
        libc::setresuid(uid, uid, uid);
    }
}

#[cfg(not(unix))]
pub fn root_shell() -> Result<()> {
    unimplemented!()
}

#[cfg(unix)]
pub fn root_shell() -> Result<()> {
    // we are root now, this was set in kernel!
    let args: Vec<String> = std::env::args().collect();
    let program = args[0].clone();

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

    let mut free_idx = 0;
    let command = matches.opt_str("c").map(|cmd| {
        free_idx = matches.free.len();
        let mut cmds = vec![];
        cmds.push(cmd);
        cmds.extend(matches.free.clone());
        cmds
    });

    let mut args = vec![];
    if let Some(cmd) = command {
        args.push("-c".to_string());
        args.push(cmd.join(" "));
    };

    if free_idx < matches.free.len() && matches.free[free_idx] == "-" {
        is_login = true;
        free_idx += 1;
    }

    // use current uid if no user specified, these has been done in kernel!
    let mut uid = unsafe { libc::getuid() };
    let gid = unsafe { libc::getgid() };
    if free_idx < matches.free.len() {
        let name = &matches.free[free_idx];
        uid = unsafe {
            #[cfg(target_arch = "aarch64")]
            let pw = libc::getpwnam(name.as_ptr() as *const u8).as_ref();
            #[cfg(target_arch = "x86_64")]
            let pw = libc::getpwnam(name.as_ptr() as *const i8).as_ref();

            match pw {
                Some(pw) => pw.pw_uid,
                None => name.parse::<u32>().unwrap_or(0),
            }
        }
    }

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

            set_identity(uid, gid);

            std::result::Result::Ok(())
        })
    };

    command = command.args(args).arg0(arg0);
    Err(command.exec().into())
}

fn add_path_to_env(path: &str) -> Result<()> {
    let mut paths =
        env::var_os("PATH").map_or(Vec::new(), |val| env::split_paths(&val).collect::<Vec<_>>());
    let new_path = PathBuf::from(path);
    paths.push(new_path);
    let new_path_env = env::join_paths(paths)?;
    env::set_var("PATH", new_path_env);
    Ok(())
}

pub fn get_version() -> i32 {
    let mut result: i32 = 0;
    #[cfg(any(target_os = "linux", target_os = "android"))]
    unsafe {
        #[allow(clippy::cast_possible_wrap)]
        libc::prctl(
            KERNEL_SU_OPTION as i32, // supposed to overflow
            CMD_GET_VERSION,
            std::ptr::addr_of_mut!(result).cast::<libc::c_void>(),
        );
    }
    result
}

fn report_event(event: u64) {
    #[cfg(any(target_os = "linux", target_os = "android"))]
    unsafe {
        #[allow(clippy::cast_possible_wrap)]
        libc::prctl(
            KERNEL_SU_OPTION as i32, // supposed to overflow
            CMD_REPORT_EVENT,
            event,
        );
    }
}

pub fn check_kernel_safemode() -> bool {
    let mut result: i32 = 0;
    #[cfg(any(target_os = "linux", target_os = "android"))]
    unsafe {
        #[allow(clippy::cast_possible_wrap)]
        libc::prctl(
            KERNEL_SU_OPTION as i32, // supposed to overflow
            CMD_CHECK_SAFEMODE,
            0,
            0,
            std::ptr::addr_of_mut!(result).cast::<libc::c_void>(),
        );
    }
    result == KERNEL_SU_OPTION as i32
}

pub fn report_post_fs_data() {
    report_event(EVENT_POST_FS_DATA);
}

pub fn report_boot_complete() {
    report_event(EVENT_BOOT_COMPLETED);
}
