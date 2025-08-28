// Build script for ksu-modsys-overlay
use std::env;
use std::fs;
use std::path::Path;

fn main() {
    // Set version info from environment or use defaults
    let out_dir = env::var("OUT_DIR").unwrap();
    
    let version_code = env::var("VERSION_CODE")
        .unwrap_or_else(|_| "10940".to_string());
    let version_name = env::var("VERSION_NAME")
        .unwrap_or_else(|_| "v0.9.5".to_string());
    
    // Write version files
    fs::write(
        Path::new(&out_dir).join("VERSION_CODE"),
        version_code,
    ).unwrap();
    
    fs::write(
        Path::new(&out_dir).join("VERSION_NAME"),
        version_name,
    ).unwrap();
    
    // Tell cargo to rerun if version info changes
    println!("cargo:rerun-if-env-changed=VERSION_CODE");
    println!("cargo:rerun-if-env-changed=VERSION_NAME");
}
