use crate::utils::ensure_dir_exists;
use crate::{defs, sepolicy};
use anyhow::{Context, Result, ensure};
use std::path::{Path, PathBuf};

fn profile_path(dir: &str, name: &str) -> Result<PathBuf> {
    // Keep names in one directory and on one line when listed
    ensure!(
        !name.is_empty()
            && name != "."
            && name != ".."
            && !name.chars().any(|c| matches!(c, '/' | '\0' | '\n' | '\r')),
        "Invalid profile name: {name:?}"
    );
    Ok(Path::new(dir).join(name))
}

pub fn set_sepolicy(pkg: &str, policy: String) -> Result<()> {
    let policy_file = profile_path(defs::PROFILE_SELINUX_DIR, pkg)?;
    ensure_dir_exists(defs::PROFILE_SELINUX_DIR)?;
    std::fs::write(&policy_file, policy)?;
    sepolicy::apply_file(&policy_file)?;
    Ok(())
}

pub fn get_sepolicy(pkg: &str) -> Result<()> {
    let policy_file = profile_path(defs::PROFILE_SELINUX_DIR, pkg)?;
    let policy = std::fs::read_to_string(policy_file)?;
    println!("{policy}");
    Ok(())
}

// ksud doesn't guarteen the correctness of template, it just save
pub fn set_template(id: &str, template: String) -> Result<()> {
    let template_file = profile_path(defs::PROFILE_TEMPLATE_DIR, id)?;
    ensure_dir_exists(defs::PROFILE_TEMPLATE_DIR)?;
    std::fs::write(template_file, template)?;
    Ok(())
}

pub fn get_template(id: &str) -> Result<()> {
    let template_file = profile_path(defs::PROFILE_TEMPLATE_DIR, id)?;
    let template = std::fs::read_to_string(template_file)?;
    println!("{template}");
    Ok(())
}

pub fn delete_template(id: &str) -> Result<()> {
    let template_file = profile_path(defs::PROFILE_TEMPLATE_DIR, id)?;
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
        }
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
            log::info!("profile sepolicy applied: {}", sepolicy.display());
        } else {
            log::info!("profile sepolicy apply failed: {}", sepolicy.display());
        }
    }
    Ok(())
}
