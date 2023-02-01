mod apk_sign;
mod cli;
mod debug;
mod defs;
mod event;
mod ksu;
mod module;
mod restorecon;
mod utils;
mod sepolicy;
mod assets;

fn main() -> anyhow::Result<()> {
    cli::run()
}
