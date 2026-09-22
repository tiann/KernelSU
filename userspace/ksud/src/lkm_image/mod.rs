// SPDX-License-Identifier: GPL-2.0-only
//!
//! LKM-in-kernel-image injection (`ksud boot-patch-v2`).
//!
//! The module is split by concern: byte/ELF/symbol helpers, the capsule
//! container, the `bootstrap.o` linker and one file per supported kernel
//! architecture, with the CLI entry point and the shared report types here.

use std::fs;
use std::io::{Cursor, Write};
use std::path::{Path, PathBuf};

use android_bootimg::parser::BootImage;
use android_bootimg::patcher::BootImagePatchOption;

use anyhow::{Context, Result, anyhow, ensure};

use restore::ReplacedInjection;

use crate::{assets, boot_patch};

mod arm64;
mod bootstrap;
mod btf;
mod bytes;
mod capsule;
mod elf;
mod image;
mod modinfo;
mod restore;
mod symbols;
mod x86_64;
mod x86_64_layout;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Arch {
    Aarch64,
    X86_64,
}

impl Arch {
    pub const fn label(self) -> &'static str {
        match self {
            Self::Aarch64 => "AArch64",
            Self::X86_64 => "x86-64",
        }
    }

    /// The kernel image container this architecture uses.
    pub const fn container_label(self) -> &'static str {
        match self {
            Self::Aarch64 => "Image",
            Self::X86_64 => "bzImage",
        }
    }

    /// The release asset directory that holds this architecture's modules.
    ///
    /// Only the host build needs it: an Android build embeds the assets of the
    /// running architecture flat, without the release directory.
    #[cfg(not(target_os = "android"))]
    const fn module_directory(self) -> &'static str {
        match self {
            Self::Aarch64 => "aarch64",
            Self::X86_64 => "x86_64",
        }
    }
}

// CLI and top-level result types.

// The flags mirror the classic `boot-patch` switches one to one; a bit set of
// switches would be less readable on the command line.
#[allow(clippy::struct_excessive_bools)]
#[derive(clap::Args, Debug)]
pub struct BootPatchV2Args {
    /// Source boot image, or a raw kernel image (Image/bzImage)
    #[arg(short, long)]
    pub boot: PathBuf,

    /// Exact kernelsu.ko; auto-select the bundled KMI build when omitted
    #[arg(short, long)]
    pub module: Option<PathBuf>,

    /// Patched boot image output
    #[arg(short, long)]
    pub output: PathBuf,

    /// Replace an existing output file
    #[arg(long, default_value = "false")]
    pub force: bool,

    /// `load_module()` arguments for the injected module, e.g. "bundled=1"
    ///
    /// This is the only source of module arguments for `boot-patch-v2`: they are
    /// stored in the capsule and handed to `load_module()` by the bootstrap.
    /// The ramdisk's `/ksu_config` is deliberately ignored -- the stub loads the
    /// module before `ksuinit` runs, so `ksuinit` skips it and those settings
    /// would never reach the module.
    ///
    /// Conflicts with the named flags below: pass either the raw string or the
    /// flags, never both.
    #[arg(
        long,
        conflicts_with_all = ["allow_shell", "no_custom_rc", "bundled"]
    )]
    pub module_args: Option<String>,

    /// Always allow shell to get root permission (`allow_shell=1`)
    #[arg(long, default_value = "false")]
    pub allow_shell: bool,

    /// Do not load a custom rc file (`norc=1`)
    #[arg(long, default_value = "false")]
    pub no_custom_rc: bool,

    /// Mark the module as the bundled one (`bundled=1`)
    #[arg(long, default_value = "false")]
    pub bundled: bool,

    /// Rewrite the injected module's `vermagic` to match this kernel
    ///
    /// Most builds do not need this: Android/GKI kernels check the KMI CRCs
    /// rather than the release string, so a module built for the same KMI loads
    /// even when the kernel versions differ.  Use it when a normal boot ends in
    /// `version magic '...' should be '...'` -- it performs the same rewrite
    /// `ksuinit` does after such a failure, so the module loads on kernels that
    /// insist on an exact match.
    #[arg(long, default_value = "false")]
    pub override_vermagic: bool,
}

impl BootPatchV2Args {
    /// The arguments the bootstrap hands to `load_module()`.
    ///
    /// Either the raw `--module-args` string, or the ones the named flags spell
    /// out -- the same `module_param` names the classic `boot-patch` writes into
    /// the ramdisk's `/ksu_config`.  clap guarantees the two cannot be combined.
    pub fn load_args(&self) -> String {
        if let Some(args) = &self.module_args {
            return args.clone();
        }
        let mut args = Vec::new();
        if self.bundled {
            args.push("bundled=1");
        }
        if self.no_custom_rc {
            args.push("norc=1");
        }
        if self.allow_shell {
            args.push("allow_shell=1");
        }
        args.join(" ")
    }
}

#[derive(Clone, Copy, Debug)]
pub struct CallSite {
    pub file_offset: usize,
    pub address: u64,
    pub target: u64,
}

#[derive(clap::Args, Debug)]
pub struct BootRestoreV2Args {
    /// Patched boot image, or the patched raw kernel image
    #[arg(short, long)]
    pub boot: PathBuf,

    /// Restored boot image; defaults to kernelsu_restore_<timestamp>.img
    #[arg(short, long)]
    pub output: Option<PathBuf>,

    /// Replace an existing output file
    #[arg(long, default_value = "false")]
    pub force: bool,

    /// Only check the capsule, the record and the restored SHA-256
    #[arg(long, default_value = "false")]
    pub verify_only: bool,

    /// Flash the restored image to the boot partition (Android only)
    #[cfg(target_os = "android")]
    #[arg(short, long, default_value = "false")]
    pub flash: bool,
}

#[derive(Clone, Copy, Debug)]
pub struct BtfReport {
    pub file_offset: usize,
    pub size: usize,
    pub type_count: usize,
}

/// The parts of the injection report that only one architecture can produce.
#[derive(Debug)]
pub enum InjectionDetails {
    /// An uncompressed `Image`: the capsule simply grows the file.
    Arm64 {
        /// The GKI `load_info` ABI the capsule reserves storage for.
        load_info_structure_size: Option<u64>,
        load_info_storage_size: u64,
        load_info_hdr_offset: u64,
        load_info_len_offset: u64,
        memblock_reserve_call: usize,
        page_offset: u64,
        image_size: usize,
    },
    /// A `bzImage`: the payload has to be recompressed and the stub rebuilt.
    X86_64 {
        capsule_offset: usize,
        capsule_size: usize,
        reservation_extension: usize,
        payload_before: usize,
        payload_after: usize,
        payload_format: String,
        content_before: usize,
        content_after: usize,
        syssize: usize,
    },
}

/// What `boot-patch-v2` did to the kernel, in the order the CLI prints it.
#[derive(Debug)]
pub struct ImageInjectionReport {
    pub arch: Arch,
    pub kernel_release: String,
    pub btf: Option<BtfReport>,
    /// Set when the input was already patched and had to be restored first.
    pub replaced: Option<ReplacedInjection>,
    pub kallsyms_layout: &'static str,
    pub kallsyms_count: usize,
    pub bootstrap_offset: usize,
    pub bootstrap_size: usize,
    pub fixup_count: usize,
    pub unresolved: Vec<String>,
    /// Set when `--override-vermagic` rewrote the module's version magic.
    pub vermagic: Option<VermagicRewrite>,
    pub details: InjectionDetails,
}

/// The module's version magic before and after an override.
#[derive(Clone, Debug)]
pub struct VermagicRewrite {
    pub previous: String,
    pub current: String,
}

impl ImageInjectionReport {
    /// Print the report the way `boot-patch-v2` does.
    pub fn print(&self) {
        print!("{self}");
    }
}

impl std::fmt::Display for ImageInjectionReport {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        writeln!(
            f,
            "- Architecture: {} ({})",
            self.arch.label(),
            self.arch.container_label()
        )?;
        if let Some(replaced) = self.replaced {
            writeln!(
                f,
                "- Previous injection: undone ({} instructions, capsule 0x{:x}, {} bytes)",
                replaced.patched_sites, replaced.capsule_offset, replaced.capsule_size
            )?;
        }
        writeln!(f, "- Kernel: {}", self.kernel_release)?;
        if let Some(vermagic) = &self.vermagic {
            writeln!(
                f,
                "- Module vermagic overridden: {} -> {}",
                vermagic.previous, vermagic.current
            )?;
        }
        match (self.btf, self.arch) {
            (Some(btf), _) => writeln!(
                f,
                "- BTF: v1 at 0x{:x}, {} bytes, {} types",
                btf.file_offset, btf.size, btf.type_count
            )?,
            (None, Arch::Aarch64) => writeln!(f, "- BTF: unavailable; using built-in GKI ABI")?,
            (None, Arch::X86_64) => {}
        }
        writeln!(
            f,
            "- Kallsyms: {} ({} symbols)",
            self.kallsyms_layout, self.kallsyms_count
        )?;
        match &self.details {
            InjectionDetails::Arm64 {
                load_info_structure_size,
                load_info_storage_size,
                load_info_hdr_offset,
                load_info_len_offset,
                memblock_reserve_call,
                page_offset,
                image_size,
            } => {
                writeln!(
                    f,
                    "- Load ABI: load_info={}{} storage={} hdr={} len={}",
                    load_info_structure_size
                        .map_or_else(|| "unknown".to_owned(), |size| size.to_string()),
                    if load_info_structure_size.is_some() {
                        " (BTF)"
                    } else {
                        " (built-in)"
                    },
                    load_info_storage_size,
                    load_info_hdr_offset,
                    load_info_len_offset,
                )?;
                writeln!(
                    f,
                    "- Bootstrap: offset 0x{:x}, {} bytes",
                    self.bootstrap_offset, self.bootstrap_size
                )?;
                writeln!(f, "- Memblock patch: offset 0x{memblock_reserve_call:x}")?;
                writeln!(f, "- PAGE_OFFSET: 0x{page_offset:x}")?;
                self.write_fixups(f)?;
                writeln!(f, "- Patched Image size: 0x{image_size:x}")?;
            }
            InjectionDetails::X86_64 {
                capsule_offset,
                capsule_size,
                reservation_extension,
                payload_before,
                payload_after,
                payload_format,
                content_before,
                content_after,
                syssize,
            } => {
                writeln!(
                    f,
                    "- Bootstrap: offset 0x{:x}, {} bytes",
                    self.bootstrap_offset, self.bootstrap_size
                )?;
                writeln!(
                    f,
                    "- Capsule: offset 0x{capsule_offset:x}, {capsule_size} bytes, \
reservation extension 0x{reservation_extension:x}"
                )?;
                self.write_fixups(f)?;
                writeln!(
                    f,
                    "- Payload: 0x{payload_before:x} -> 0x{payload_after:x} bytes ({payload_format})"
                )?;
                writeln!(
                    f,
                    "- Image: 0x{content_before:x} -> 0x{content_after:x} bytes (syssize 0x{syssize:x})"
                )?;
            }
        }
        Ok(())
    }
}

impl ImageInjectionReport {
    fn write_fixups(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        writeln!(
            f,
            "- Module fixups: {}, unresolved: {}",
            self.fixup_count,
            self.unresolved.len()
        )?;
        if !self.unresolved.is_empty() {
            writeln!(
                f,
                "- Native resolver symbols: {}",
                self.unresolved.join(", ")
            )?;
        }
        Ok(())
    }
}

/// The `kernelsu.ko` that ships with KernelSU for `kmi`, as it is laid out in
/// the release assets (`aarch64/...`, `x86_64/...`); `boot-patch-v2` uses it
/// when `--module` is not given.
pub fn bundled_module_name(arch: Arch, kmi: &str) -> String {
    #[cfg(target_os = "android")]
    {
        let _ = arch;
        format!("{kmi}_kernelsu.ko")
    }
    #[cfg(not(target_os = "android"))]
    {
        format!("{}/{kmi}_kernelsu.ko", arch.module_directory())
    }
}

/// The boot image bytes and the raw kernel block inside it.
///
/// `boot_data` is `None` when the input already is a kernel image: the emulator
/// and a kernel build tree hand out the `bzImage`/`Image` itself, so there is no
/// container to unpack and the output is the patched kernel as it is.
///
/// `BootImage` borrows the boot image, so it is parsed again when the image is
/// repacked instead of being kept alive here.
struct BootKernel {
    boot_data: Option<Vec<u8>>,
    raw_kernel: Vec<u8>,
}

impl BootKernel {
    const fn is_boot_image(&self) -> bool {
        self.boot_data.is_some()
    }

    /// How the messages and the report name this input.
    const fn label(&self) -> &'static str {
        if self.is_boot_image() {
            "boot image"
        } else {
            "kernel image"
        }
    }
}

/// Whether `data` is a raw kernel image instead of an Android boot image.
///
/// The boot image magic is checked first: a boot image has arbitrary header
/// bytes where a raw kernel keeps its magic, and unpacking it is the only thing
/// that makes sense for it.
fn is_raw_kernel(data: &[u8]) -> bool {
    !data.starts_with(b"ANDROID!") && (x86_64::is_x86_bz_image(data) || arm64::is_arm64_image(data))
}

fn read_boot_kernel(boot: &Path) -> Result<BootKernel> {
    let data =
        fs::read(boot).with_context(|| format!("cannot read kernel image {}", boot.display()))?;
    if is_raw_kernel(&data) {
        println!(
            "- Reading raw kernel image ({})",
            detect_arch(&data).container_label()
        );
        return Ok(BootKernel {
            boot_data: None,
            raw_kernel: data,
        });
    }
    println!("- Reading boot image");
    let mut raw_kernel = Vec::new();
    {
        let boot_image = BootImage::parse(&data).with_context(|| {
            format!(
                "{} is neither a raw kernel image (Image/bzImage) nor an Android boot image",
                boot.display()
            )
        })?;
        let kernel = boot_image
            .get_blocks()
            .get_kernel()
            .context("boot image does not contain a kernel")?;
        println!("- Decompressing kernel");
        kernel
            .dump(&mut raw_kernel, false)
            .context("cannot decompress boot kernel")?;
    }
    Ok(BootKernel {
        boot_data: Some(data),
        raw_kernel,
    })
}

/// The architecture of a raw kernel block.
fn detect_arch(raw_kernel: &[u8]) -> Arch {
    if x86_64::is_x86_bz_image(raw_kernel) {
        Arch::X86_64
    } else {
        Arch::Aarch64
    }
}

/// Undo the container: a patched kernel image is written back as it is, a
/// patched boot kernel is compressed into the boot image it came from.
fn repack_boot_kernel(state: &BootKernel, kernel: Vec<u8>) -> Result<Vec<u8>> {
    let Some(boot_data) = &state.boot_data else {
        return Ok(kernel);
    };
    let boot_image = BootImage::parse(boot_data).context("cannot parse boot image")?;
    println!("- Repacking boot image");
    let mut patcher = BootImagePatchOption::new(&boot_image);
    patcher.replace_kernel(Box::new(Cursor::new(kernel)), false);
    let mut repacked = Cursor::new(Vec::with_capacity(boot_data.len()));
    patcher
        .patch(&mut repacked)
        .context("cannot repack boot image")?;
    Ok(repacked.into_inner())
}

fn write_boot_image(repacked: &[u8], output: &Path, source: &Path, label: &str) -> Result<()> {
    let output_parent = output
        .parent()
        .filter(|path| !path.as_os_str().is_empty())
        .unwrap_or_else(|| Path::new("."));
    fs::create_dir_all(output_parent)
        .with_context(|| format!("cannot create output directory {}", output_parent.display()))?;
    let mut temporary =
        tempfile::NamedTempFile::new_in(output_parent).context("cannot create temporary output")?;
    temporary
        .write_all(repacked)
        .with_context(|| format!("cannot write patched {label}"))?;
    temporary.flush().context("cannot flush patched output")?;
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;

        let mode = fs::metadata(source)
            .with_context(|| format!("cannot stat {label} {}", source.display()))?
            .permissions()
            .mode();
        temporary
            .as_file()
            .set_permissions(fs::Permissions::from_mode(mode))
            .with_context(|| format!("cannot copy {label} permissions"))?;
    }
    temporary.persist(output).map_err(|error| {
        anyhow!(
            "cannot persist {label} {}: {}",
            output.display(),
            error.error
        )
    })?;
    println!("- Output {label} written to {}", output.display());
    Ok(())
}

fn ensure_output_is_free(output: &Path, inputs: &[(&str, &Path)], force: bool) -> Result<()> {
    if !output.exists() {
        return Ok(());
    }
    let output = fs::canonicalize(output)
        .with_context(|| format!("cannot resolve output {}", output.display()))?;
    for (label, input) in inputs {
        let input = fs::canonicalize(input)
            .with_context(|| format!("cannot resolve {label} {}", input.display()))?;
        ensure!(output != input, "refusing to overwrite the input {label}");
    }
    ensure!(
        force,
        "output already exists: {}; use --force",
        output.display()
    );
    Ok(())
}

pub fn patch_boot(args: &BootPatchV2Args) -> Result<()> {
    ensure!(
        args.boot.exists(),
        "kernel image does not exist: {}",
        args.boot.display()
    );
    if let Some(module) = &args.module {
        ensure!(
            module.is_file(),
            "kernel module does not exist: {}",
            module.display()
        );
    }
    let mut inputs = vec![("input image", args.boot.as_path())];
    if let Some(module) = &args.module {
        inputs.push(("kernel module", module.as_path()));
    }
    ensure_output_is_free(&args.output, &inputs, args.force)?;

    let state = read_boot_kernel(&args.boot)?;
    let arch = detect_arch(&state.raw_kernel);

    let module = if let Some(module) = &args.module {
        println!("- Module: {}", module.display());
        fs::read(module)
            .with_context(|| format!("cannot read kernel module {}", module.display()))?
    } else {
        let kmi = boot_patch::parse_kmi(&state.raw_kernel)
            .context("cannot detect KMI from the boot image kernel")?;
        let name = bundled_module_name(arch, &kmi);
        println!("- KMI: {kmi}");
        println!("- Using bundled module: {name}");
        assets::get_asset_data(&name)
            .with_context(|| format!("no bundled KernelSU module for KMI {kmi}: {name}"))?
            .into_owned()
    };

    let load_args = args.load_args();
    println!(
        "- Module args: {}",
        if load_args.is_empty() {
            "(none)"
        } else {
            load_args.as_str()
        }
    );
    println!("- Recovering BTF/kallsyms and injecting module");
    let (kernel, report) = match arch {
        Arch::X86_64 => x86_64::inject_image(
            &state.raw_kernel,
            &module,
            &load_args,
            args.override_vermagic,
        )?,
        Arch::Aarch64 => arm64::inject_image(
            &state.raw_kernel,
            &module,
            &load_args,
            args.override_vermagic,
        )?,
    };
    report.print();

    let repacked = repack_boot_kernel(&state, kernel)?;
    write_boot_image(&repacked, &args.output, &args.boot, state.label())?;
    Ok(())
}

/// Undo a `boot-patch-v2` injection, using only what the capsule records.
pub fn restore_boot(args: &BootRestoreV2Args) -> Result<()> {
    ensure!(
        args.boot.exists(),
        "kernel image does not exist: {}",
        args.boot.display()
    );
    let output = args.output.clone().unwrap_or_else(default_restore_output);
    ensure_output_is_free(&output, &[("input image", args.boot.as_path())], args.force)?;

    let state = read_boot_kernel(&args.boot)?;
    let arch = detect_arch(&state.raw_kernel);
    println!("- Reading the restore record");
    let restored = match arch {
        Arch::X86_64 => x86_64::restore_image(&state.raw_kernel)?,
        Arch::Aarch64 => arm64::restore_image(&state.raw_kernel)?,
    };
    restored.report.print();

    if args.verify_only {
        println!("- Nothing written (--verify-only)");
        return Ok(());
    }

    let repacked = repack_boot_kernel(&state, restored.image)?;
    #[cfg(target_os = "android")]
    let flash = args.flash;
    #[cfg(not(target_os = "android"))]
    let flash = false;
    if args.output.is_some() || !flash {
        write_boot_image(&repacked, &output, &args.boot, state.label())?;
    }
    #[cfg(target_os = "android")]
    if flash {
        ensure!(
            state.is_boot_image(),
            "--flash writes a boot image; use --output with a raw kernel image"
        );
        let kmi = boot_patch::get_current_kmi().unwrap_or_default();
        let partition = boot_patch::auto_boot_partition_path(&kmi, false, false, &None);
        println!("- Flashing {}", partition.display());
        boot_patch::flash_partition(&partition.display().to_string(), &repacked)?;
    }
    Ok(())
}

/// A kernel block for the environment gated tests: a real boot image is parsed
/// and its kernel decompressed exactly the way `boot-patch-v2` does it, and
/// anything else is taken as an already decompressed kernel.
#[cfg(test)]
pub fn test_kernel_from(path: &str) -> Option<Vec<u8>> {
    let data = fs::read(path).ok()?;
    if !data.starts_with(b"ANDROID!") {
        return Some(data);
    }
    let boot_image = BootImage::parse(&data).ok()?;
    let mut kernel = Vec::new();
    boot_image
        .get_blocks()
        .get_kernel()?
        .dump(&mut kernel, false)
        .ok()?;
    Some(kernel)
}

fn default_restore_output() -> PathBuf {
    let now = chrono::Utc::now();
    PathBuf::from(format!(
        "kernelsu_restore_{}.img",
        now.format("%Y%m%d_%H%M%S")
    ))
}

#[cfg(test)]
mod tests {
    use clap::Parser;

    use super::*;

    #[derive(Parser)]
    struct TestCli {
        #[command(flatten)]
        patch: BootPatchV2Args,
    }

    fn parse(extra: &[&str]) -> Result<TestCli, clap::Error> {
        let mut argv = vec!["ksud", "-b", "boot.img", "-o", "out.img"];
        argv.extend_from_slice(extra);
        TestCli::try_parse_from(argv)
    }

    /// The named flags spell out the same `module_param` names the classic
    /// `boot-patch` writes into the ramdisk's `/ksu_config`.
    #[test]
    fn config_flags_become_load_args() {
        assert_eq!(parse(&[]).unwrap().patch.load_args(), "");
        assert_eq!(
            parse(&["--allow-shell"]).unwrap().patch.load_args(),
            "allow_shell=1"
        );
        assert_eq!(
            parse(&["--no-custom-rc", "--bundled"])
                .unwrap()
                .patch
                .load_args(),
            "bundled=1 norc=1"
        );
        assert_eq!(
            parse(&["--bundled", "--no-custom-rc", "--allow-shell"])
                .unwrap()
                .patch
                .load_args(),
            "bundled=1 norc=1 allow_shell=1"
        );
        assert_eq!(
            parse(&["--module-args", "ksu_probe=1"])
                .unwrap()
                .patch
                .load_args(),
            "ksu_probe=1"
        );
    }

    /// A raw string and the named flags describe the same thing, so clap has to
    /// reject the combination instead of silently picking one of them.
    #[test]
    fn module_args_conflicts_with_the_config_flags() {
        assert!(parse(&["--module-args", "x", "--allow-shell"]).is_err());
        assert!(parse(&["--module-args", "x", "--no-custom-rc"]).is_err());
        assert!(parse(&["--module-args", "x", "--bundled"]).is_err());
    }

    /// The emulator boots the `bzImage`/`Image` itself and a kernel build tree
    /// hands it out directly, so both have to be recognised as a kernel image
    /// that needs no unpacking -- the Android boot image is the only container.
    #[test]
    fn raw_kernel_images_are_detected() {
        let mut bz_image = vec![0_u8; 0x210];
        bz_image[x86_64::BOOT_FLAG_OFFSET..x86_64::BOOT_FLAG_OFFSET + 2]
            .copy_from_slice(&x86_64::BOOT_FLAG.to_le_bytes());
        bz_image[x86_64::HEADER_MAGIC_OFFSET..x86_64::HEADER_MAGIC_OFFSET + 4]
            .copy_from_slice(x86_64::HEADER_MAGIC);
        assert!(is_raw_kernel(&bz_image));
        assert_eq!(detect_arch(&bz_image), Arch::X86_64);

        let mut image = vec![0_u8; 0x40];
        image[arm64::ARM64_IMAGE_MAGIC_OFFSET..arm64::ARM64_IMAGE_MAGIC_OFFSET + 4]
            .copy_from_slice(arm64::ARM64_IMAGE_MAGIC);
        assert!(is_raw_kernel(&image));
        assert_eq!(detect_arch(&image), Arch::Aarch64);

        assert!(!is_raw_kernel(b"ANDROID!"));
        assert!(!is_raw_kernel(&[0_u8; 0x400]));

        // A boot image wins even when its header bytes look like a kernel
        // magic, because only unpacking it can find the kernel inside.
        let mut boot_image = vec![0_u8; 0x40];
        boot_image[..8].copy_from_slice(b"ANDROID!");
        boot_image[arm64::ARM64_IMAGE_MAGIC_OFFSET..arm64::ARM64_IMAGE_MAGIC_OFFSET + 4]
            .copy_from_slice(arm64::ARM64_IMAGE_MAGIC);
        assert!(!is_raw_kernel(&boot_image));
    }

    /// A kernel image input has no container to rebuild: the patched kernel is
    /// the output, and the messages call it a kernel image rather than a boot
    /// image.
    #[test]
    fn raw_kernel_input_is_written_back_as_it_is() {
        let state = BootKernel {
            boot_data: None,
            raw_kernel: vec![1, 2, 3],
        };
        assert_eq!(state.label(), "kernel image");
        assert!(!state.is_boot_image());
        assert_eq!(repack_boot_kernel(&state, vec![4, 5]).unwrap(), vec![4, 5]);

        let state = BootKernel {
            boot_data: Some(vec![0; 8]),
            raw_kernel: vec![1, 2, 3],
        };
        assert_eq!(state.label(), "boot image");
        assert!(state.is_boot_image());
    }
}
