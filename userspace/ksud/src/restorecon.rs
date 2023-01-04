use anyhow::ensure;
use anyhow::Ok;
use anyhow::Result;
use subprocess::Exec;

const SYSTEM_CON: &str = "u:object_r:system_file:s0";
const _ADB_CON: &str = "u:object_r:adb_data_file:s0";

pub fn restore_syscon(dir: &str) -> Result<()> {
    // todo use libselinux directly
    let cmd = format!("chcon -R {} {}", SYSTEM_CON, dir);
    let result = Exec::shell(cmd).join()?;
    ensure!(result.success(), "chcon for: {} failed.", dir);
    Ok(())
}