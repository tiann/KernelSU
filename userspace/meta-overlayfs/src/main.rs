use anyhow::Result;
use log::info;

mod defs;
mod mount;

fn main() -> Result<()> {
    // Initialize logger
    env_logger::builder()
        .filter_level(log::LevelFilter::Info)
        .init();

    info!("meta-overlayfs v{}", env!("CARGO_PKG_VERSION"));

    // Dual-directory support: metadata + content
    let metadata_dir = std::env::var("MODULE_METADATA_DIR")
        .unwrap_or_else(|_| defs::MODULE_METADATA_DIR.to_string());
    let content_dir = std::env::var("MODULE_CONTENT_DIR")
        .unwrap_or_else(|_| defs::MODULE_CONTENT_DIR.to_string());

    info!("Metadata directory: {}", metadata_dir);
    info!("Content directory: {}", content_dir);

    // Execute dual-directory mounting
    mount::mount_modules_systemlessly(&metadata_dir, &content_dir)?;

    info!("Mount completed successfully");
    Ok(())
}
