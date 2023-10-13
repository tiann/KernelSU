use anyhow::Result;
use log::info;
use std::sync::Once;

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

pub struct ModuleApiProxy {
    api: Box<dyn ModuleApi>,
}

// TODO: remove unsafe
impl ModuleApiProxy {
    pub fn get() -> &'static ModuleApiProxy {
        static mut INSTANCE: *const ModuleApiProxy = std::ptr::null();
        static ONCE: Once = Once::new();

        ONCE.call_once(|| {
            let instance: ModuleApiProxy = ModuleApiProxy {
                api: Box::new(KsuModuleApi {}),
            };

            unsafe {
                INSTANCE = std::mem::transmute(Box::new(instance));
            }
        });

        unsafe { &*INSTANCE }
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
