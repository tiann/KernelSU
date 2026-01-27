#![allow(clippy::ref_option, clippy::needless_pass_by_value)]

use android_bootimg::cpio::{Cpio, CpioEntry};
use android_bootimg::parser::BootImage;
use android_bootimg::patcher::BootImagePatchOption;
use anyhow::Result;
use anyhow::bail;
use anyhow::ensure;
use anyhow::{Context, anyhow};
use memmap2::{Mmap, MmapOptions};
use regex_lite::Regex;
use std::fs::{File, OpenOptions};
use std::io::{BufReader, Cursor, Read, Seek, SeekFrom};
#[cfg(unix)]
use std::os::unix::fs::PermissionsExt;
use std::path::PathBuf;

use crate::assets;

#[cfg(target_os = "android")]
mod android {
    use super::{PermissionsExt, Result};
    pub(super) use crate::defs::{BACKUP_FILENAME, KSU_BACKUP_DIR, KSU_BACKUP_FILE_PREFIX};
    use crate::utils;
    use android_bootimg::cpio::{Cpio, CpioEntry};
    use anyhow::{Context, anyhow, bail, ensure};
    use regex_lite::Regex;
    use std::fs::{File, OpenOptions};
    use std::os::fd::AsRawFd;
    use std::path::Path;
    use std::process::Command;

    pub(super) fn ensure_gki_kernel() -> Result<()> {
        let version = get_kernel_version()?;
        let is_gki = version.0 == 5 && version.1 >= 10 || version.2 > 5;
        ensure!(is_gki, "only support GKI kernel");
        Ok(())
    }

    pub fn get_kernel_version() -> Result<(i32, i32, i32)> {
        let uname = rustix::system::uname();
        let version = uname.release().to_string_lossy();
        let re = Regex::new(r"(\d+)\.(\d+)\.(\d+)")?;
        if let Some(captures) = re.captures(&version) {
            let major = captures
                .get(1)
                .and_then(|m| m.as_str().parse::<i32>().ok())
                .ok_or_else(|| anyhow!("Major version parse error"))?;
            let minor = captures
                .get(2)
                .and_then(|m| m.as_str().parse::<i32>().ok())
                .ok_or_else(|| anyhow!("Minor version parse error"))?;
            let patch = captures
                .get(3)
                .and_then(|m| m.as_str().parse::<i32>().ok())
                .ok_or_else(|| anyhow!("Patch version parse error"))?;
            Ok((major, minor, patch))
        } else {
            Err(anyhow!("Invalid kernel version string"))
        }
    }

    fn parse_kmi(version: &str) -> Result<String> {
        let re = Regex::new(r"(.* )?(\d+\.\d+)(\S+)?(android\d+)(.*)")?;
        let cap = re
            .captures(version)
            .ok_or_else(|| anyhow::anyhow!("Failed to get KMI from boot/modules"))?;
        let android_version = cap.get(4).map_or("", |m| m.as_str());
        let kernel_version = cap.get(2).map_or("", |m| m.as_str());
        Ok(format!("{android_version}-{kernel_version}"))
    }

    fn parse_kmi_from_uname() -> Result<String> {
        let uname = rustix::system::uname();
        let version = uname.release().to_string_lossy();
        parse_kmi(&version)
    }

    fn parse_kmi_from_modules() -> Result<String> {
        use std::io::BufRead;
        // find a *.ko in /vendor/lib/modules
        let modfile = std::fs::read_dir("/vendor/lib/modules")?
            .filter_map(Result::ok)
            .find(|entry| entry.path().extension().is_some_and(|ext| ext == "ko"))
            .map(|entry| entry.path())
            .ok_or_else(|| anyhow!("No kernel module found"))?;
        let output = Command::new("modinfo").arg(modfile).output()?;
        for line in output.stdout.lines().map_while(Result::ok) {
            if line.starts_with("vermagic") {
                return parse_kmi(&line);
            }
        }
        bail!("Parse KMI from modules failed")
    }

    pub fn get_current_kmi() -> Result<String> {
        parse_kmi_from_uname().or_else(|_| parse_kmi_from_modules())
    }

    fn calculate_sha1(file_path: impl AsRef<Path>) -> Result<String> {
        use sha1::Digest;
        use std::io::Read;
        let mut file = std::fs::File::open(file_path.as_ref())?;
        let mut hasher = sha1::Sha1::new();
        let mut buffer = [0; 1024];

        loop {
            let n = file.read(&mut buffer)?;
            if n == 0 {
                break;
            }
            hasher.update(&buffer[..n]);
        }

        let result = hasher.finalize();
        Ok(format!("{result:x}"))
    }

    pub(super) fn do_backup(cpio: &mut Cpio, image: &Path) -> Result<()> {
        let sha1 = calculate_sha1(image)?;
        let filename = format!("{KSU_BACKUP_FILE_PREFIX}{sha1}");

        println!("- Backup stock boot image");
        let target = format!("{KSU_BACKUP_DIR}{filename}");
        let mut target_file = OpenOptions::new()
            .create(true)
            .truncate(true)
            .write(true)
            .open(&target)?;
        let mut source = OpenOptions::new()
            .create(false)
            .truncate(false)
            .read(true)
            .write(false)
            .open(image)?;

        std::io::copy(&mut source, &mut target_file)
            .with_context(|| format!("backup to {target}"))?;

        let backup_file = CpioEntry::regular(0o755, Box::new(sha1));
        cpio.add(BACKUP_FILENAME, backup_file)?;
        println!("- Stock image has been backup to");
        println!("- {target}");
        Ok(())
    }

    pub(super) fn clean_backup(sha1: &str) -> Result<()> {
        println!("- Clean up backup");
        let backup_name = format!("{KSU_BACKUP_FILE_PREFIX}{sha1}");
        let dir = std::fs::read_dir(KSU_BACKUP_DIR)?;
        for entry in dir.flatten() {
            let path = entry.path();
            if !path.is_file() {
                continue;
            }
            if let Some(name) = path.file_name() {
                let name = name.to_string_lossy().to_string();
                if name != backup_name
                    && name.starts_with(KSU_BACKUP_FILE_PREFIX)
                    && std::fs::remove_file(path).is_ok()
                {
                    println!("- removed {name}");
                }
            }
        }
        Ok(())
    }

    pub fn choose_boot_partition(
        kmi: &str,
        is_replace_kernel: bool,
        partition: &Option<String>,
    ) -> String {
        let slot_suffix = get_slot_suffix(false);
        let skip_init_boot = kmi.starts_with("android12-");
        let init_boot_exist =
            Path::new(&format!("/dev/block/by-name/init_boot{slot_suffix}")).exists();

        // if specific partition is specified, use it
        if let Some(part) = partition {
            return match part.as_str() {
                "boot" | "init_boot" | "vendor_boot" => part.clone(),
                _ => "boot".to_string(),
            };
        }

        // if init_boot exists and not skipping it, use it
        if !is_replace_kernel && init_boot_exist && !skip_init_boot {
            return "init_boot".to_string();
        }

        "boot".to_string()
    }

    pub fn get_slot_suffix(ota: bool) -> String {
        let mut slot_suffix = utils::getprop("ro.boot.slot_suffix").unwrap_or_default();
        if !slot_suffix.is_empty() && ota {
            if slot_suffix == "_a" {
                slot_suffix = "_b".to_string();
            } else {
                slot_suffix = "_a".to_string();
            }
        }
        slot_suffix
    }

    #[cfg(target_os = "android")]
    pub fn list_available_partitions() -> Vec<String> {
        let slot_suffix = get_slot_suffix(false);
        let candidates = vec!["boot", "init_boot", "vendor_boot"];
        candidates
            .into_iter()
            .filter(|name| Path::new(&format!("/dev/block/by-name/{name}{slot_suffix}")).exists())
            .map(ToString::to_string)
            .collect()
    }

    #[cfg(target_os = "android")]
    pub(super) fn post_ota() -> Result<()> {
        use crate::assets::BOOTCTL_PATH;
        use crate::defs::ADB_DIR;
        let status = Command::new(BOOTCTL_PATH).arg("hal-info").status()?;
        if !status.success() {
            return Ok(());
        }

        let current_slot = Command::new(BOOTCTL_PATH)
            .arg("get-current-slot")
            .output()?
            .stdout;
        let current_slot = String::from_utf8(current_slot)?;
        let current_slot = current_slot.trim();
        let target_slot = i32::from(current_slot == "0");

        Command::new(BOOTCTL_PATH)
            .arg(format!("set-active-boot-slot {target_slot}"))
            .status()?;

        let post_fs_data = Path::new(ADB_DIR).join("post-fs-data.d");
        utils::ensure_dir_exists(&post_fs_data)?;
        let post_ota_sh = post_fs_data.join("post_ota.sh");

        let sh_content = format!(
            r"
{BOOTCTL_PATH} mark-boot-successful
rm -f {BOOTCTL_PATH}
rm -f /data/adb/post-fs-data.d/post_ota.sh
"
        );

        std::fs::write(&post_ota_sh, sh_content)?;
        #[cfg(unix)]
        std::fs::set_permissions(post_ota_sh, std::fs::Permissions::from_mode(0o755))?;

        Ok(())
    }

    pub(super) fn open_block_for_write(block_dev: &String) -> Result<File> {
        let file = File::open(block_dev)?;
        unsafe {
            const BLKROSET: i32 = libc::_IO(0x12, 93);
            let mut val: libc::c_int = 0;
            if libc::ioctl(file.as_raw_fd(), BLKROSET, &raw mut val) != 0 {
                bail!("Failed to set rw for {block_dev}: {}", *libc::__errno());
            }
        }

        Ok(OpenOptions::new()
            .write(true)
            .truncate(false)
            .create(false)
            .open(block_dev)?)
    }
}

#[cfg(target_os = "android")]
pub use android::*;

fn parse_kmi(buffer: Vec<u8>) -> Result<String> {
    let re = Regex::new(r"(\d+\.\d+)(?:\S+)?(android\d+)").context("Failed to compile regex")?;
    buffer
        .windows(3)
        .enumerate()
        .filter(|(_, x)| {
            x[1] == b'.' && (x[0] == b'5' || x[0] == b'6') && (x[2] >= b'0' && x[2] <= b'9')
        })
        .find_map(|(i, _)| {
            let a = &buffer[i..buffer.len().min(i + 100)];
            if let Some(e) = a.iter().position(|c| *c == 0)
                && let Ok(s) = std::str::from_utf8(&a[..e])
                && let Some(caps) = re.captures(s)
                && let (Some(kernel_version), Some(android_version)) = (caps.get(1), caps.get(2))
            {
                Some(format!(
                    "{}-{}",
                    android_version.as_str(),
                    kernel_version.as_str()
                ))
            } else {
                None
            }
        })
        .ok_or_else(|| {
            println!("- Failed to get KMI version");
            anyhow!("Try to choose LKM manually")
        })
}

fn parse_kmi_from_kernel(kernel: &PathBuf) -> Result<String> {
    let file = File::open(kernel).context("Failed to open kernel file")?;
    let mut reader = BufReader::new(file);
    let mut buffer = Vec::new();
    reader
        .read_to_end(&mut buffer)
        .context("Failed to read kernel file")?;

    parse_kmi(buffer)
}

fn parse_kmi_from_boot(image: &PathBuf) -> Result<String> {
    let image = unsafe { Mmap::map(&File::open(image)?)? };

    let bootimage = BootImage::parse(&image)?;
    if let Some(kernel) = bootimage.get_blocks().get_kernel() {
        let mut output = Vec::<u8>::new();
        kernel.dump(&mut output, false)?;
        parse_kmi(output)
    } else {
        bail!("no kernel found in boot image")
    }
}

#[derive(clap::Args, Debug)]
pub struct BootPatchArgs {
    /// boot image path, if not specified, will try to find the boot image automatically
    #[arg(short, long)]
    pub boot: Option<PathBuf>,

    /// kernel image path to replace
    #[arg(short, long)]
    pub kernel: Option<PathBuf>,

    /// LKM module path to replace, if not specified, will use the builtin one
    #[arg(short, long)]
    pub module: Option<PathBuf>,

    /// init to be replaced
    #[arg(short, long, requires("module"))]
    pub init: Option<PathBuf>,

    /// will use another slot when boot image is not specified
    #[cfg(target_os = "android")]
    #[arg(short = 'u', long, default_value = "false")]
    pub ota: bool,

    /// Flash it to boot partition after patch
    #[cfg(target_os = "android")]
    #[arg(short, long, default_value = "false")]
    pub flash: bool,

    /// output path, if not specified, will use current directory
    #[arg(short, long, default_value = None)]
    pub out: Option<PathBuf>,

    /// KMI version, if specified, will use the specified KMI
    #[arg(long, default_value = None)]
    pub kmi: Option<String>,

    /// target partition override (init_boot | boot | vendor_boot)
    #[cfg(target_os = "android")]
    #[arg(long, default_value = None)]
    pub partition: Option<String>,

    /// File name of the output.
    #[arg(long, default_value = None)]
    pub out_name: Option<String>,
}

pub fn patch(args: BootPatchArgs) -> Result<()> {
    let inner = move || {
        let BootPatchArgs {
            boot: image,
            init,
            kernel,
            module: kmod,
            out,
            kmi,
            out_name,
            ..
        } = args;
        #[cfg(target_os = "android")]
        let BootPatchArgs {
            ota,
            flash,
            partition,
            ..
        } = args;

        println!(include_str!("banner"));

        #[cfg(target_os = "android")]
        if image.is_none() {
            ensure_gki_kernel()?;
        }

        let is_replace_kernel = kernel.is_some();

        if is_replace_kernel {
            ensure!(
                init.is_none() && kmod.is_none(),
                "init and module must not be specified."
            );
        }

        let kmi = kmi.map_or_else(
            || -> Result<_> {
                #[cfg(target_os = "android")]
                match get_current_kmi() {
                    Ok(value) => {
                        return Ok(value);
                    }
                    Err(e) => {
                        println!("- {e}");
                    }
                }
                Ok(if let Some(image_path) = &image {
                    println!(
                        "- Trying to auto detect KMI version for {}",
                        image_path.display()
                    );
                    parse_kmi_from_boot(image_path)?
                } else if let Some(kernel_path) = &kernel {
                    println!(
                        "- Trying to auto detect KMI version for {}",
                        kernel_path.display()
                    );
                    parse_kmi_from_kernel(kernel_path)?
                } else {
                    String::new()
                })
            },
            Ok,
        )?;

        let output_to_file;

        #[cfg(target_os = "android")]
        {
            output_to_file = !flash;
            if flash && image.is_some() && partition.is_none() {
                bail!("Partition is required")
            }
        }

        #[cfg(not(target_os = "android"))]
        {
            output_to_file = true;
        }

        #[cfg(target_os = "android")]
        let boot_partition;

        #[cfg(target_os = "android")]
        {
            let slot_suffix = get_slot_suffix(ota);
            let boot_partition_name = choose_boot_partition(&kmi, is_replace_kernel, &partition);
            boot_partition = format!("/dev/block/by-name/{boot_partition_name}{slot_suffix}");

            println!("- Bootdevice: {boot_partition}");
        }

        let boot_image_file = if let Some(image) = image {
            ensure!(image.exists(), "boot image not found");
            image
        } else {
            #[cfg(not(target_os = "android"))]
            {
                bail!("Please specify a boot image");
            }

            #[cfg(target_os = "android")]
            PathBuf::from(&boot_partition)
        };

        // try extract bootctl
        #[cfg(target_os = "android")]
        let _ = assets::ensure_binaries(false);

        println!("- Parsing boot image");

        let boot_image_data = map_file(&boot_image_file)?;
        let boot_image = BootImage::parse(&boot_image_data)?;
        let mut patcher = BootImagePatchOption::new(&boot_image);

        if let Some(kernel) = kernel {
            println!("- Adding Kernel");
            let kernel = map_file(&kernel)?;
            patcher.replace_kernel(Box::new(Cursor::new(kernel)), false);
        } else {
            println!("- Adding KernelSU LKM");

            let kernelsu_ko = if let Some(kmod) = kmod {
                Box::new(map_file(&kmod)?)
            } else {
                // If kmod is not specified, extract from assets
                println!("- KMI: {kmi}");
                let name = format!("{kmi}_kernelsu.ko");
                assets::get_asset(&name)?
            };

            let ksu_init = if let Some(init) = init {
                Box::new(map_file(&init)?)
            } else {
                assets::get_asset("ksuinit")?
            };

            let (mut cpio, vendor_ramdisk_idx) =
                if let Some(ramdisk_image) = boot_image.get_blocks().get_ramdisk() {
                    if ramdisk_image.is_vendor_ramdisk() {
                        let (pos, target) = ramdisk_image
                            .iter_vendor_ramdisk()
                            .enumerate()
                            .find(|entry| entry.1.get_name_raw() == b"")
                            .or_else(|| {
                                ramdisk_image
                                    .iter_vendor_ramdisk()
                                    .enumerate()
                                    .find(|entry| entry.1.get_name_raw() == b"init_boot")
                            })
                            .ok_or_else(|| anyhow!("No suitable vendor ramdisk entry found"))?;

                        let mut ramdisk = Vec::<u8>::new();
                        target.dump(&mut ramdisk, false)?;
                        (Cpio::load_from_data(ramdisk.as_slice())?, Some(pos))
                    } else {
                        let mut ramdisk = Vec::<u8>::new();
                        ramdisk_image.dump(&mut ramdisk, false)?;
                        (Cpio::load_from_data(ramdisk.as_slice())?, None)
                    }
                } else {
                    (Cpio::new(), None)
                };

            let is_magisk_patched = cpio.is_magisk_patched();
            ensure!(!is_magisk_patched, "Cannot work with Magisk patched image");

            let is_kernelsu_patched = cpio.exists("kernelsu.ko");

            if !is_kernelsu_patched {
                // kernelsu.ko is not exist, backup init if necessary
                if cpio.exists("init") {
                    cpio.mv("init", "init.real")?;
                }
            }

            let ksu_init = CpioEntry::regular(0o755, ksu_init);
            let kernelsu_ko = CpioEntry::regular(0o755, kernelsu_ko);

            cpio.add("init", ksu_init)?;
            cpio.add("kernelsu.ko", kernelsu_ko)?;

            #[cfg(target_os = "android")]
            if !is_kernelsu_patched
                && flash
                && let Err(e) = do_backup(&mut cpio, boot_image_file.as_path())
            {
                println!("- Backup stock image failed: {e:?}");
            }

            let mut new_cpio = Vec::<u8>::new();
            cpio.dump(&mut new_cpio)?;

            if let Some(idx) = vendor_ramdisk_idx {
                patcher.replace_vendor_ramdisk(idx, Box::new(Cursor::new(new_cpio)), false);
            } else {
                patcher.replace_ramdisk(Box::new(Cursor::new(new_cpio)), false);
            }
        }

        println!("- Repacking boot image");

        let mut output_file = if output_to_file {
            // if image is specified, write to output file
            let output_dir = out.unwrap_or(std::env::current_dir()?);
            let name = out_name.unwrap_or_else(|| {
                let now = chrono::Utc::now();
                format!("kernelsu_patched_{}.img", now.format("%Y%m%d_%H%M%S"))
            });
            let output_image = output_dir.join(name);
            let output = OpenOptions::new()
                .write(true)
                .truncate(true)
                .create(true)
                .open(&output_image)?;
            println!("- Output file is written to");
            println!("- {}", output_image.display().to_string().trim_matches('"'));
            output
        } else {
            #[cfg(target_os = "android")]
            {
                // We should not read and write boot dev at same time
                tempfile::Builder::new()
                    .prefix("KernelSU_tmp_boot")
                    .tempfile()?
                    .into_file()
            }

            #[cfg(not(target_os = "android"))]
            unreachable!()
        };

        patcher.patch(&mut output_file)?;

        #[cfg(target_os = "android")]
        if flash {
            println!("- Flashing new boot image");
            let mut dev = open_block_for_write(&boot_partition)?;
            output_file.rewind()?;
            std::io::copy(&mut output_file, &mut dev)?;

            if ota {
                post_ota()?;
            }
        }

        println!("- Done!");
        Ok(())
    };

    let result = inner();
    if let Err(ref e) = result {
        println!("- Patch Error: {e}");
    }
    result
}

#[derive(clap::Args, Debug)]
pub struct BootRestoreArgs {
    /// boot image path, if not specified, will try to find the boot image automatically
    #[arg(short, long)]
    pub boot: Option<PathBuf>,

    /// Flash it to boot partition after restore
    #[cfg(target_os = "android")]
    #[arg(short, long, default_value = "false")]
    pub flash: bool,

    /// Always use stock boot image for restoring
    #[cfg(target_os = "android")]
    #[arg(short, long, default_value = "false")]
    pub stock: bool,

    /// File name of the output.
    #[arg(long, default_value = None)]
    pub out_name: Option<String>,

    /// target partition override (init_boot | boot | vendor_boot)
    #[cfg(target_os = "android")]
    #[arg(long, default_value = None)]
    pub partition: Option<String>,
}

pub fn restore(args: BootRestoreArgs) -> Result<()> {
    let inner = move || -> Result<()> {
        let BootRestoreArgs {
            boot: image,
            out_name,
            ..
        } = args;
        #[cfg(target_os = "android")]
        let BootRestoreArgs {
            flash,
            partition,
            stock,
            ..
        } = args;

        #[cfg(target_os = "android")]
        let kmi = get_current_kmi().unwrap_or_default();

        let output_to_file;

        #[cfg(target_os = "android")]
        {
            output_to_file = !flash;
            if flash && image.is_some() {
                bail!("Can't use image and --flash together")
            }
        }

        #[cfg(not(target_os = "android"))]
        {
            output_to_file = true;
        }

        #[cfg(target_os = "android")]
        let boot_partition;

        #[cfg(target_os = "android")]
        {
            let slot_suffix = get_slot_suffix(false);
            let boot_partition_name = choose_boot_partition(&kmi, false, &partition);
            boot_partition = format!("/dev/block/by-name/{boot_partition_name}{slot_suffix}");

            println!("- Bootdevice: {boot_partition}");
        }

        let boot_image_file = if let Some(image) = image {
            ensure!(image.exists(), "boot image not found");
            image
        } else {
            #[cfg(not(target_os = "android"))]
            {
                bail!("Please specify a boot image");
            }

            #[cfg(target_os = "android")]
            PathBuf::from(&boot_partition)
        };

        let bootimage_data = map_file(&boot_image_file)?;
        let boot_image = BootImage::parse(&bootimage_data)?;

        let (mut cpio, vendor_ramdisk_idx) =
            if let Some(ramdisk_image) = boot_image.get_blocks().get_ramdisk() {
                if ramdisk_image.is_vendor_ramdisk() {
                    let (pos, target) = ramdisk_image
                        .iter_vendor_ramdisk()
                        .enumerate()
                        .find(|entry| entry.1.get_name_raw() == b"")
                        .or_else(|| {
                            ramdisk_image
                                .iter_vendor_ramdisk()
                                .enumerate()
                                .find(|entry| entry.1.get_name_raw() == b"init_boot")
                        })
                        .ok_or_else(|| anyhow!("No suitable vendor ramdisk entry found"))?;

                    let mut ramdisk = Vec::<u8>::new();
                    target.dump(&mut ramdisk, false)?;
                    (Cpio::load_from_data(ramdisk.as_slice())?, Some(pos))
                } else {
                    let mut ramdisk = Vec::<u8>::new();
                    ramdisk_image.dump(&mut ramdisk, false)?;
                    (Cpio::load_from_data(ramdisk.as_slice())?, None)
                }
            } else {
                bail!("No compatible ramdisk found.")
            };
        let is_kernelsu_patched = cpio.exists("kernelsu.ko");
        ensure!(is_kernelsu_patched, "boot image is not patched by KernelSU");

        #[cfg(target_os = "android")]
        let mut stock_boot = None;

        #[cfg(target_os = "android")]
        if let Some(backup_file) = cpio.entry_by_name(BACKUP_FILENAME) {
            let sha = String::from_utf8(backup_file.data().unwrap().to_vec())?;
            let sha = sha.trim();
            let backup_path =
                PathBuf::from(KSU_BACKUP_DIR).join(format!("{KSU_BACKUP_FILE_PREFIX}{sha}"));
            if backup_path.is_file() {
                println!("- Using backup file {}", backup_path.display());
                stock_boot = Some(backup_path);
            } else if stock {
                bail!(
                    "Error: no stock boot image {} found!",
                    backup_path.display()
                )
            } else {
                println!("- Warning: no backup {} found!", backup_path.display());
            }

            if let Err(e) = clean_backup(sha) {
                println!("- Warning: Cleanup backup image failed: {e}");
            }
        } else if stock {
            bail!("Error: no stock boot image found!")
        } else {
            println!("- Backup info is absent!");
        }

        let mut output_file = if output_to_file {
            // if image is specified, write to output file
            let output_dir = std::env::current_dir()?;
            let name = out_name.unwrap_or_else(|| {
                let now = chrono::Utc::now();
                format!("kernelsu_patched_{}.img", now.format("%Y%m%d_%H%M%S"))
            });
            let output_image = output_dir.join(name);
            let output = OpenOptions::new()
                .write(true)
                .truncate(true)
                .create(true)
                .open(&output_image)?;
            println!("- Output file is written to");
            println!("- {}", output_image.display().to_string().trim_matches('"'));
            output
        } else {
            #[cfg(target_os = "android")]
            {
                if stock_boot.is_some() {
                    open_block_for_write(&boot_partition)?
                } else {
                    tempfile::Builder::new()
                        .prefix("KernelSU_tmp_boot")
                        .tempfile()?
                        .into_file()
                }
            }

            #[cfg(not(target_os = "android"))]
            unreachable!()
        };

        #[cfg(target_os = "android")]
        if let Some(stock_boot) = stock_boot {
            if flash {
                println!("- Flashing stock boot image");
            }
            let mut stock_boot = File::open(stock_boot)?;
            std::io::copy(&mut stock_boot, &mut output_file).context("copy out new boot failed")?;
            return Ok(());
        }

        println!("- Removing KernelSU from boot image");
        // remove kernelsu.ko
        cpio.rm("kernelsu.ko", false);

        // if init.real exists, restore it
        if cpio.exists("init.real") {
            cpio.mv("init.real", "init")?;
        }

        let mut new_cpio = Vec::<u8>::new();
        cpio.dump(&mut new_cpio)?;
        println!("- Repacking boot image");

        let mut patcher = BootImagePatchOption::new(&boot_image);
        if let Some(idx) = vendor_ramdisk_idx {
            patcher.replace_vendor_ramdisk(idx, Box::new(Cursor::new(new_cpio)), false);
        } else {
            patcher.replace_ramdisk(Box::new(Cursor::new(new_cpio)), false);
        }
        patcher.patch(&mut output_file)?;

        #[cfg(target_os = "android")]
        if flash {
            println!("- Flashing restored boot image");
            let mut boot = open_block_for_write(&boot_partition)?;
            output_file.rewind()?;
            std::io::copy(&mut output_file, &mut boot)?;
        }

        println!("- Done!");
        Ok(())
    };

    let result = inner();
    if let Err(ref e) = result {
        println!("- Restore Error: {e}");
    }
    result
}

fn map_file(file: &PathBuf) -> Result<Mmap> {
    unsafe {
        let mut file = File::open(file)?;
        Ok(MmapOptions::new()
            .len(file.seek(SeekFrom::End(0))? as usize)
            .map(&file)?)
    }
}
