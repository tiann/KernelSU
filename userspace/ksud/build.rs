use std::env;
use std::ffi::OsString;
use std::fs;
use std::io;
use std::path::{Path, PathBuf};
use std::process::Command;

const BOOTSTRAP_SOURCE: &str = "src/lkm_image_bootstrap.S";
const BOOTSTRAP_OBJECT: &str = "lkm_image_bootstrap.o";
const PREPARED_BOOTSTRAP_OBJECT: &str = ".lkm_image_bootstrap.o";
const BOOTSTRAP_X86_SOURCE: &str = "src/lkm_image_bootstrap_x86_64.S";
const BOOTSTRAP_X86_OBJECT: &str = "lkm_image_bootstrap_x86_64.o";
const PREPARED_BOOTSTRAP_X86_OBJECT: &str = ".lkm_image_bootstrap_x86_64.o";

const EM_AARCH64: u16 = 183;
const EM_X86_64: u16 = 62;

fn get_git_version() -> Result<(u32, String), std::io::Error> {
    let output = Command::new("git")
        .args(["rev-list", "--count", "HEAD"])
        .output()?;

    let output = output.stdout;
    let version_code = String::from_utf8(output).expect("Failed to read git count stdout");
    let version_code: u32 = version_code
        .trim()
        .parse()
        .map_err(|_| std::io::Error::other("Failed to parse git count"))?;
    let version_code = 30000 + version_code;

    let version_name = String::from_utf8(
        Command::new("git")
            .args(["describe", "--tags", "--always"])
            .output()?
            .stdout,
    )
    .map_err(|_| std::io::Error::other("Failed to read git describe stdout"))?;
    let version_name = version_name.trim_start_matches('v').to_string();
    Ok((version_code, version_name))
}

fn configure_bindgen() {
    // The bindgen::Builder is the main entry point
    // to bindgen, and lets you build up options for
    // the resulting bindings.
    let bindings = bindgen::Builder::default()
        // The input header we would like to generate
        // bindings for.
        .header("src/ksu_uapi.h")
        .clang_args(["-x", "c++", "-I../../"])
        // Tell cargo to invalidate the built crate whenever any of the
        // included header files changed.
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        // Finish the builder and generate the bindings.
        .generate()
        // Unwrap the Result and panic on failure.
        .expect("Unable to generate bindings");

    // Write the bindings to the $OUT_DIR/bindings.rs file.
    let out_path = std::path::PathBuf::from(env::var("OUT_DIR").unwrap());
    // for debug, uncomment below
    // let out_path = std::path::PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}

fn validate_bootstrap_object(path: &Path, machine: u16) -> io::Result<()> {
    let object = fs::read(path)?;
    let valid = object.len() >= 64
        && object.starts_with(b"\x7fELF")
        && object[4] == 2
        && object[5] == 1
        && u16::from_le_bytes([object[16], object[17]]) == 1
        && u16::from_le_bytes([object[18], object[19]]) == machine;
    if valid {
        Ok(())
    } else {
        Err(io::Error::other(format!(
            "bootstrap object must be a little-endian {} ELF64 ET_REL",
            if machine == EM_X86_64 {
                "x86-64"
            } else {
                "AArch64"
            }
        )))
    }
}

fn copy_bootstrap_object(source: &Path, output: &Path, machine: u16) -> io::Result<()> {
    validate_bootstrap_object(source, machine)?;
    fs::copy(source, output)?;
    Ok(())
}

fn ndk_clang() -> Option<PathBuf> {
    let ndk = env::var_os("ANDROID_NDK_HOME")
        .or_else(|| env::var_os("ANDROID_NDK_ROOT"))
        .map(PathBuf::from)?;
    let prebuilt = ndk.join("toolchains/llvm/prebuilt");
    let mut hosts = fs::read_dir(prebuilt)
        .ok()?
        .filter_map(Result::ok)
        .map(|entry| entry.path())
        .collect::<Vec<_>>();
    hosts.sort();
    hosts.into_iter().find_map(|host| {
        ["clang", "clang.exe"]
            .into_iter()
            .map(|name| host.join("bin").join(name))
            .find(|path| path.is_file())
    })
}

fn run_assembler(program: &Path, arguments: &[OsString]) -> Result<(), String> {
    let output = Command::new(program)
        .args(arguments)
        .output()
        .map_err(|error| format!("{}: {error}", program.display()))?;
    if output.status.success() {
        return Ok(());
    }
    let details = if output.stderr.is_empty() {
        &output.stdout
    } else {
        &output.stderr
    };
    Err(format!(
        "{}: {}",
        program.display(),
        String::from_utf8_lossy(details).trim()
    ))
}

struct BootstrapSpec {
    source: &'static str,
    object: &'static str,
    prepared: &'static str,
    env_override: &'static str,
    machine: u16,
    /// Target triple passed to clang/llvm-mc, plus the fallback driver.
    triple: &'static str,
    /// GNU cross driver tried before clang.
    gnu_driver: &'static str,
    /// Human readable architecture name used in diagnostics.
    label: &'static str,
}

const BOOTSTRAP_SPECS: [BootstrapSpec; 2] = [
    BootstrapSpec {
        source: BOOTSTRAP_SOURCE,
        object: BOOTSTRAP_OBJECT,
        prepared: PREPARED_BOOTSTRAP_OBJECT,
        env_override: "KSU_LKM_BOOTSTRAP_OBJECT",
        machine: EM_AARCH64,
        triple: "aarch64-linux-gnu",
        gnu_driver: "aarch64-linux-gnu-gcc",
        label: "AArch64",
    },
    BootstrapSpec {
        source: BOOTSTRAP_X86_SOURCE,
        object: BOOTSTRAP_X86_OBJECT,
        prepared: PREPARED_BOOTSTRAP_X86_OBJECT,
        env_override: "KSU_LKM_BOOTSTRAP_X86_OBJECT",
        machine: EM_X86_64,
        triple: "x86_64-linux-gnu",
        gnu_driver: "x86_64-linux-gnu-gcc",
        label: "x86-64",
    },
];

fn assemble_bootstrap(spec: &BootstrapSpec) {
    println!("cargo:rerun-if-changed={}", spec.source);
    println!("cargo:rerun-if-env-changed={}", spec.env_override);
    println!("cargo:rerun-if-env-changed=KSU_LKM_BOOTSTRAP_CC");
    println!("cargo:rerun-if-env-changed=ANDROID_NDK_HOME");
    println!("cargo:rerun-if-env-changed=ANDROID_NDK_ROOT");

    let manifest = PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").unwrap());
    let source = manifest.join(spec.source);
    let output = PathBuf::from(env::var_os("OUT_DIR").unwrap()).join(spec.object);

    if let Some(prebuilt) = env::var_os(spec.env_override) {
        let prebuilt = PathBuf::from(prebuilt);
        copy_bootstrap_object(&prebuilt, &output, spec.machine).unwrap_or_else(|error| {
            panic!(
                "cannot use {} {}: {error}",
                spec.env_override,
                prebuilt.display()
            )
        });
        return;
    }

    // cross builds prepare this file on the host before entering the container.
    let prepared = manifest.join(spec.prepared);
    println!("cargo:rerun-if-changed={}", prepared.display());
    if prepared.is_file() {
        copy_bootstrap_object(&prepared, &output, spec.machine).unwrap_or_else(|error| {
            panic!(
                "cannot use prepared bootstrap object {}: {error}",
                prepared.display()
            )
        });
        return;
    }

    let mut errors = Vec::new();
    let mut drivers = Vec::<PathBuf>::new();
    if let Some(compiler) = env::var_os("KSU_LKM_BOOTSTRAP_CC") {
        drivers.push(PathBuf::from(compiler));
    }
    drivers.push(PathBuf::from(spec.gnu_driver));
    if let Some(clang) = ndk_clang() {
        drivers.push(clang);
    }
    drivers.push(PathBuf::from("clang"));

    for driver in drivers {
        let mut arguments = Vec::<OsString>::new();
        if driver
            .file_name()
            .and_then(|name| name.to_str())
            .is_some_and(|name| name.contains("clang"))
        {
            arguments.push(format!("--target={}", spec.triple).into());
        }
        arguments.extend([
            "-c".into(),
            "-nostdlib".into(),
            "-o".into(),
            output.as_os_str().to_owned(),
            source.as_os_str().to_owned(),
        ]);
        match run_assembler(&driver, &arguments) {
            Ok(()) => {
                validate_bootstrap_object(&output, spec.machine)
                    .expect("assembler produced an invalid bootstrap object");
                return;
            }
            Err(error) => errors.push(error),
        }
    }

    let llvm_mc = PathBuf::from("llvm-mc");
    let llvm_arguments = [
        format!("-triple={}", spec.triple).into(),
        "-filetype=obj".into(),
        "-o".into(),
        output.as_os_str().to_owned(),
        source.as_os_str().to_owned(),
    ];
    match run_assembler(&llvm_mc, &llvm_arguments) {
        Ok(()) => {
            validate_bootstrap_object(&output, spec.machine)
                .expect("llvm-mc produced an invalid bootstrap object");
        }
        Err(error) => {
            errors.push(error);
            panic!(
                "cannot assemble the {} LKM bootstrap; install a {} GNU compiler, clang, or llvm-mc, or set {}:\n{}",
                spec.label,
                spec.label,
                spec.env_override,
                errors.join("\n")
            );
        }
    }
}

fn main() {
    for spec in &BOOTSTRAP_SPECS {
        assemble_bootstrap(spec);
    }

    let (code, name) = match get_git_version() {
        Ok((code, name)) => (code, name),
        Err(_) => {
            // show warning if git is not installed
            println!("cargo:warning=Failed to get git version, using 0.0.0");
            (0, "0.0.0".to_string())
        }
    };
    if env::var("KSU_PACKAGE_NAME").is_err() {
        println!("cargo:rustc-env=KSU_PACKAGE_NAME=me.weishu.kernelsu");
    }
    println!("cargo:rustc-env=VERSION_CODE={code}");
    println!("cargo:rustc-env=VERSION_NAME={name}");

    let target_os = env::var("CARGO_CFG_TARGET_OS").expect("CARGO_CFG_TARGET_OS not set");
    if target_os == "android" {
        configure_bindgen();
    }
}
