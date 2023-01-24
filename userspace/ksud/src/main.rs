mod cli;
mod event;
mod module;
mod defs;
mod utils;
mod restorecon;
mod debug;

fn main() -> anyhow::Result<()> {
    cli::run()
}
