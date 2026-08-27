use std::ffi::CString;
use std::io::{ErrorKind, Write};

use anyhow::{Context, Result, ensure};
use rustix::fs::{Mode, symlink, unlink};
use rustix::{
    fd::AsFd,
    fs::{Access, CWD, FileType, access, makedev, mkdir, mknodat},
    mount::{
        FsMountFlags, FsOpenFlags, MountAttrFlags, MoveMountFlags, UnmountFlags, fsconfig_create,
        fsmount, fsopen, move_mount, unmount,
    },
};

struct AutoUmount {
    mountpoints: Vec<String>,
}

const KSU_CONFIG_PATH: &str = "/ksu_config";
const KSU_BLOCK_MODULES_PATH: &str = "/ksu_block_modules";
const KSU_BLOCK_MODULES_MAX_LEN: usize = 255;
const KSU_DEFAULT_BLOCK_MODULES: &str = "vr,vklp,oplus_secure_guard,oplus_secure_guard_new,mkp";

fn valid_block_modules(modules: &str) -> bool {
    modules.len() <= KSU_BLOCK_MODULES_MAX_LEN
        && (modules.is_empty()
            || modules.split(',').all(|name| {
                !name.is_empty()
                    && name
                        .bytes()
                        .all(|ch| ch.is_ascii_alphanumeric() || ch == b'_' || ch == b'-')
            }))
}

fn load_module_params() -> Result<CString> {
    let mut params = std::fs::read(KSU_CONFIG_PATH).unwrap_or_default();

    let blocked_modules = match std::fs::read_to_string(KSU_BLOCK_MODULES_PATH) {
        Ok(modules) => modules,
        Err(err) if err.kind() == ErrorKind::NotFound => KSU_DEFAULT_BLOCK_MODULES.to_owned(),
        Err(err) => {
            return Err(err).with_context(|| format!("Cannot read {KSU_BLOCK_MODULES_PATH}"));
        }
    };

    ensure!(
        valid_block_modules(&blocked_modules),
        "Invalid blocked preset module list"
    );
    if params
        .last()
        .is_some_and(|byte| !byte.is_ascii_whitespace())
    {
        params.push(b' ');
    }
    params.extend_from_slice(format!("block_modules={blocked_modules}").as_bytes());

    CString::new(params).context("KernelSU module parameters contain a NUL byte")
}

impl Drop for AutoUmount {
    fn drop(&mut self) {
        for mountpoint in self.mountpoints.iter().rev() {
            if let Err(e) = unmount(mountpoint.as_str(), UnmountFlags::DETACH) {
                log::error!("Cannot umount {}: {}", mountpoint, e)
            }
        }
    }
}

fn mount_filesystem(name: &str, mountpoint: &str) -> Result<()> {
    mkdir(mountpoint, Mode::from_raw_mode(0o755)).or_else(|err| match err.kind() {
        ErrorKind::AlreadyExists => Ok(()),
        _ => Err(err),
    })?;
    let fs_fd = fsopen(name, FsOpenFlags::FSOPEN_CLOEXEC)?;
    fsconfig_create(fs_fd.as_fd())?;
    let mount_fd = fsmount(
        fs_fd.as_fd(),
        FsMountFlags::FSMOUNT_CLOEXEC,
        MountAttrFlags::empty(),
    )?;
    move_mount(
        mount_fd.as_fd(),
        "",
        CWD,
        mountpoint,
        MoveMountFlags::MOVE_MOUNT_F_EMPTY_PATH,
    )?;
    Ok(())
}

fn prepare_mount() -> AutoUmount {
    let mut mountpoints = vec![];

    // mount procfs
    match mount_filesystem("proc", "/proc") {
        Ok(_) => mountpoints.push("/proc".to_string()),
        Err(e) => log::error!("Cannot mount procfs: {:?}", e),
    }

    AutoUmount { mountpoints }
}

fn setup_kmsg() {
    const KMSG: &str = "/dev/kmsg";
    let device = match access(KMSG, Access::EXISTS) {
        Ok(_) => KMSG,
        Err(_) => {
            // try to create it
            mknodat(
                CWD,
                "/kmsg",
                FileType::CharacterDevice,
                0o666.into(),
                makedev(1, 11),
            )
            .ok();
            "/kmsg"
        }
    };

    let _ = kernlog::init_with_device(device);
}

fn unlimit_kmsg() {
    // Disable kmsg rate limiting
    if let Ok(mut rate) = std::fs::File::options()
        .write(true)
        .open("/proc/sys/kernel/printk_devkmsg")
    {
        writeln!(rate, "on").ok();
    }
}

pub fn init() -> Result<()> {
    // Setup kernel log first
    setup_kmsg();

    log::info!("Hello, KernelSU!");

    // mount /proc to access kernel interface
    let _dontdrop = prepare_mount();

    // This relies on the fact that we have /proc mounted
    unlimit_kmsg();

    if ksuinit::has_kernelsu() {
        log::info!("KernelSU may be already loaded in kernel, skip!");
    } else {
        log::info!("Loading kernelsu.ko..");
        if let Err(e) = load_module_from_path("/kernelsu.ko") {
            log::error!("Cannot load kernelsu.ko: {:?}", e);
        }
    }

    // And now we should prepare the real init to transfer control to it
    unlink("/init")?;

    let real_init = match access("/init.real", Access::EXISTS) {
        Ok(_) => "init.real",
        Err(_) => "/system/bin/init",
    };

    log::info!("init is {}", real_init);
    symlink(real_init, "/init")?;

    Ok(())
}

fn load_module_from_path(path: &str) -> Result<()> {
    anyhow::ensure!(rustix::process::getpid().is_init(), "Invalid process");
    let buffer = std::fs::read(path).with_context(|| format!("Cannot read file {}", path))?;
    let params = load_module_params()?;
    log::info!("load kernelsu with params {params:?}");
    ksuinit::load_module(&buffer, &params)
}
