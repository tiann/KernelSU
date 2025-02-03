use anyhow::{Ok, Result};
use clap::Parser;
use std::path::PathBuf;

#[cfg(target_os = "android")]
use android_logger::Config;
#[cfg(target_os = "android")]
use log::LevelFilter;

use crate::{apk_sign, assets, debug, defs, init_event, ksucalls, module, utils};

/// KernelSU userspace cli
#[derive(Parser, Debug)]
#[command(author, version = defs::VERSION_NAME, about, long_about = None)]
struct Args {
    #[command(subcommand)]
    command: Commands,
}

#[derive(clap::Subcommand, Debug)]
enum Commands {
    /// Manage KernelSU modules
    Module {
        #[command(subcommand)]
        command: Module,
    },

    /// Trigger `post-fs-data` event
    PostFsData,

    /// Trigger `service` event
    Services,

    /// Trigger `boot-complete` event
    BootCompleted,

    /// Install KernelSU userspace component to system
    Install {
        #[arg(long, default_value = None)]
        magiskboot: Option<PathBuf>,
    },

    /// Uninstall KernelSU modules and itself(LKM Only)
    Uninstall {
        /// magiskboot path, if not specified, will search from $PATH
        #[arg(long, default_value = None)]
        magiskboot: Option<PathBuf>,
    },

    /// SELinux policy Patch tool
    Sepolicy {
        #[command(subcommand)]
        command: Sepolicy,
    },

    /// Manage App Profiles
    Profile {
        #[command(subcommand)]
        command: Profile,
    },

    /// Patch boot or init_boot images to apply KernelSU
    BootPatch {
        /// boot image path, if not specified, will try to find the boot image automatically
        #[arg(short, long)]
        boot: Option<PathBuf>,

        /// kernel image path to replace
        #[arg(short, long)]
        kernel: Option<PathBuf>,

        /// LKM module path to replace, if not specified, will use the builtin one
        #[arg(short, long)]
        module: Option<PathBuf>,

        /// init to be replaced
        #[arg(short, long, requires("module"))]
        init: Option<PathBuf>,

        /// will use another slot when boot image is not specified
        #[arg(short = 'u', long, default_value = "false")]
        ota: bool,

        /// Flash it to boot partition after patch
        #[arg(short, long, default_value = "false")]
        flash: bool,

        /// output path, if not specified, will use current directory
        #[arg(short, long, default_value = None)]
        out: Option<PathBuf>,

        /// magiskboot path, if not specified, will search from $PATH
        #[arg(long, default_value = None)]
        magiskboot: Option<PathBuf>,

        /// KMI version, if specified, will use the specified KMI
        #[arg(long, default_value = None)]
        kmi: Option<String>,
    },

    /// Restore boot or init_boot images patched by KernelSU
    BootRestore {
        /// boot image path, if not specified, will try to find the boot image automatically
        #[arg(short, long)]
        boot: Option<PathBuf>,

        /// Flash it to boot partition after patch
        #[arg(short, long, default_value = "false")]
        flash: bool,

        /// magiskboot path, if not specified, will search from $PATH
        #[arg(long, default_value = None)]
        magiskboot: Option<PathBuf>,
    },

    /// Show boot information
    BootInfo {
        #[command(subcommand)]
        command: BootInfo,
    },
    /// For developers
    Debug {
        #[command(subcommand)]
        command: Debug,
    },
}

#[derive(clap::Subcommand, Debug)]
enum BootInfo {
    /// show current kmi version
    CurrentKmi,

    /// show supported kmi versions
    SupportedKmi,
}

#[derive(clap::Subcommand, Debug)]
enum Debug {
    /// Set the manager app, kernel CONFIG_KSU_DEBUG should be enabled.
    SetManager {
        /// manager package name
        #[arg(default_value_t = String::from("me.weishu.kernelsu"))]
        apk: String,
    },

    /// Get apk size and hash
    GetSign {
        /// apk path
        apk: String,
    },

    /// Root Shell
    Su {
        /// switch to gloabl mount namespace
        #[arg(short, long, default_value = "false")]
        global_mnt: bool,
    },

    /// Get kernel version
    Version,

    Mount,

    /// Copy sparse file
    Xcp {
        /// source file
        src: String,
        /// destination file
        dst: String,
        /// punch hole
        #[arg(short, long, default_value = "false")]
        punch_hole: bool,
    },

    /// For testing
    Test,
}

#[derive(clap::Subcommand, Debug)]
enum Sepolicy {
    /// Patch sepolicy
    Patch {
        /// sepolicy statements
        sepolicy: String,
    },

    /// Apply sepolicy from file
    Apply {
        /// sepolicy file path
        file: String,
    },

    /// Check if sepolicy statement is supported/valid
    Check {
        /// sepolicy statements
        sepolicy: String,
    },
}

#[derive(clap::Subcommand, Debug)]
enum Module {
    /// Install module <ZIP>
    Install {
        /// module zip file path
        zip: String,
    },

    /// Uninstall module <id>
    Uninstall {
        /// module id
        id: String,
    },

    /// enable module <id>
    Enable {
        /// module id
        id: String,
    },

    /// disable module <id>
    Disable {
        // module id
        id: String,
    },

    /// run action for module <id>
    Action {
        // module id
        id: String,
    },

    /// list all modules
    List,

    /// Shrink module image size
    Shrink,
}

#[derive(clap::Subcommand, Debug)]
enum Profile {
    /// get root profile's selinux policy of <package-name>
    GetSepolicy {
        /// package name
        package: String,
    },

    /// set root profile's selinux policy of <package-name> to <profile>
    SetSepolicy {
        /// package name
        package: String,
        /// policy statements
        policy: String,
    },

    /// get template of <id>
    GetTemplate {
        /// template id
        id: String,
    },

    /// set template of <id> to <template string>
    SetTemplate {
        /// template id
        id: String,
        /// template string
        template: String,
    },

    /// delete template of <id>
    DeleteTemplate {
        /// template id
        id: String,
    },

    /// list all templates
    ListTemplates,
}

pub fn run() -> Result<()> {
    #[cfg(target_os = "android")]
    android_logger::init_once(
        Config::default()
            .with_max_level(LevelFilter::Trace) // limit log level
            .with_tag("KernelSU"), // logs will show under mytag tag
    );

    #[cfg(not(target_os = "android"))]
    env_logger::init();

    // the kernel executes su with argv[0] = "su" and replace it with us
    let arg0 = std::env::args().next().unwrap_or_default();
    if arg0 == "su" || arg0 == "/system/bin/su" {
        return crate::su::root_shell();
    }

    let cli = Args::parse();

    log::info!("command: {:?}", cli.command);

    let result = match cli.command {
        Commands::PostFsData => init_event::on_post_data_fs(),
        Commands::BootCompleted => init_event::on_boot_completed(),

        Commands::Module { command } => {
            #[cfg(any(target_os = "linux", target_os = "android"))]
            {
                utils::switch_mnt_ns(1)?;
            }
            match command {
                Module::Install { zip } => module::install_module(&zip),
                Module::Uninstall { id } => module::uninstall_module(&id),
                Module::Enable { id } => module::enable_module(&id),
                Module::Disable { id } => module::disable_module(&id),
                Module::Action { id } => module::run_action(&id),
                Module::List => module::list_modules(),
                Module::Shrink => module::shrink_ksu_images(),
            }
        }
        Commands::Install { magiskboot } => utils::install(magiskboot),
        Commands::Uninstall { magiskboot } => utils::uninstall(magiskboot),
        Commands::Sepolicy { command } => match command {
            Sepolicy::Patch { sepolicy } => crate::sepolicy::live_patch(&sepolicy),
            Sepolicy::Apply { file } => crate::sepolicy::apply_file(file),
            Sepolicy::Check { sepolicy } => crate::sepolicy::check_rule(&sepolicy),
        },
        Commands::Services => init_event::on_services(),
        Commands::Profile { command } => match command {
            Profile::GetSepolicy { package } => crate::profile::get_sepolicy(package),
            Profile::SetSepolicy { package, policy } => {
                crate::profile::set_sepolicy(package, policy)
            }
            Profile::GetTemplate { id } => crate::profile::get_template(id),
            Profile::SetTemplate { id, template } => crate::profile::set_template(id, template),
            Profile::DeleteTemplate { id } => crate::profile::delete_template(id),
            Profile::ListTemplates => crate::profile::list_templates(),
        },

        Commands::Debug { command } => match command {
            Debug::SetManager { apk } => debug::set_manager(&apk),
            Debug::GetSign { apk } => {
                let sign = apk_sign::get_apk_signature(&apk)?;
                println!("size: {:#x}, hash: {}", sign.0, sign.1);
                Ok(())
            }
            Debug::Version => {
                println!("Kernel Version: {}", ksucalls::get_version());
                Ok(())
            }
            Debug::Su { global_mnt } => crate::su::grant_root(global_mnt),
            Debug::Mount => init_event::mount_modules_systemlessly(defs::MODULE_DIR),
            Debug::Xcp {
                src,
                dst,
                punch_hole,
            } => {
                utils::copy_sparse_file(src, dst, punch_hole)?;
                Ok(())
            }
            Debug::Test => assets::ensure_binaries(false),
        },

        Commands::BootPatch {
            boot,
            init,
            kernel,
            module,
            ota,
            flash,
            out,
            magiskboot,
            kmi,
        } => crate::boot_patch::patch(boot, kernel, module, init, ota, flash, out, magiskboot, kmi),

        Commands::BootInfo { command } => match command {
            BootInfo::CurrentKmi => {
                let kmi = crate::boot_patch::get_current_kmi()?;
                println!("{}", kmi);
                // return here to avoid printing the error message
                return Ok(());
            }
            BootInfo::SupportedKmi => {
                let kmi = crate::assets::list_supported_kmi()?;
                kmi.iter().for_each(|kmi| println!("{}", kmi));
                return Ok(());
            }
        },
        Commands::BootRestore {
            boot,
            magiskboot,
            flash,
        } => crate::boot_patch::restore(boot, magiskboot, flash),
    };

    if let Err(e) = &result {
        log::error!("Error: {:?}", e);
    }
    result
}
