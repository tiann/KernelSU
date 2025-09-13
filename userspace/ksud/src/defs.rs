pub use ksu_core::defs::*;

// Version strings are defined/generated only in ksud; avoid putting them in ksu-core
pub const VERSION_CODE: &str = include_str!(concat!(env!("OUT_DIR"), "/VERSION_CODE"));
pub const VERSION_NAME: &str = include_str!(concat!(env!("OUT_DIR"), "/VERSION_NAME"));
