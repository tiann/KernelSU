use anyhow::Result;
use log::{error, info, warn};

use crate::{ksu_uapi, ksucalls, logger::set_stdio_log_max_level, utils};

pub fn unload() -> Result<()> {
    set_stdio_log_max_level(log::LevelFilter::Info);
    if (ksucalls::get_info().flags & ksu_uapi::KSU_GET_INFO_FLAG_UNLOADABLE) == 0 {
        error!("KernelSU is not unloadable!");
        std::process::exit(1);
    }

    // TODO: guard unload by supercall

    if ksucalls::check_wrapper_fd(0).is_ok_and(|x| x)
        || ksucalls::check_wrapper_fd(1).is_ok_and(|x| x)
        || ksucalls::check_wrapper_fd(2).is_ok_and(|x| x)
    {
        error!("Please restart root shell with su -W!");
        std::process::exit(1);
    }

    info!("unload: starting KernelSU unload sequence");

    // 1. Switch cgroups so we don't get killed along with our parent shell
    utils::switch_cgroups();

    // 2. delete_module("kernelsu")
    info!("unload: removing kernelsu module...");
    if let Err(e) = rustix::system::delete_module(c"kernelsu", 0) {
        warn!("unload: delete_module kernelsu failed: {e}");
    }

    // 3. Exit
    info!("unload: done, exiting ksud");
    std::process::exit(0);
}
