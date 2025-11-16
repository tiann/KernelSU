use anyhow::Result;
use log::info;

mod defs;
mod mount;

fn main() -> Result<()> {
    // Initialize logger
    env_logger::builder()
        .filter_level(log::LevelFilter::Info)
        .init();

    info!(
        "KernelSU Official Mount Handler v{}",
        env!("CARGO_PKG_VERSION")
    );

    // Get module directory from environment variable or use default
    let module_dir = std::env::var("MODULE_DIR").unwrap_or_else(|_| defs::MODULE_DIR.to_string());

    info!("Module directory: {}", module_dir);

    // Execute mounting
    mount::mount_modules_systemlessly(&module_dir)?;

    info!("Mount completed successfully");
    Ok(())
}
