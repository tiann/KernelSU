pub use ksu_core::defs::*;

// 版本号仅在 ksud 定义与生成，避免放入 ksu-core
pub const VERSION_CODE: &str = include_str!(concat!(env!("OUT_DIR"), "/VERSION_CODE"));
pub const VERSION_NAME: &str = include_str!(concat!(env!("OUT_DIR"), "/VERSION_NAME"));
