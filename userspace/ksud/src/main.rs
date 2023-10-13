mod apk_sign;
mod assets;
mod cli;
mod debug;
mod defs;
mod ksu;
mod module;
mod module_api;
mod mount;
mod profile;
mod restorecon;
mod sepolicy;
mod utils;

fn main() -> anyhow::Result<()> {
    cli::run()
}
