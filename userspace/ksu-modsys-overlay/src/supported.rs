use anyhow::Result;
use serde_json::json;

/// Check if this modsys implementation is supported
pub fn check_supported() -> Result<()> {
    let response = json!({
        "code": 0,
        "msg": "Supported"
    });
    
    println!("{}", response);
    Ok(())
}
