use anyhow::{bail, ensure, Context, Error, Ok, Result};
use retry::delay::NoDelay;
use std::{
    fs::{create_dir_all, set_permissions, write, File, Permissions},
    io::ErrorKind::AlreadyExists,
    os::unix::prelude::PermissionsExt,
    path::Path,
};
use sys_mount::{unmount, FilesystemType, Mount, UnmountFlags};

fn do_mount_image(src: &str, target: &str) -> Result<()> {
    Mount::builder()
        .fstype(FilesystemType::from("ext4"))
        .mount(src, target)
        .with_context(|| format!("Failed to do mount: {src} -> {target}"))?;
    Ok(())
}

pub fn mount_image(src: &str, target: &str) -> Result<()> {
    // umount target first.
    let _ = umount_dir(target);
    let result = retry::retry(NoDelay.take(3), || do_mount_image(src, target));
    ensure!(result.is_ok(), "Failed to mount {} -> {}", src, target);
    Ok(())
}

pub fn umount_dir(src: &str) -> Result<()> {
    unmount(src, UnmountFlags::empty()).with_context(|| format!("Failed to umount {src}"))?;
    Ok(())
}

pub fn ensure_clean_dir(dir: &str) -> Result<()> {
    let path = Path::new(dir);
    if path.exists() {
        std::fs::remove_dir_all(path)?;
    }
    Ok(std::fs::create_dir_all(path)?)
}

pub fn ensure_file_exists<T: AsRef<Path>>(file: T) -> Result<()> {
    match File::options().write(true).create_new(true).open(&file) {
        std::result::Result::Ok(_) => Ok(()),
        Err(err) => {
            if err.kind() == AlreadyExists && file.as_ref().is_file() {
                Ok(())
            } else {
                Err(Error::from(err)).with_context(|| {
                    format!("{} is not a regular file", file.as_ref().to_str().unwrap())
                })
            }
        }
    }
}

pub fn ensure_dir_exists<T: AsRef<Path>>(dir: T) -> Result<()> {
    let result = create_dir_all(&dir).map_err(Error::from);
    if dir.as_ref().is_dir() {
        result
    } else if result.is_ok() {
        bail!(
            "{} is not a regular directory",
            dir.as_ref().to_str().unwrap()
        )
    } else {
        result
    }
}

pub fn ensure_binary<T: AsRef<Path>>(path: T, contents: &[u8]) -> Result<()> {
    if path.as_ref().exists() {
        return Ok(());
    }

    ensure_dir_exists(path.as_ref().parent().ok_or_else(|| {
        anyhow::anyhow!(
            "{} does not have parent directory",
            path.as_ref().to_string_lossy()
        )
    })?)?;

    write(&path, contents)?;
    set_permissions(&path, Permissions::from_mode(0o755))?;
    Ok(())
}

pub fn getprop(prop: &str) -> Option<String> {
    android_properties::getprop(prop).value()
}

pub fn is_safe_mode() -> bool {
    getprop("persist.sys.safemode")
        .filter(|prop| prop == "1")
        .is_some()
        || getprop("ro.sys.safemode")
            .filter(|prop| prop == "1")
            .is_some()
}

pub fn get_zip_uncompressed_size(zip_path: &str) -> Result<u64> {
    let mut zip = zip::ZipArchive::new(std::fs::File::open(zip_path)?)?;
    let total: u64 = (0..zip.len())
        .map(|i| zip.by_index(i).unwrap().size())
        .sum();
    Ok(total)
}
