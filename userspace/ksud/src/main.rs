mod apk_sign;
mod assets;
mod boot_patch;
mod cli;
mod debug;
mod defs;
mod init_event;
mod modsys;
mod profile;
mod su;
mod utils;

fn main() -> anyhow::Result<()> {
    cli::run()
}
