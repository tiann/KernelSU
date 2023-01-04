mod cli;
mod event;
mod module;
mod defs;
mod utils;
mod restorecon;

fn main() -> anyhow::Result<()> {
    cli::run()
}
