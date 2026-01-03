use crate::{defs, utils::{self, umask}};
use anyhow::{Context, Ok, Result, bail};
use getopts::Options;
use libc::c_int;
use log::{error, debug, info, warn};
use std::env;
#[cfg(unix)]
use std::os::unix::process::CommandExt;
use std::path::PathBuf;
use std::{ffi::{CStr, CString}, process::Command, io::Write};
use crate::ksucalls::get_wrapped_fd;
use rustix::{
    process::getuid,
    thread::{Gid, Uid, set_thread_res_gid, set_thread_res_uid},
};

// 新增函数：记录详细的执行环境
fn log_execution_context(shell: &str, args: &[String], envs: &[(String, String)]) {
    info!("=== SU EXECUTION CONTEXT ===");
    info!("Target shell: {}", shell);
    info!("Arguments: {:?}", args);
    
    info!("Environment variables:");
    for (key, value) in envs {
        info!("  {}={}", key, value);
    }
    
    let uid = getuid().as_raw();
    info!("Current UID: {}", uid);
    
    // 获取当前进程信息
    if let Ok(status) = std::fs::read_to_string("/proc/self/status") {
        for line in status.lines().take(10) {
            debug!("Process status: {}", line);
        }
    }
    
    // 检查TTY状态
    let stdin_is_tty = unsafe { libc::isatty(0) == 1 };
    let stdout_is_tty = unsafe { libc::isatty(1) == 1 };
    let stderr_is_tty = unsafe { libc::isatty(2) == 1 };
    info!("TTY status - stdin: {}, stdout: {}, stderr: {}", 
          stdin_is_tty, stdout_is_tty, stderr_is_tty);
    
    info!("=============================");
}

pub fn grant_root(global_mnt: bool) -> Result<()> {
    debug!("Attempting to grant root access. Global mount namespace: {}", global_mnt);
    
    match crate::ksucalls::grant_root() {
        Ok(_) => debug!("Successfully granted root access via kernel"),
        Err(e) => {
            error!("Failed to grant root access: {:?}", e);
            bail!("grant_root kernel call failed: {}", e);
        }
    };
    
    let mut command = Command::new("sh");
    debug!("Preparing to execute shell command with global_mnt={}", global_mnt);
    
    let command = unsafe {
        command.pre_exec(move || {
            if global_mnt {
                debug!("Switching to global mount namespace");
                match utils::switch_mnt_ns(1) {
                    Ok(_) => debug!("Successfully switched to global mount namespace"),
                    Err(e) => warn!("Failed to switch to global mount namespace: {:?}", e),
                };
            }
            Result::Ok(())
        })
    };
    
    // add /data/adb/ksu/bin to PATH
    match add_path_to_env(defs::BINARY_DIR) {
        Ok(_) => debug!("Successfully added {} to PATH", defs::BINARY_DIR),
        Err(e) => warn!("Failed to add {} to PATH: {:?}", defs::BINARY_DIR, e),
    };
    
    debug!("Executing shell command...");
    let result = command.exec();
    error!("Shell command execution failed: {:?}", result);
    Err(result.into())
}

fn print_usage(program: &str, opts: &Options) {
    let brief = format!("KernelSU\n\nUsage: {program} [options] [-] [user [argument...]]");
    print!("{}", opts.usage(&brief));
}

fn set_identity(uid: u32, gid: u32, groups: &[u32]) {
    debug!("Setting identity - UID: {}, GID: {}, Groups: {:?}", uid, gid, groups);
    
    match rustix::thread::set_thread_groups(
        groups
            .iter()
            .map(|g| Gid::from_raw(*g))
            .collect::<Vec<_>>()
            .as_ref(),
    ) {
        Ok(_) => debug!("Successfully set supplementary groups"),
        Err(e) => warn!("Failed to set supplementary groups: {:?}", e),
    };
    
    let gid = Gid::from_raw(gid);
    let uid = Uid::from_raw(uid);
    
    match set_thread_res_gid(gid, gid, gid) {
        Ok(_) => debug!("Successfully set real, effective and saved GID"),
        Err(e) => warn!("Failed to set GID: {:?}", e),
    };
    
    match set_thread_res_uid(uid, uid, uid) {
        Ok(_) => debug!("Successfully set real, effective and saved UID"),
        Err(e) => warn!("Failed to set UID: {:?}", e),
    };
}

fn wrap_tty(fd: c_int) {
    let inner_fn = move || -> Result<()> {
        let is_tty = unsafe { libc::isatty(fd) == 1 };
        debug!("Wrapping TTY for fd={}, is_tty={}", fd, is_tty);
        
        if !is_tty {
            debug!("FD {} is not a TTY, skipping wrapping", fd);
            return Ok(());
        }
        
        let new_fd = get_wrapped_fd(fd).context("get_wrapped_fd")?;
        debug!("Got wrapped fd: {} for original fd: {}", new_fd, fd);
        
        let dup_result = unsafe { libc::dup2(new_fd, fd) };
        if dup_result == -1 {
            let errno = unsafe { *libc::__errno_location() };
            let err_str = std::io::Error::from_raw_os_error(errno).to_string();
            error!("dup2 failed: {} (errno: {})", err_str, errno);
            bail!("dup {} -> {} failed: {} (errno: {})", new_fd, fd, err_str, errno);
        }
        
        debug!("Successfully duplicated fd {} to {}", new_fd, fd);
        unsafe { libc::close(new_fd) };
        Ok(())
    };
    
    if let Err(e) = inner_fn() {
        error!("wrap tty {}: {:?}", fd, e);
        // 写入错误到stderr以便父进程可以捕获
        let _ = std::io::stderr().write_all(format!("TTY wrap error for fd {}: {:?}\n", fd, e).as_bytes());
    }
}

#[allow(clippy::similar_names)]
pub fn root_shell() -> Result<()> {
    debug!("Starting root_shell function");
    // we are root now, this was set in kernel!
    use anyhow::anyhow;
    
    let env_args: Vec<String> = env::args().collect();
    debug!("Command arguments: {:?}", env_args);
    
    let program = env_args[0].clone();
    let args = env_args.iter().position(|arg| arg == "-c").map_or_else(
        || env_args.clone(),
        |i| {
            let rest = env_args[i + 1..].to_vec();
            let mut new_args = env_args[..i].to_vec();
            new_args.push("-c".to_string());
            if !rest.is_empty() {
                new_args.push(rest.join(" "));
            }
            new_args
        },
    );
    
    let mut opts = Options::new();
    opts.optopt("c", "command", "pass COMMAND to the invoked shell", "COMMAND");
    opts.optflag("h", "help", "display this help message and exit");
    opts.optflag("l", "login", "pretend the shell to be a login shell");
    opts.optflag("p", "preserve-environment", "preserve the entire environment");
    opts.optopt("s", "shell", "use SHELL instead of the default /system/bin/sh", "SHELL");
    opts.optflag("v", "version", "display version number and exit");
    opts.optflag("V", "", "display version code and exit");
    opts.optflag("M", "mount-master", "force run in the global mount namespace");
    opts.optopt("g", "group", "Specify the primary group", "GROUP");
    opts.optmulti("G", "supp-group", "Specify a supplementary group. The first specified supplementary group is also used as a primary group if the option -g is not specified.", "GROUP");
    opts.optflag("W", "no-wrapper", "don't use ksu fd wrapper");
    
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
    
    debug!("Processed arguments: {:?}", args);
    
    let matches = match opts.parse(&args[1..]) {
        Result::Ok(m) => {
            debug!("Successfully parsed command line options");
            m
        }
        Err(f) => {
            error!("Failed to parse command line options: {}", f);
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
    
    debug!("Shell configuration - shell: {}, is_login: {}, preserve_env: {}, mount_master: {}, use_fd_wrapper: {}",
           shell, is_login, preserve_env, mount_master, use_fd_wrapper);
    
    let groups = matches
        .opt_strs("G")
        .into_iter()
        .map(|g| g.parse::<u32>().map_err(|_| anyhow!("Invalid GID: {g}")))
        .collect::<Result<Vec<_>, _>>()?;
    
    debug!("Supplementary groups: {:?}", groups);
    
    // if -g provided, use it.
    let mut gid = matches
        .opt_str("g")
        .map(|g| g.parse::<u32>().map_err(|_| anyhow!("Invalid GID: {g}")))
        .transpose()?;
    
    // otherwise, use the first gid of groups.
    if gid.is_none() && !groups.is_empty() {
        gid = Some(groups[0]);
        debug!("Using first supplementary group as primary GID: {}", groups[0]);
    }
    
    // we've make sure that -c is the last option and it already contains the whole command, no need to construct it again
    let args = matches
        .opt_str("c")
        .map(|cmd| vec!["-c".to_string(), cmd])
        .unwrap_or_default();
    
    debug!("Command args: {:?}", args);
    
    let mut free_idx = 0;
    if !matches.free.is_empty() && matches.free[free_idx] == "-" {
        is_login = true;
        free_idx += 1;
        debug!("Login shell requested");
    }
    
    // use current uid if no user specified, these has been done in kernel!
    let mut uid = getuid().as_raw();
    debug!("Default UID: {}", uid);
    
    if free_idx < matches.free.len() {
        let name = &matches.free[free_idx];
        debug!("Target user specified: {}", name);
        
        uid = unsafe {
            let pw = CString::new(name.as_str())
                .ok()
                .and_then(|c_name| libc::getpwnam(c_name.as_ptr()).as_ref());
            
            match pw {
                Some(pw) => {
                    debug!("Found user in passwd database: name={}, uid={}", 
                           std::ffi::CStr::from_ptr(pw.pw_name).to_string_lossy(), pw.pw_uid);
                    pw.pw_uid
                }
                None => {
                    match name.parse::<u32>() {
                        Ok(uid) => {
                            debug!("Parsed UID directly from argument: {}", uid);
                            uid
                        }
                        Err(_) => {
                            warn!("Could not resolve user '{}', defaulting to UID 0", name);
                            0
                        }
                    }
                }
            }
        }
    }
    
    // if there is no gid provided, use uid.
    let gid = gid.unwrap_or(uid);
    debug!("Final UID: {}, GID: {}", uid, gid);
    
    // https://github.com/topjohnwu/Magisk/blob/master/native/src/su/su_daemon.cpp#L408
    let arg0 = if is_login { "-" } else { &shell };
    debug!("arg0 for exec: {}", arg0);
    
    let mut command = &mut Command::new(&shell);
    command.env("KSU_LOG_LEVEL", "DEBUG"); // 确保子进程也能看到详细日志
    
    let mut envs = Vec::new();
    if !preserve_env {
        debug!("Clearing environment variables (preserve_env=false)");
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
                
            envs.push(("HOME".to_string(), home.to_string()));
            envs.push(("USER".to_string(), pw_name.to_string()));
            envs.push(("LOGNAME".to_string(), pw_name.to_string()));
            envs.push(("SHELL".to_string(), shell.clone()));
        }
    } else {
        // 收集当前环境变量用于日志
        for (key, value) in env::vars() {
            envs.push((key, value));
        }
    }
    
    // add /data/adb/ksu/bin to PATH
    match add_path_to_env(defs::BINARY_DIR) {
        Ok(_) => debug!("Successfully added {} to PATH", defs::BINARY_DIR),
        Err(e) => warn!("Failed to add {} to PATH: {:?}", defs::BINARY_DIR, e),
    };
    
    // when KSURC_PATH exists and ENV is not set, set ENV to KSURC_PATH
    if PathBuf::from(defs::KSURC_PATH).exists() && env::var("ENV").is_err() {
        debug!("Setting ENV to {}", defs::KSURC_PATH);
        command = command.env("ENV", defs::KSURC_PATH);
        envs.push(("ENV".to_string(), defs::KSURC_PATH.to_string()));
    }
    
    // 记录执行上下文
    log_execution_context(&shell, &args, &envs);
    
    // escape from the current cgroup and become session leader
    // WARNING!!! This cause some root shell hang forever!
    // command = command.process_group(0);
    
    command = unsafe {
        command.pre_exec(move || {
            umask(0o22);
            
            debug!("Pre-exec phase started");
            utils::switch_cgroups();
            debug!("Switched cgroups");
            
            // switch to global mount namespace if mount_master
            if mount_master {
                debug!("Switching to global mount namespace");
                match utils::switch_mnt_ns(1) {
                    Ok(_) => debug!("Successfully switched to global mount namespace"),
                    Err(e) => warn!("Failed to switch to global mount namespace: {:?}", e),
                };
            }
            
            if use_fd_wrapper {
                debug!("Wrapping TTY file descriptors");
                wrap_tty(0);
                wrap_tty(1);
                wrap_tty(2);
            } else {
                debug!("TTY wrapping disabled");
            }
            
            set_identity(uid, gid, &groups);
            debug!("Identity set, about to execute command");
            
            Result::Ok(())
        })
    };
    
    command = command.args(&args).arg0(arg0);
    
    // 在执行前检查shell是否存在
    if !PathBuf::from(&shell).exists() {
        let error_msg = format!("Shell not found at path: {}", shell);
        error!("{}", error_msg);
        let _ = std::io::stderr().write_all(error_msg.as_bytes());
        bail!(error_msg);
    }
    
    debug!("Executing command: {} with args {:?}", shell, args);
    match command.output() {
        Ok(output) => {
            debug!("Command executed successfully");
            debug!("stdout: {:?}", String::from_utf8_lossy(&output.stdout));
            debug!("stderr: {:?}", String::from_utf8_lossy(&output.stderr));
            debug!("status: {:?}", output.status);
            
            // 将输出写回标准输出/错误
            let _ = std::io::stdout().write_all(&output.stdout);
            let _ = std::io::stderr().write_all(&output.stderr);
            
            if !output.status.success() {
                let err_msg = format!("Shell command failed with status: {:?}", output.status);
                error!("{}", err_msg);
                bail!(err_msg);
            }
            
            Ok(())
        }
        Err(e) => {
            error!("Failed to execute shell command: {:?}", e);
            
            // 尝试获取更详细的错误信息
            if let std::io::ErrorKind::BrokenPipe = e.kind() {
                error!("Broken pipe error detected - this typically means the child process terminated unexpectedly");
                
                // 检查shell是否存在和可执行
                let shell_path = PathBuf::from(&shell);
                if !shell_path.exists() {
                    error!("Shell binary does not exist: {}", shell);
                } else if !shell_path.metadata().map(|m| m.permissions().mode() & 0o111 != 0).unwrap_or(false) {
                    error!("Shell binary is not executable: {}", shell);
                }
                
                // 检查父进程状态
                if let Ok(ppid) = std::fs::read_to_string("/proc/self/stat") {
                    if let Some(parent_pid) = ppid.split_whitespace().nth(3) {
                        debug!("Parent PID: {}", parent_pid);
                        if let Ok(parent_status) = std::fs::read_to_string(format!("/proc/{}/status", parent_pid)) {
                            for line in parent_status.lines().take(10) {
                                debug!("Parent process status: {}", line);
                            }
                        }
                    }
                }
            }
            
            Err(e.into())
        }
    }
}

fn add_path_to_env(path: &str) -> Result<()> {
    debug!("Adding path to environment: {}", path);
    let mut paths = env::var_os("PATH").map_or(Vec::new(), |val| env::split_paths(&val).collect::<Vec<_>>());
    let new_path = PathBuf::from(path.trim_end_matches('/'));
    
    // 检查路径是否已存在
    if paths.contains(&new_path) {
        debug!("Path already exists in PATH environment variable");
        return Ok(());
    }
    
    paths.push(new_path.clone());
    let new_path_env = env::join_paths(paths)?;
    debug!("New PATH value: {:?}", new_path_env);
    
    unsafe {
        env::set_var("PATH", new_path_env)
    };
    
    debug!("Successfully added path to environment");
    Ok(())
}
