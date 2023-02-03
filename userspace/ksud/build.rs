use std::env;
use std::fs::File;
use std::io::Write;
use std::path::Path;
use std::process::Command;

fn get_git_version() -> (u32, String) {
    let version_code = String::from_utf8(
        Command::new("git")
            .args(["rev-list", "--count", "HEAD"])
            .output()
            .expect("Failed to get git count")
            .stdout,
    )
    .expect("Failed to read git count stdout");
    let version_code: u32 = version_code.trim().parse().expect("Failed to parse git count");
    let version_code = 10000 + 200 + version_code; // For historical reasons

    let version_name = String::from_utf8(
        Command::new("git")
            .args(["describe", "--tags", "--always"])
            .output()
            .expect("Failed to get git version")
            .stdout,
    )
    .expect("Failed to read git version stdout");
    (version_code, version_name)
}

fn main() {
    let (code, name)= get_git_version();
    let out_dir = env::var("OUT_DIR").expect("Failed to get $OUT_DIR");
    let out_dir = Path::new(&out_dir);
    File::create(Path::new(out_dir).join("VERSION_CODE"))
        .expect("Failed to create VERSION_CODE")
        .write_all(code.to_string().as_bytes())
        .expect("Failed to write VERSION_CODE");

    File::create(Path::new(out_dir).join("VERSION_NAME"))
        .expect("Failed to create VERSION_NAME")
        .write_all(name.trim().as_bytes())
        .expect("Failed to write VERSION_NAME");
}
