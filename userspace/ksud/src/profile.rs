use crate::defs;
use crate::utils::ensure_dir_exists;
use anyhow::Result;
use std::path::Path;

pub fn set_sepolicy(pkg: String, policy: String) -> Result<()> {
    ensure_dir_exists(defs::PROFILE_SELINUX_DIR)?;
    let policy_file = Path::new(defs::PROFILE_SELINUX_DIR).join(pkg);
    std::fs::write(policy_file, policy)?;
    Ok(())
}

pub fn get_sepolicy(pkg: String) -> Result<()> {
    ensure_dir_exists(defs::PROFILE_SELINUX_DIR)?;
    let policy_file = Path::new(defs::PROFILE_SELINUX_DIR).join(pkg);
    let policy = std::fs::read_to_string(policy_file)?;
    println!("{policy}");
    Ok(())
}

pub fn set_template(name: String, template: String) -> Result<()> {
    ensure_dir_exists(defs::PROFILE_TEMPLATE_DIR)?;
    let template_file = Path::new(defs::PROFILE_TEMPLATE_DIR).join(name);
    std::fs::write(template_file, template)?;
    Ok(())
}

pub fn get_template(name: String) -> Result<()> {
    ensure_dir_exists(defs::PROFILE_TEMPLATE_DIR)?;
    let template_file = Path::new(defs::PROFILE_TEMPLATE_DIR).join(name);
    let template = std::fs::read_to_string(template_file)?;
    println!("{template}");
    Ok(())
}

pub fn list_templates() -> Result<()> {
    let templates = std::fs::read_dir(defs::PROFILE_TEMPLATE_DIR)?;
    for template in templates {
        let template = template?;
        let template = template.file_name();
        if let Some(template) = template.to_str() {
            println!("{template}");
        };
    }
    Ok(())
}
