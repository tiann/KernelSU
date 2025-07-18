use crate::utils::ensure_dir_exists;
use crate::{defs, sepolicy};
use anyhow::{Context, Result};
use std::path::Path;

pub fn set_sepolicy(pkg: String, policy: String) -> Result<()> {
    ensure_dir_exists(defs::PROFILE_SELINUX_DIR)?;
    let policy_file = Path::new(defs::PROFILE_SELINUX_DIR).join(pkg);
    std::fs::write(&policy_file, policy)?;
    sepolicy::apply_file(&policy_file)?;
    Ok(())
}

pub fn get_sepolicy(pkg: String) -> Result<()> {
    let policy_file = Path::new(defs::PROFILE_SELINUX_DIR).join(pkg);
    let policy = std::fs::read_to_string(policy_file)?;
    println!("{policy}");
    Ok(())
}

// ksud doesn't guarteen the correctness of template, it just save
pub fn set_template(id: String, template: String) -> Result<()> {
    ensure_dir_exists(defs::PROFILE_TEMPLATE_DIR)?;
    let template_file = Path::new(defs::PROFILE_TEMPLATE_DIR).join(id);
    std::fs::write(template_file, template)?;
    Ok(())
}

pub fn get_template(id: String) -> Result<()> {
    let template_file = Path::new(defs::PROFILE_TEMPLATE_DIR).join(id);
    let template = std::fs::read_to_string(template_file)?;
    println!("{template}");
    Ok(())
}

pub fn delete_template(id: String) -> Result<()> {
    let template_file = Path::new(defs::PROFILE_TEMPLATE_DIR).join(id);
    std::fs::remove_file(template_file)?;
    Ok(())
}

pub fn list_templates() -> Result<()> {
    let templates = std::fs::read_dir(defs::PROFILE_TEMPLATE_DIR);
    let Ok(templates) = templates else {
        return Ok(());
    };
    for template in templates {
        let template = template?;
        let template = template.file_name();
        if let Some(template) = template.to_str() {
            println!("{template}");
        };
    }
    Ok(())
}

pub fn apply_sepolies() -> Result<()> {
    let path = Path::new(defs::PROFILE_SELINUX_DIR);
    if !path.exists() {
        log::info!("profile sepolicy dir not exists.");
        return Ok(());
    }

    let sepolicies =
        std::fs::read_dir(path).with_context(|| "profile sepolicy dir open failed.".to_string())?;
    for sepolicy in sepolicies {
        let Ok(sepolicy) = sepolicy else {
            log::info!("profile sepolicy dir read failed.");
            continue;
        };
        let sepolicy = sepolicy.path();
        if sepolicy::apply_file(&sepolicy).is_ok() {
            log::info!("profile sepolicy applied: {sepolicy:?}");
        } else {
            log::info!("profile sepolicy apply failed: {sepolicy:?}");
        }
    }
    Ok(())
}
