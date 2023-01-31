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

fn main() -> anyhow::Result<()> {
    cli::run()
}
