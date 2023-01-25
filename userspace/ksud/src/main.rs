mod cli;
mod event;
mod module;
mod defs;
mod utils;
mod restorecon;
mod debug;
mod apk_sign;
mod ksu;

fn main() -> anyhow::Result<()> {
    cli::run()
}
