use anyhow::{Result, Ok};
use std::{ffi::c_int, process::Command};

extern {
    fn get_signature(input: *const u8,
        out_size: *mut u32,
        out_hash: *mut u32) -> c_int;

    fn set_kernel_param(size: u32, hash: u32) -> c_int;
}

fn get_apk_signature(apk: &str) -> (u32, u32){
    let mut out_size: u32 = 0;
    let mut out_hash: u32 = 0;
    unsafe{
        get_signature(apk.as_ptr(), &mut out_size, &mut out_hash);
    }
    (out_size, out_hash)
}

fn get_apk_path(package_name: &str) -> String {
    let mut cmd = "pm path ".to_string();
    cmd += package_name;
    cmd += &" | sed 's/package://g'".to_string();
    let output = Command::new("sh")
        .arg("-c")
        .arg(cmd)
        .output()
        .unwrap();
    let out = String::from_utf8(output.stdout).unwrap();
    return out;
}

pub fn set_manager(apk: &str) -> Result<()> {
    if apk.ends_with(".apk") {
        let sign = get_apk_signature(apk);
        unsafe{
            set_kernel_param(sign.0, sign.1);
        }
    } else {
        let path = get_apk_path(apk);
        let path = String::from(path.trim());
        println!("path: {}", path);
        if path.ends_with(".apk") {
            let sign = get_apk_signature(path.as_str());
            unsafe{
                set_kernel_param(sign.0, sign.1);
            }
        }else{
            println!("Invalid argument!");
        }
    }
    Ok(())
}