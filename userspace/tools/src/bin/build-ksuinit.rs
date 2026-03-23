use std::env;
use std::path::{Path, PathBuf};
use std::process::{Command, ExitCode};

const TARGET: &str = "aarch64-unknown-linux-musl";

fn host_tag() -> &'static str {
    match env::consts::OS {
        "windows" => "windows",
        "linux" => "linux",
        "macos" => "darwin",
        other => {
            eprintln!("Unsupported host OS: {other}");
            std::process::exit(2);
        }
    }
}

fn find_linker(ndk_home: &Path) -> PathBuf {
    let base = ndk_home
        .join("toolchains")
        .join("llvm")
        .join("prebuilt")
        .join(format!("{}-x86_64", host_tag()))
        .join("bin");

    let linker = if cfg!(windows) {
        let cmd = base.join("aarch64-linux-android26-clang.cmd");
        if cmd.exists() {
            cmd
        } else {
            base.join("aarch64-linux-android26-clang")
        }
    } else {
        base.join("aarch64-linux-android26-clang")
    };

    if !linker.exists() {
        eprintln!("Linker not found: {}", linker.display());
        std::process::exit(2);
    }

    linker
}

fn main() -> ExitCode {
    let ndk_home = match env::var("ANDROID_NDK_HOME") {
        Ok(v) if !v.trim().is_empty() => v,
        _ => {
            eprintln!("ANDROID_NDK_HOME is required");
            return ExitCode::from(2);
        }
    };

    let linker = find_linker(Path::new(&ndk_home));
    let rustflags = match env::var("RUSTFLAGS") {
        Ok(existing) if !existing.trim().is_empty() => format!("{existing} -C link-arg=-no-pie"),
        _ => "-C link-arg=-no-pie".to_string(),
    };

    let mut cmd = Command::new("cargo");
    cmd.arg("build")
        .arg("--target")
        .arg(TARGET)
        .arg("--release")
        .current_dir(Path::new(env!("CARGO_MANIFEST_DIR")).join("../ksuinit"))
        .env(
            "CARGO_TARGET_AARCH64_UNKNOWN_LINUX_MUSL_LINKER",
            linker.as_os_str(),
        )
        .env("RUSTFLAGS", rustflags);

    // Allow users to append extra build flags after `--`.
    for arg in env::args().skip(1) {
        cmd.arg(arg);
    }

    let status = match cmd.status() {
        Ok(s) => s,
        Err(e) => {
            eprintln!("Failed to execute cargo build: {e}");
            return ExitCode::from(1);
        }
    };

    if status.success() {
        ExitCode::SUCCESS
    } else {
        ExitCode::from(1)
    }
}
