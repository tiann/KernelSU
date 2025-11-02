use anyhow::{Context, Result, bail};
use const_format::concatcp;
use std::collections::HashMap;
use std::fs::File;
use std::io::{Read, Write};
use std::path::Path;

use crate::defs;

const FEATURE_CONFIG_PATH: &str = concatcp!(defs::WORKING_DIR, ".feature_config");
const FEATURE_MAGIC: u32 = 0x7f4b5355;
const FEATURE_VERSION: u32 = 1;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u32)]
pub enum FeatureId {
    SuCompat = 0,
    KernelUmount = 1,
}

impl FeatureId {
    pub fn from_u32(id: u32) -> Option<Self> {
        match id {
            0 => Some(FeatureId::SuCompat),
            1 => Some(FeatureId::KernelUmount),
            _ => None,
        }
    }

    pub fn name(&self) -> &'static str {
        match self {
            FeatureId::SuCompat => "su_compat",
            FeatureId::KernelUmount => "kernel_umount",
        }
    }

    pub fn description(&self) -> &'static str {
        match self {
            FeatureId::SuCompat => {
                "SU Compatibility Mode - allows authorized apps to gain root via traditional 'su' command"
            }
            FeatureId::KernelUmount => {
                "Kernel Umount - controls whether kernel automatically unmounts modules when not needed"
            }
        }
    }
}

fn parse_feature_id(name: &str) -> Result<FeatureId> {
    match name {
        "su_compat" | "0" => Ok(FeatureId::SuCompat),
        "kernel_umount" | "1" => Ok(FeatureId::KernelUmount),
        _ => bail!("Unknown feature: {}", name),
    }
}

pub fn load_binary_config() -> Result<HashMap<u32, u64>> {
    let path = Path::new(FEATURE_CONFIG_PATH);
    if !path.exists() {
        log::info!("Feature config not found, using defaults");
        return Ok(HashMap::new());
    }

    let mut file = File::open(path).with_context(|| "Failed to open feature config")?;

    let mut magic_buf = [0u8; 4];
    file.read_exact(&mut magic_buf)
        .with_context(|| "Failed to read magic")?;
    let magic = u32::from_le_bytes(magic_buf);

    if magic != FEATURE_MAGIC {
        bail!(
            "Invalid feature config magic: expected 0x{:08x}, got 0x{:08x}",
            FEATURE_MAGIC,
            magic
        );
    }

    let mut version_buf = [0u8; 4];
    file.read_exact(&mut version_buf)
        .with_context(|| "Failed to read version")?;
    let version = u32::from_le_bytes(version_buf);

    if version != FEATURE_VERSION {
        log::warn!(
            "Feature config version mismatch: expected {}, got {}",
            FEATURE_VERSION,
            version
        );
    }

    let mut count_buf = [0u8; 4];
    file.read_exact(&mut count_buf)
        .with_context(|| "Failed to read count")?;
    let count = u32::from_le_bytes(count_buf);

    let mut features = HashMap::new();
    for _ in 0..count {
        let mut id_buf = [0u8; 4];
        let mut value_buf = [0u8; 8];

        file.read_exact(&mut id_buf)
            .with_context(|| "Failed to read feature id")?;
        file.read_exact(&mut value_buf)
            .with_context(|| "Failed to read feature value")?;

        let id = u32::from_le_bytes(id_buf);
        let value = u64::from_le_bytes(value_buf);

        features.insert(id, value);
    }

    log::info!("Loaded {} features from config", features.len());
    Ok(features)
}

pub fn save_binary_config(features: &HashMap<u32, u64>) -> Result<()> {
    crate::utils::ensure_dir_exists(Path::new(defs::WORKING_DIR))?;

    let path = Path::new(FEATURE_CONFIG_PATH);
    let mut file = File::create(path).with_context(|| "Failed to create feature config")?;

    file.write_all(&FEATURE_MAGIC.to_le_bytes())
        .with_context(|| "Failed to write magic")?;

    file.write_all(&FEATURE_VERSION.to_le_bytes())
        .with_context(|| "Failed to write version")?;

    let count = features.len() as u32;
    file.write_all(&count.to_le_bytes())
        .with_context(|| "Failed to write count")?;

    for (&id, &value) in features.iter() {
        file.write_all(&id.to_le_bytes())
            .with_context(|| format!("Failed to write feature id {}", id))?;
        file.write_all(&value.to_le_bytes())
            .with_context(|| format!("Failed to write feature value for id {}", id))?;
    }

    file.sync_all()
        .with_context(|| "Failed to sync feature config")?;

    log::info!("Saved {} features to config", features.len());
    Ok(())
}

pub fn apply_config(features: &HashMap<u32, u64>) -> Result<()> {
    log::info!("Applying feature configuration to kernel...");

    let mut applied = 0;
    for (&id, &value) in features.iter() {
        match crate::ksucalls::set_feature(id, value) {
            Ok(_) => {
                if let Some(feature_id) = FeatureId::from_u32(id) {
                    log::info!("Set feature {} to {}", feature_id.name(), value);
                } else {
                    log::info!("Set feature {} to {}", id, value);
                }
                applied += 1;
            }
            Err(e) => {
                log::warn!("Failed to set feature {}: {}", id, e);
            }
        }
    }

    log::info!("Applied {} features successfully", applied);
    Ok(())
}

pub fn get_feature(id: String) -> Result<()> {
    let feature_id = parse_feature_id(&id)?;
    let (value, supported) = crate::ksucalls::get_feature(feature_id as u32)
        .with_context(|| format!("Failed to get feature {}", id))?;

    if !supported {
        println!("Feature '{}' is not supported by kernel", id);
        return Ok(());
    }

    println!("Feature: {} ({})", feature_id.name(), feature_id as u32);
    println!("Description: {}", feature_id.description());
    println!("Value: {}", value);
    println!(
        "Status: {}",
        if value != 0 { "enabled" } else { "disabled" }
    );

    Ok(())
}

pub fn set_feature(id: String, value: u64) -> Result<()> {
    let feature_id = parse_feature_id(&id)?;

    crate::ksucalls::set_feature(feature_id as u32, value)
        .with_context(|| format!("Failed to set feature {} to {}", id, value))?;

    println!(
        "Feature '{}' set to {} ({})",
        feature_id.name(),
        value,
        if value != 0 { "enabled" } else { "disabled" }
    );

    Ok(())
}

pub fn list_features() -> Result<()> {
    println!("Available Features:");
    println!("{}", "=".repeat(80));

    let all_features = [FeatureId::SuCompat, FeatureId::KernelUmount];

    for feature_id in all_features.iter() {
        let id = *feature_id as u32;
        let (value, supported) = crate::ksucalls::get_feature(id).unwrap_or((0, false));

        let status = if !supported {
            "NOT_SUPPORTED".to_string()
        } else if value != 0 {
            format!("ENABLED ({})", value)
        } else {
            "DISABLED".to_string()
        };

        println!("[{}] {} (ID={})", status, feature_id.name(), id);
        println!("    {}", feature_id.description());
        println!();
    }

    Ok(())
}

pub fn load_config_and_apply() -> Result<()> {
    let features = load_binary_config()?;

    if features.is_empty() {
        println!("No features found in config file");
        return Ok(());
    }

    apply_config(&features)?;
    println!("Feature configuration loaded and applied");
    Ok(())
}

pub fn save_config() -> Result<()> {
    let mut features = HashMap::new();

    let all_features = [FeatureId::SuCompat, FeatureId::KernelUmount];

    for feature_id in all_features.iter() {
        let id = *feature_id as u32;
        if let Ok((value, supported)) = crate::ksucalls::get_feature(id)
            && supported
        {
            features.insert(id, value);
            log::info!("Saved feature {} = {}", feature_id.name(), value);
        }
    }

    save_binary_config(&features)?;
    println!(
        "Current feature states saved to config file ({} features)",
        features.len()
    );
    Ok(())
}

pub fn init_features() -> Result<()> {
    log::info!("Initializing features from config...");

    let features = load_binary_config()?;

    if features.is_empty() {
        log::info!("No feature config found, skipping initialization");
        return Ok(());
    }

    apply_config(&features)?;

    Ok(())
}
