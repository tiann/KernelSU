use anyhow::Result;
use clap::Parser;

use crate::{event, module};

/// KernelSU userspace cli
#[derive(Parser, Debug)]
#[command(author, version, about, long_about = None)]
struct Args {
    #[command(subcommand)]
    command: Commands,
}

#[derive(clap::Subcommand, Debug)]
enum Commands {
    /// Start KernelSU userspace daemon
    Daemon,

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
    Install,

    /// SELinux policy Patch tool
    Sepolicy,

    /// For test
    Test,
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

    /// list all modules
    List,
}

pub fn run() -> Result<()> {
    let cli = Args::parse();

    let result = match cli.command {
        Commands::Daemon => event::daemon(),
        Commands::PostFsData => event::on_post_data_fs(),
        Commands::BootCompleted => event::on_boot_completed(),

        Commands::Module { command } => {
            env_logger::init();

            match command {
                Module::Install { zip } => module::install_module(zip),
                Module::Uninstall { id } => module::uninstall_module(id),
                Module::Enable { id } => module::enable_module(id),
                Module::Disable { id } => module::disable_module(id),
                Module::List => module::list_modules(),
            }
        }
        Commands::Install => event::install(),
        Commands::Sepolicy => todo!(),
        Commands::Services => event::on_services(),
        Commands::Test => event::do_systemless_mount("/data/adb/ksu/modules"),
    };

    if let Err(e) = &result {
        log::error!("Error: {}", e);
    }
    result
}
