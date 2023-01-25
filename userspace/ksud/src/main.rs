mod cli;
mod event;
mod module;
mod defs;
mod utils;
mod restorecon;
mod debug;
mod apk_sign;

fn main() -> anyhow::Result<()> {
    cli::run()
}
