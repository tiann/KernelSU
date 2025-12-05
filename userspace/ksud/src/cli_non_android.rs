use anyhow::{Context, Ok, Result};
use clap::Parser;
use std::path::PathBuf;

use crate::boot_patch::{BootPatchArgs, BootRestoreArgs};
use crate::{apk_sign, assets, defs};

/// KernelSU cli for non-android
#[derive(Parser, Debug)]
#[command(author, version = defs::VERSION_NAME, about, long_about = None)]
struct Args {
    #[command(subcommand)]
    command: Commands,
}

#[derive(clap::Subcommand, Debug)]
enum Commands {
    /// Patch boot or init_boot images to apply KernelSU
    BootPatch(BootPatchArgs),

    /// Restore boot or init_boot images patched by KernelSU
    BootRestore(BootRestoreArgs),

    /// For developers
    Debug {
        #[command(subcommand)]
        command: Debug,
    },
}

#[derive(clap::Subcommand, Debug)]
enum Debug {
    /// Get apk size and hash
    GetSign {
        /// apk path
        apk: String,
    },
}

pub fn run() -> Result<()> {
    env_logger::init();

    let cli = Args::parse();

    log::info!("command: {:?}", cli.command);

    let result = match cli.command {
        Commands::Debug { command } => match command {
            Debug::GetSign { apk } => {
                let sign = apk_sign::get_apk_signature(&apk)?;
                println!("size: {:#x}, hash: {}", sign.0, sign.1);
                Ok(())
            }
        },

        Commands::BootPatch(boot_patch) => crate::boot_patch::patch(boot_patch),

        Commands::BootRestore(boot_restore) => crate::boot_patch::restore(boot_restore),
    };

    if let Err(e) = &result {
        log::error!("Error: {e:?}");
    }
    result
}
