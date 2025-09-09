use std::fs::{create_dir_all, File};
use std::io::Error as IoError;
use std::path::Path;

pub fn ensure_file_exists<T: AsRef<Path>>(file: T) -> Result<(), IoError> {
    match File::options().write(true).create_new(true).open(&file) {
        Ok(_) => Ok(()),
        Err(err) => {
            if err.kind() == std::io::ErrorKind::AlreadyExists && file.as_ref().is_file() {
                Ok(())
            } else {
                Err(err)
            }
        }
    }
}

pub fn ensure_dir_exists<T: AsRef<Path>>(dir: T) -> Result<(), IoError> {
    create_dir_all(&dir)
}

pub fn ensure_clean_dir(dir: impl AsRef<Path>) -> Result<(), IoError> {
    let path = dir.as_ref();
    if path.exists() {
        std::fs::remove_dir_all(path)?;
    }
    create_dir_all(path)
}

pub fn get_zip_uncompressed_size(zip_path: &str) -> Result<u64, Box<dyn std::error::Error>> {
    let mut zip = zip::ZipArchive::new(std::fs::File::open(zip_path)?)?;
    let total: u64 = (0..zip.len()).map(|i| zip.by_index(i).unwrap().size()).sum();
    Ok(total)
}

pub fn has_magisk() -> bool {
    which::which("magisk").is_ok()
}
