pub fn umask(mask: u32) {
    use rustix::process;
    process::umask(rustix::fs::Mode::from_raw_mode(mask));
}

#[cfg(any(target_os = "linux", target_os = "android"))]
pub fn switch_mnt_ns(pid: i32) -> Result<(), Box<dyn core::error::Error>> {
    use rustix::{
        fd::AsFd,
        fs::{Mode, OFlags, open},
        thread::{move_into_link_name_space, LinkNameSpaceType},
    };
    let path = format!("/proc/{pid}/ns/mnt");
    let fd = open(path, OFlags::RDONLY, Mode::from_raw_mode(0))?;
    let current_dir = std::env::current_dir();
    move_into_link_name_space(fd.as_fd(), Some(LinkNameSpaceType::Mount))?;
    if let Ok(current_dir) = current_dir {
        let _ = std::env::set_current_dir(current_dir);
    }
    Ok(())
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
pub fn switch_mnt_ns(_pid: i32) -> Result<(), Box<dyn core::error::Error>> {
    Ok(())
}

use std::path::Path;
use std::fs::OpenOptions;
use std::io::Write;

fn switch_cgroup(grp: &str, pid: u32) {
    let path = Path::new(grp).join("cgroup.procs");
    if !path.exists() {
        return;
    }
    if let Ok(mut fp) = OpenOptions::new().append(true).open(path) {
        let _ = writeln!(fp, "{pid}");
    }
}

pub fn switch_cgroups() {
    let pid = std::process::id();
    switch_cgroup("/acct", pid);
    switch_cgroup("/dev/cg2_bpf", pid);
    switch_cgroup("/sys/fs/cgroup", pid);

    if crate::props::getprop("ro.config.per_app_memcg")
        .filter(|prop| prop == "false")
        .is_none()
    {
        switch_cgroup("/dev/memcg/apps", pid);
    }
}
