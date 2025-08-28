use anyhow::Result;
use clap::Parser;

mod cli;
mod defs;
mod module;
mod mount;
mod stage;
mod supported;
mod utils;

#[cfg(target_os = "android")]
use android_logger::Config;
#[cfg(target_os = "android")]
use log::LevelFilter;

/// KernelSU Module System - Overlay Implementation
#[derive(Parser, Debug)]
#[command(author, version = "0.1.0", about, long_about = None)]
struct Args {
    #[command(subcommand)]
    command: cli::Commands,
}

fn main() -> Result<()> {
    #[cfg(target_os = "android")]
    android_logger::init_once(
        Config::default()
            .with_max_level(LevelFilter::Trace)
            .with_tag("KSU-ModSys"),
    );

    #[cfg(not(target_os = "android"))]
    env_logger::init();

    let cli = Args::parse();
    log::info!("ksu-modsys-overlay command: {:?}", cli.command);

    cli::run(cli.command)
}
