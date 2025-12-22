use anyhow::{Context, Result, bail};
use log::{debug, warn};
use std::collections::HashMap;
use std::fs::{self, File};
use std::io::{Read, Write};
use std::path::{Path, PathBuf};

use crate::defs;
use crate::utils::ensure_dir_exists;

#[allow(clippy::unreadable_literal)]
const MODULE_CONFIG_MAGIC: u32 = 0x4b53554d; // "KSUM"
const MODULE_CONFIG_VERSION: u32 = 1;

// Validation limits
pub const MAX_CONFIG_KEY_LEN: usize = 256;
pub const MAX_CONFIG_VALUE_LEN: usize = 1024 * 1024; // 1MB
pub const MAX_CONFIG_COUNT: usize = 32;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ConfigType {
    Persist,
    Temp,
}

impl ConfigType {
    const fn filename(self) -> &'static str {
        match self {
            Self::Persist => defs::PERSIST_CONFIG_NAME,
            Self::Temp => defs::TEMP_CONFIG_NAME,
        }
    }
}

/// Parse a boolean config value
/// Accepts "true", "1" (case-insensitive) as true, everything else as false
pub fn parse_bool_config(value: &str) -> bool {
    let trimmed = value.trim();
    trimmed.eq_ignore_ascii_case("true") || trimmed == "1"
}

/// Validate config key
/// Uses the same validation rules as module_id: ^[a-zA-Z][a-zA-Z0-9._-]+$
/// - Must start with a letter (a-zA-Z)
/// - Followed by one or more alphanumeric, dot, underscore, or hyphen characters
/// - Minimum length: 2 characters
pub fn validate_config_key(key: &str) -> Result<()> {
    if key.is_empty() {
        bail!("Config key cannot be empty");
    }

    if key.len() > MAX_CONFIG_KEY_LEN {
        bail!(
            "Config key too long: {} bytes (max: {})",
            key.len(),
            MAX_CONFIG_KEY_LEN
        );
    }

    // Use same pattern as module_id for consistency
    let re = regex_lite::Regex::new(r"^[a-zA-Z][a-zA-Z0-9._-]+$")?;
    if !re.is_match(key) {
        bail!("Invalid config key: '{key}'. Must match /^[a-zA-Z][a-zA-Z0-9._-]+$/");
    }

    Ok(())
}

/// Validate config value
/// Only enforces maximum length - no character restrictions
/// Values are stored in binary format with length prefix, so any UTF-8 data is safe
pub fn validate_config_value(value: &str) -> Result<()> {
    if value.len() > MAX_CONFIG_VALUE_LEN {
        bail!(
            "Config value too long: {} bytes (max: {})",
            value.len(),
            MAX_CONFIG_VALUE_LEN
        );
    }

    // No character restrictions - binary storage format handles all UTF-8 safely
    Ok(())
}

/// Validate config count
fn validate_config_count(config: &HashMap<String, String>) -> Result<()> {
    if config.len() > MAX_CONFIG_COUNT {
        bail!(
            "Too many config entries: {} (max: {})",
            config.len(),
            MAX_CONFIG_COUNT
        );
    }
    Ok(())
}

/// Get the config directory path for a module
fn get_config_dir(module_id: &str) -> PathBuf {
    Path::new(defs::MODULE_CONFIG_DIR).join(module_id)
}

/// Get the config file path for a module
fn get_config_path(module_id: &str, config_type: ConfigType) -> PathBuf {
    get_config_dir(module_id).join(config_type.filename())
}

/// Ensure the config directory exists
fn ensure_config_dir(module_id: &str) -> Result<PathBuf> {
    let dir = get_config_dir(module_id);
    ensure_dir_exists(&dir)?;
    Ok(dir)
}

/// Load config from binary file
pub fn load_config(module_id: &str, config_type: ConfigType) -> Result<HashMap<String, String>> {
    crate::module::validate_module_id(module_id)?;

    let config_path = get_config_path(module_id, config_type);

    if !config_path.exists() {
        debug!("Config file not found: {}", config_path.display());
        return Ok(HashMap::new());
    }

    let mut file = File::open(&config_path)
        .with_context(|| format!("Failed to open config file: {}", config_path.display()))?;

    // Read magic
    let mut magic_buf = [0u8; 4];
    file.read_exact(&mut magic_buf)
        .with_context(|| "Failed to read magic")?;
    let magic = u32::from_le_bytes(magic_buf);

    if magic != MODULE_CONFIG_MAGIC {
        bail!("Invalid config magic: expected 0x{MODULE_CONFIG_MAGIC:08x}, got 0x{magic:08x}");
    }

    // Read version
    let mut version_buf = [0u8; 4];
    file.read_exact(&mut version_buf)
        .with_context(|| "Failed to read version")?;
    let version = u32::from_le_bytes(version_buf);

    if version != MODULE_CONFIG_VERSION {
        bail!("Unsupported config version: expected {MODULE_CONFIG_VERSION}, got {version}");
    }

    // Read count
    let mut count_buf = [0u8; 4];
    file.read_exact(&mut count_buf)
        .with_context(|| "Failed to read count")?;
    let count = u32::from_le_bytes(count_buf);

    // Read entries
    let mut config = HashMap::new();
    for i in 0..count {
        // Read key length
        let mut key_len_buf = [0u8; 4];
        file.read_exact(&mut key_len_buf)
            .with_context(|| format!("Failed to read key length for entry {i}"))?;
        let key_len = u32::from_le_bytes(key_len_buf) as usize;

        // Read key data
        let mut key_buf = vec![0u8; key_len];
        file.read_exact(&mut key_buf)
            .with_context(|| format!("Failed to read key data for entry {i}"))?;
        let key = String::from_utf8(key_buf)
            .with_context(|| format!("Invalid UTF-8 in key for entry {i}"))?;

        // Read value length
        let mut value_len_buf = [0u8; 4];
        file.read_exact(&mut value_len_buf)
            .with_context(|| format!("Failed to read value length for entry {i}"))?;
        let value_len = u32::from_le_bytes(value_len_buf) as usize;

        // Read value data
        let mut value_buf = vec![0u8; value_len];
        file.read_exact(&mut value_buf)
            .with_context(|| format!("Failed to read value data for entry {i}"))?;
        let value = String::from_utf8(value_buf)
            .with_context(|| format!("Invalid UTF-8 in value for entry {i}"))?;

        config.insert(key, value);
    }

    debug!(
        "Loaded {} entries from {}",
        config.len(),
        config_path.display()
    );
    Ok(config)
}

/// Save config to binary file
pub fn save_config(
    module_id: &str,
    config_type: ConfigType,
    config: &HashMap<String, String>,
) -> Result<()> {
    crate::module::validate_module_id(module_id)?;

    // Validate config count
    validate_config_count(config)?;

    // Validate all keys and values
    for (key, value) in config {
        validate_config_key(key).with_context(|| format!("Invalid config key: '{key}'"))?;
        validate_config_value(value)
            .with_context(|| format!("Invalid config value for key '{key}'"))?;
    }

    ensure_config_dir(module_id)?;

    let config_path = get_config_path(module_id, config_type);
    let temp_path = config_path.with_extension("tmp");

    // Write to temporary file first
    let mut file = File::create(&temp_path)
        .with_context(|| format!("Failed to create temp config file: {}", temp_path.display()))?;

    // Write magic
    file.write_all(&MODULE_CONFIG_MAGIC.to_le_bytes())
        .with_context(|| "Failed to write magic")?;

    // Write version
    file.write_all(&MODULE_CONFIG_VERSION.to_le_bytes())
        .with_context(|| "Failed to write version")?;

    // Write count
    let count = config.len() as u32;
    file.write_all(&count.to_le_bytes())
        .with_context(|| "Failed to write count")?;

    // Write entries
    for (key, value) in config {
        // Write key length
        let key_bytes = key.as_bytes();
        let key_len = key_bytes.len() as u32;
        file.write_all(&key_len.to_le_bytes())
            .with_context(|| format!("Failed to write key length for '{key}'"))?;

        // Write key data
        file.write_all(key_bytes)
            .with_context(|| format!("Failed to write key data for '{key}'"))?;

        // Write value length
        let value_bytes = value.as_bytes();
        let value_len = value_bytes.len() as u32;
        file.write_all(&value_len.to_le_bytes())
            .with_context(|| format!("Failed to write value length for '{key}'"))?;

        // Write value data
        file.write_all(value_bytes)
            .with_context(|| format!("Failed to write value data for '{key}'"))?;
    }

    file.sync_all()
        .with_context(|| "Failed to sync config file")?;

    // Atomic rename
    fs::rename(&temp_path, &config_path).with_context(|| {
        format!(
            "Failed to rename config file: {} -> {}",
            temp_path.display(),
            config_path.display()
        )
    })?;

    debug!(
        "Saved {} entries to {}",
        config.len(),
        config_path.display()
    );
    Ok(())
}

/// Get a single config value
#[allow(dead_code)]
pub fn get_config_value(
    module_id: &str,
    key: &str,
    config_type: ConfigType,
) -> Result<Option<String>> {
    let config = load_config(module_id, config_type)?;
    Ok(config.get(key).cloned())
}

/// Set a single config value
pub fn set_config_value(
    module_id: &str,
    key: &str,
    value: &str,
    config_type: ConfigType,
) -> Result<()> {
    // Validate input early for better error messages
    validate_config_key(key)?;
    validate_config_value(value)?;

    let mut config = load_config(module_id, config_type)?;
    config.insert(key.to_string(), value.to_string());

    // Note: save_config will also validate, but this provides earlier feedback
    save_config(module_id, config_type, &config)?;
    Ok(())
}

/// Delete a single config value
pub fn delete_config_value(module_id: &str, key: &str, config_type: ConfigType) -> Result<()> {
    let mut config = load_config(module_id, config_type)?;

    if config.remove(key).is_none() {
        bail!("Key '{key}' not found in config");
    }

    save_config(module_id, config_type, &config)?;
    Ok(())
}

/// Clear all config values
pub fn clear_config(module_id: &str, config_type: ConfigType) -> Result<()> {
    let config_path = get_config_path(module_id, config_type);

    if config_path.exists() {
        fs::remove_file(&config_path)
            .with_context(|| format!("Failed to remove config file: {}", config_path.display()))?;
        debug!("Cleared config: {}", config_path.display());
    }

    Ok(())
}

/// Merge persist and temp configs (temp takes priority)
pub fn merge_configs(module_id: &str) -> Result<HashMap<String, String>> {
    crate::module::validate_module_id(module_id)?;

    let mut merged = match load_config(module_id, ConfigType::Persist) {
        Ok(config) => config,
        Err(e) => {
            warn!("Failed to load persist config for module '{module_id}': {e}");
            HashMap::new()
        }
    };

    let temp = match load_config(module_id, ConfigType::Temp) {
        Ok(config) => config,
        Err(e) => {
            warn!("Failed to load temp config for module '{module_id}': {e}");
            HashMap::new()
        }
    };

    // Temp config overrides persist config
    for (key, value) in temp {
        merged.insert(key, value);
    }

    Ok(merged)
}

/// Get all module configs (for iteration)
/// Loads all configs in a single pass to minimize I/O overhead
pub fn get_all_module_configs() -> Result<HashMap<String, HashMap<String, String>>> {
    let config_root = Path::new(defs::MODULE_CONFIG_DIR);

    if !config_root.exists() {
        return Ok(HashMap::new());
    }

    let mut all_configs = HashMap::new();

    for entry in fs::read_dir(config_root)
        .with_context(|| format!("Failed to read config directory: {}", config_root.display()))?
    {
        let entry = entry?;
        let path = entry.path();

        if !path.is_dir() {
            continue;
        }

        if let Some(module_id) = path.file_name().and_then(|n| n.to_str()) {
            match merge_configs(module_id) {
                Ok(config) => {
                    if !config.is_empty() {
                        all_configs.insert(module_id.to_string(), config);
                    }
                }
                Err(e) => {
                    warn!("Failed to load config for module '{module_id}': {e}");
                    // Continue processing other modules
                }
            }
        }
    }

    Ok(all_configs)
}

/// Clear all temporary configs (called during post-fs-data)
pub fn clear_all_temp_configs() -> Result<()> {
    let config_root = Path::new(defs::MODULE_CONFIG_DIR);

    if !config_root.exists() {
        debug!("Config directory does not exist, nothing to clear");
        return Ok(());
    }

    let mut cleared_count = 0;

    for entry in fs::read_dir(config_root)
        .with_context(|| format!("Failed to read config directory: {}", config_root.display()))?
    {
        let entry = entry?;
        let path = entry.path();

        if !path.is_dir() {
            continue;
        }

        let temp_config = path.join(defs::TEMP_CONFIG_NAME);
        if temp_config.exists() {
            match fs::remove_file(&temp_config) {
                Ok(()) => {
                    debug!("Cleared temp config: {}", temp_config.display());
                    cleared_count += 1;
                }
                Err(e) => {
                    warn!("Failed to clear temp config {}: {e}", temp_config.display());
                }
            }
        }
    }

    if cleared_count > 0 {
        debug!("Cleared {cleared_count} temp config file(s)");
    }

    Ok(())
}

/// Clear all configs for a module (called during uninstall)
pub fn clear_module_configs(module_id: &str) -> Result<()> {
    crate::module::validate_module_id(module_id)?;

    let config_dir = get_config_dir(module_id);

    if config_dir.exists() {
        fs::remove_dir_all(&config_dir).with_context(|| {
            format!(
                "Failed to remove config directory: {}",
                config_dir.display()
            )
        })?;
        debug!("Cleared all configs for module: {module_id}");
    }

    Ok(())
}
