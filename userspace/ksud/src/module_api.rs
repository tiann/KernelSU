use anyhow::Result;
use log::info;
use std::{os::unix::process::CommandExt, process::Command};
use derive_new::new;

use crate::module::KsuModuleApi;

pub trait ModuleApi {
    fn on_post_data_fs(&self) -> Result<()>;
    fn on_boot_completed(&self) -> Result<()>;
    fn on_services(&self) -> Result<()>;

    fn install_module(&self, zip: &str) -> Result<()>;
    fn uninstall_module(&self, id: &str) -> Result<()>;
    fn enable_module(&self, id: &str) -> Result<()>;
    fn disable_module(&self, id: &str) -> Result<()>;
    fn list_modules(&self) -> Result<()>;
    fn mount_modules(&self) -> Result<()>;
}

const fn should_use_external_module_api() -> bool {
    false
}

#[derive(new)]
struct ModuleApiProxy {}

pub fn get_api() -> impl ModuleApi {
    ModuleApiProxy::new()
}

fn get_impl() -> Box<dyn ModuleApi> {
    if should_use_external_module_api() {
        Box::new(ExternalModuleApi {})
    } else {
        Box::new(KsuModuleApi {})
    }
}

impl ModuleApi for ModuleApiProxy {
    fn on_post_data_fs(&self) -> Result<()> {
        crate::ksu::report_post_fs_data();
        info!("on_post_data_fs triggered!");
        get_impl().on_post_data_fs()
    }

    fn on_boot_completed(&self) -> Result<()> {
        crate::ksu::report_boot_complete();
        info!("on_boot_completed triggered!");
        get_impl().on_boot_completed()
    }

    fn on_services(&self) -> Result<()> {
        info!("on_services triggered!");
        get_impl().on_services()
    }

    fn install_module(&self, zip: &str) -> Result<()> {
        get_impl().install_module(zip)
    }

    fn uninstall_module(&self, id: &str) -> Result<()> {
        get_impl().install_module(id)
    }

    fn enable_module(&self, id: &str) -> Result<()> {
        get_impl().install_module(id)
    }

    fn disable_module(&self, id: &str) -> Result<()> {
        get_impl().install_module(id)
    }

    fn list_modules(&self) -> Result<()> {
        get_impl().list_modules()
    }

    fn mount_modules(&self) -> Result<()> {
        get_impl().mount_modules()
    }
}

struct ExternalModuleApi;

impl ModuleApi for ExternalModuleApi {
    fn on_post_data_fs(&self) -> Result<()> {
        Err(Command::new("/data/adb/ksu/module")
            .arg("post-fs-data")
            .exec()
            .into())
    }
    fn on_boot_completed(&self) -> Result<()> {
        Err(Command::new("/data/adb/ksu/module")
            .arg("boot-completed")
            .exec()
            .into())
    }
    fn on_services(&self) -> Result<()> {
        Err(Command::new("/data/adb/ksu/module")
            .arg("services")
            .exec()
            .into())
    }
    fn install_module(&self, zip: &str) -> Result<()> {
        Err(Command::new("/data/adb/ksu/module")
            .args(["install", zip])
            .exec()
            .into())
    }
    fn uninstall_module(&self, id: &str) -> Result<()> {
        Err(Command::new("/data/adb/ksu/module")
            .args(["uninstall", id])
            .exec()
            .into())
    }
    fn enable_module(&self, id: &str) -> Result<()> {
        Err(Command::new("/data/adb/ksu/module")
            .args(["enable", id])
            .exec()
            .into())
    }
    fn disable_module(&self, id: &str) -> Result<()> {
        Err(Command::new("/data/adb/ksu/module")
            .args(["disable", id])
            .exec()
            .into())
    }
    fn list_modules(&self) -> Result<()> {
        Err(Command::new("/data/adb/ksu/module")
            .args(["list"])
            .exec()
            .into())
    }
    fn mount_modules(&self) -> Result<()> {
        Err(Command::new("/data/adb/ksu/module")
            .args(["mount"])
            .exec()
            .into())
    }
}
