use anyhow::Result;
use clap::Subcommand;

use crate::{module, mount, stage, supported};

#[derive(Subcommand, Debug)]
pub enum Commands {
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

    /// Enable module <id>
    Enable {
        /// module id  
        id: String,
    },

    /// Disable module <id>
    Disable {
        /// module id
        id: String,
    },

    /// Run action for module <id>
    Action {
        /// module id
        id: String,
    },

    /// List all modules
    List,

    /// Shrink module image size
    Shrink,

    /// Execute stage scripts
    Stage {
        /// Stage name: post-fs-data, service, boot-completed
        stage: String,
    },

    /// Mount systemlessly  
    Mount {
        /// Mount command
        #[command(subcommand)]
        command: MountCommand,
    },

    /// Check if this modsys implementation is supported
    #[command(long_flag = "supported")]
    Supported,

    /// Debug utilities
    Debug {
        #[command(subcommand)]
        command: Debug,
    },
}

#[derive(Subcommand, Debug)]
pub enum MountCommand {
    /// Mount modules systemlessly
    Systemless,
}

#[derive(Subcommand, Debug)]
pub enum Debug {
    /// Copy sparse file efficiently (debug)
    Xcp {
        /// source file
        src: String,
        /// destination file
        dst: String,
        /// punch hole when possible
        #[arg(short, long, default_value = "false")]
        punch_hole: bool,
    },
}

pub fn run(command: Commands) -> Result<()> {
    match command {
        Commands::Install { zip } => module::install_module(&zip),
        Commands::Uninstall { id } => module::uninstall_module(&id),
        Commands::Enable { id } => module::enable_module(&id),
        Commands::Disable { id } => module::disable_module(&id),
        Commands::Action { id } => module::run_action(&id),
        Commands::List => module::list_modules(),
        Commands::Shrink => module::shrink_ksu_images(),
        Commands::Stage { stage } => stage::run_stage(&stage),
        Commands::Mount { command } => match command {
            MountCommand::Systemless => mount::mount_modules_systemlessly(),
        },
        Commands::Supported => supported::check_supported(),
        Commands::Debug { command } => match command {
            Debug::Xcp {
                src,
                dst,
                punch_hole,
            } => crate::utils::copy_sparse_file(src, dst, punch_hole),
        },
    }
}
