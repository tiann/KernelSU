use anyhow::Result;
use lazy_static::lazy_static;
use log::info;
use std::{process::Command, os::unix::process::CommandExt};

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

fn should_use_external_module_api() -> bool{
    false
}
pub struct ModuleApiProxy {
    api: Box<dyn ModuleApi + Send + Sync>,
}

lazy_static! {
    static ref MODULE_API_PROXY: ModuleApiProxy = if should_use_external_module_api() {
        ModuleApiProxy {
            api: Box::new(ExternalModuleApi {}),
        }
    } else {
        ModuleApiProxy {
            api: Box::new(KsuModuleApi {}),
        }
    };
}

impl ModuleApiProxy {
    pub fn get() -> &'static ModuleApiProxy {
        &MODULE_API_PROXY
    }
}

impl ModuleApi for ModuleApiProxy {
    fn on_post_data_fs(&self) -> Result<()> {
        crate::ksu::report_post_fs_data();
        info!("on_post_data_fs triggered!");
        self.api.on_post_data_fs()
    }

    fn on_boot_completed(&self) -> Result<()> {
        crate::ksu::report_boot_complete();
        info!("on_boot_completed triggered!");
        self.api.on_boot_completed()
    }

    fn on_services(&self) -> Result<()> {
        info!("on_services triggered!");
        self.api.on_services()
    }

    fn install_module(&self, zip: &str) -> Result<()> {
        self.api.install_module(zip)
    }

    fn uninstall_module(&self, id: &str) -> Result<()> {
        self.api.install_module(id)
    }

    fn enable_module(&self, id: &str) -> Result<()> {
        self.api.install_module(id)
    }

    fn disable_module(&self, id: &str) -> Result<()> {
        self.api.install_module(id)
    }

    fn list_modules(&self) -> Result<()> {
        self.api.list_modules()
    }

    fn mount_modules(&self) -> Result<()> {
        self.api.mount_modules()
    }
}

struct ExternalModuleApi;

impl ModuleApi for ExternalModuleApi{
    fn on_post_data_fs(&self) -> Result<()>{
        Err(Command::new("/data/adb/ksu/module").args("post-fs-data").exec().into())
    }
    fn on_boot_completed(&self) -> Result<()>{
        Err(Command::new("/data/adb/ksu/module").args("boot-completed").exec().into())
    }
    fn on_services(&self) -> Result<()>{
        Err(Command::new("/data/adb/ksu/module").args("services").exec().into())
    }
    fn install_module(&self, zip: &str) -> Result<()>{
        Err(Command::new("/data/adb/ksu/module").args(["install", zip]).exec().into())
    }
    fn uninstall_module(&self, id: &str) -> Result<()>{
        Err(Command::new("/data/adb/ksu/module").args(["uninstall", id]).exec().into())
    }
    fn enable_module(&self, id: &str) -> Result<()>{
        Err(Command::new("/data/adb/ksu/module").args(["enable", id]).exec().into())
    }
    fn disable_module(&self, id: &str) -> Result<()>{
        Err(Command::new("/data/adb/ksu/module").args(["disable", id]).exec().into())
    }
    fn list_modules(&self) -> Result<()>{
        Err(Command::new("/data/adb/ksu/module").args(["list"]).exec().into())
    }
    fn mount_modules(&self) -> Result<()>{
        Err(Command::new("/data/adb/ksu/module").args(["mount"]).exec().into())
    }
}