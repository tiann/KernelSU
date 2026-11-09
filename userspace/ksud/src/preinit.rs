//! Preinit: copy each enabled module's `preinit/` dir to the preinit partition (/metadata). On the
//! next boot the runner embedded in the kernel executes their `preinit.conf` before SELinux policy
//! is loaded, see kernel/preinit/runner/runner.c.

use crate::{defs, restorecon::lsetfilecon, utils::ensure_dir_exists};

use anyhow::{Context, Result, bail};
use log::{info, warn};

use std::{
    collections::{BTreeMap, BTreeSet, HashMap},
    fmt::Write as _,
    fs::{self, File, Permissions},
    os::unix::fs::{MetadataExt, PermissionsExt, lchown},
    path::{Path, PathBuf},
};

// Space accounting is done in filesystem blocks
const BLOCK: u64 = 4096;
// Upper bound for everything under preinit/
const LIMIT_TOTAL: u64 = 4 << 20;
// Always leave this much free on /metadata: it also holds vold keys and OTA state
const RESERVE: u64 = 8 << 20;
// Must match BOOTCOUNT_LIMIT in the runner
const BOOTCOUNT_LIMIT: u64 = 3;

const STATE_OK: &str = "ok";
const STATE_NO_SPACE: &str = "no_space";
const STATE_ERROR: &str = "error";
const STATE_BOOTLOOP: &str = "bootloop";
const STATE_UNSUPPORTED: &str = "unsupported";

/// A file or directory inside a module's preinit dir.
struct Entry {
    rel: PathBuf,
    is_dir: bool,
    size: u64,
    // ctime, not mtime: extraction keeps the zip mtime but always sets ctime
    ctime: (i64, i64),
    mode: u32,
}

impl Entry {
    fn manifest_line(&self, id: &str) -> String {
        format!(
            "{id}\t{}\t{}\t{}.{}\t{:o}\t{}",
            if self.is_dir { 'd' } else { 'f' },
            self.size,
            self.ctime.0,
            self.ctime.1,
            self.mode,
            self.rel.display()
        )
    }
}

struct Status {
    id: String,
    state: &'static str,
    message: String,
}

const fn round_up(size: u64) -> u64 {
    size.div_ceil(BLOCK) * BLOCK
}

fn scan(root: &Path, rel: &Path, out: &mut Vec<Entry>) -> Result<()> {
    let dir = root.join(rel);
    let mut entries: Vec<_> = fs::read_dir(&dir)
        .with_context(|| format!("Failed to read {}", dir.display()))?
        .flatten()
        .collect();
    entries.sort_by_key(fs::DirEntry::file_name);
    for entry in entries {
        let rel = rel.join(entry.file_name());
        let meta = entry.metadata()?;
        let file_type = meta.file_type();
        if !file_type.is_dir() && !file_type.is_file() {
            bail!("unsupported file type: {}", rel.display());
        }
        out.push(Entry {
            rel: rel.clone(),
            is_dir: file_type.is_dir(),
            size: if file_type.is_file() { meta.len() } else { 0 },
            ctime: (meta.ctime(), meta.ctime_nsec()),
            mode: meta.mode() & 0o7777,
        });
        if file_type.is_dir() {
            scan(root, &rel, out)?;
        }
    }
    Ok(())
}

/// Space the copy will take, the module dir itself included.
fn usage(entries: &[Entry]) -> u64 {
    BLOCK
        + entries
            .iter()
            .map(|e| if e.is_dir { BLOCK } else { round_up(e.size) })
            .sum::<u64>()
}

/// Space currently allocated under `path`.
fn disk_usage(path: &Path) -> u64 {
    let Ok(meta) = fs::symlink_metadata(path) else {
        return 0;
    };
    let mut total = meta.blocks() * 512;
    if meta.is_dir()
        && let Ok(entries) = fs::read_dir(path)
    {
        total += entries
            .flatten()
            .map(|e| disk_usage(&e.path()))
            .sum::<u64>();
    }
    total
}

fn available(path: &Path) -> Result<u64> {
    let st = rustix::fs::statvfs(path)
        .with_context(|| format!("Failed to statvfs {}", path.display()))?;
    Ok(st.f_bavail * st.f_frsize)
}

/// The runner refuses anything that is not owned by root or is writable by group/other.
fn secure(path: &Path, mode: u32, con: &str) -> Result<()> {
    lchown(path, Some(0), Some(0))
        .with_context(|| format!("Failed to chown {}", path.display()))?;
    fs::set_permissions(path, Permissions::from_mode(mode & 0o755))
        .with_context(|| format!("Failed to chmod {}", path.display()))?;
    if let Err(e) = lsetfilecon(path, con) {
        warn!("{e:#}");
    }
    Ok(())
}

fn sync_dir(path: &Path) {
    if let Err(e) = File::open(path).and_then(|f| f.sync_all()) {
        warn!("Failed to sync {}: {e}", path.display());
    }
}

/// Copy a module into `.<id>.tmp`, then swap it in for `<id>`.
fn install(preinit_dir: &Path, id: &str, src: &Path, entries: &[Entry], con: &str) -> Result<()> {
    let tmp = preinit_dir.join(format!(".{id}.tmp"));
    if tmp.exists() {
        fs::remove_dir_all(&tmp)?;
    }
    fs::create_dir(&tmp).with_context(|| format!("Failed to create {}", tmp.display()))?;
    secure(&tmp, 0o755, con)?;

    for entry in entries {
        let to = tmp.join(&entry.rel);
        if entry.is_dir {
            fs::create_dir(&to).with_context(|| format!("Failed to create {}", to.display()))?;
        } else {
            let from = src.join(&entry.rel);
            fs::copy(&from, &to).with_context(|| format!("Failed to copy {}", from.display()))?;
            File::open(&to)?.sync_all()?;
        }
        secure(&to, entry.mode, con)?;
    }
    for entry in entries.iter().filter(|e| e.is_dir) {
        sync_dir(&tmp.join(&entry.rel));
    }
    sync_dir(&tmp);

    let dst = preinit_dir.join(id);
    let old = preinit_dir.join(format!(".{id}.old"));
    if dst.exists() {
        if old.exists() {
            fs::remove_dir_all(&old)?;
        }
        fs::rename(&dst, &old)?;
    }
    fs::rename(&tmp, &dst)?;
    sync_dir(preinit_dir);
    if old.exists() {
        fs::remove_dir_all(&old)?;
    }
    Ok(())
}

/// Whether `dst` already holds exactly what `install` would copy from `src`: touching the module
/// files (chmod, chcon) changes their ctime but must not count as new content.
fn same_copy(dst: &Path, src: &Path, entries: &[Entry]) -> bool {
    let mut existing = Vec::new();
    if scan(dst, Path::new(""), &mut existing).is_err() || existing.len() != entries.len() {
        return false;
    }
    entries.iter().zip(&existing).all(|(want, have)| {
        want.rel == have.rel
            && want.is_dir == have.is_dir
            && want.mode & 0o755 == have.mode
            && (want.is_dir
                || (want.size == have.size
                    && matches!(
                        (fs::read(src.join(&want.rel)), fs::read(dst.join(&have.rel))),
                        (Ok(a), Ok(b)) if a == b
                    )))
    })
}

fn read_manifest(path: &Path) -> HashMap<String, Vec<String>> {
    let mut manifest: HashMap<String, Vec<String>> = HashMap::new();
    let Ok(content) = fs::read_to_string(path) else {
        return manifest;
    };
    for line in content.lines() {
        if let Some((id, _)) = line.split_once('\t') {
            manifest
                .entry(id.to_owned())
                .or_default()
                .push(line.to_owned());
        }
    }
    manifest
}

fn write_atomic(path: &Path, content: &str) -> Result<()> {
    let tmp = path.with_extension("tmp");
    fs::write(&tmp, content).with_context(|| format!("Failed to write {}", tmp.display()))?;
    File::open(&tmp)?.sync_all()?;
    fs::rename(&tmp, path).with_context(|| format!("Failed to rename to {}", path.display()))?;
    Ok(())
}

fn write_status(status: &[Status]) {
    let mut content = String::new();
    for s in status {
        let _ = writeln!(content, "{} {} {}", s.id, s.state, s.message);
    }
    let result = ensure_dir_exists(defs::WORKING_DIR)
        .and_then(|()| write_atomic(Path::new(defs::PREINIT_STATUS_PATH), &content));
    if let Err(e) = result {
        warn!("Failed to write preinit status: {e:#}");
    }
}

fn remove_entry(path: &Path) -> bool {
    let removed = if path.is_dir() {
        fs::remove_dir_all(path)
    } else {
        fs::remove_file(path)
    };
    if let Err(e) = &removed {
        warn!("Failed to remove {}: {e}", path.display());
    }
    removed.is_ok()
}

fn remove_all(ksu_dir: &Path) {
    let preinit_dir = ksu_dir.join(defs::PREINIT_SUBDIR);
    if preinit_dir.exists()
        && let Err(e) = fs::remove_dir_all(&preinit_dir)
    {
        warn!("Failed to remove {}: {e}", preinit_dir.display());
    }
    fs::remove_file(ksu_dir.join(defs::PREINIT_BOOTCOUNT_FILE)).ok();
}

/// Make `<ksu_dir>/preinit/` mirror the `preinit/` dirs of `modules` (id -> module dir, enabled
/// modules for the next boot only). Unchanged modules are left alone to spare /metadata writes.
pub fn sync(
    ksu_dir: &Path,
    stale_ksu_dirs: &[&Path],
    modules: &BTreeMap<String, PathBuf>,
    con: &str,
    supported: bool,
) -> Result<()> {
    for stale in stale_ksu_dirs {
        remove_all(stale);
    }

    // Nothing would run the copies: keep /metadata clean and say why.
    if !supported {
        remove_all(ksu_dir);
        let status: Vec<Status> = modules
            .iter()
            .filter(|(_, dir)| dir.join(defs::MODULE_PREINIT_DIR).is_dir())
            .map(|(id, _)| Status {
                id: id.clone(),
                state: STATE_UNSUPPORTED,
                message: "kernel does not run preinit".to_owned(),
            })
            .collect();
        write_status(&status);
        return Ok(());
    }

    let mut status = Vec::new();
    let mut candidates = Vec::new();
    for (id, module_dir) in modules {
        let src = module_dir.join(defs::MODULE_PREINIT_DIR);
        if !src.is_dir() {
            continue;
        }
        let mut entries = Vec::new();
        let scanned = if src.join(defs::PREINIT_CONF).is_file() {
            scan(&src, Path::new(""), &mut entries)
        } else {
            Err(anyhow::anyhow!("{} not found", defs::PREINIT_CONF))
        };
        match scanned {
            Ok(()) => candidates.push((id.clone(), src, entries)),
            Err(e) => status.push(Status {
                id: id.clone(),
                state: STATE_ERROR,
                message: format!("{e:#}"),
            }),
        }
    }

    if candidates.is_empty() {
        remove_all(ksu_dir);
        write_status(&status);
        return Ok(());
    }

    let mut changed = false;
    let result = sync_candidates(ksu_dir, con, &candidates, &mut status, &mut changed);
    if let Err(e) = &result {
        // Whatever was not decided yet will not run as expected
        for (id, _, _) in &candidates {
            if !status.iter().any(|s| &s.id == id) {
                status.push(Status {
                    id: id.clone(),
                    state: STATE_ERROR,
                    message: format!("{e:#}"),
                });
            }
        }
    }

    // New content deserves a fresh set of attempts
    if changed {
        fs::remove_file(ksu_dir.join(defs::PREINIT_BOOTCOUNT_FILE)).ok();
        sync_dir(ksu_dir);
    }

    status.sort_by(|a, b| a.id.cmp(&b.id));
    write_status(&status);
    result
}

fn sync_candidates(
    ksu_dir: &Path,
    con: &str,
    candidates: &[(String, PathBuf, Vec<Entry>)],
    status: &mut Vec<Status>,
    changed: &mut bool,
) -> Result<()> {
    let preinit_dir = ksu_dir.join(defs::PREINIT_SUBDIR);

    // ksud runs with umask 0, tighten what was created by earlier versions
    secure(ksu_dir, 0o755, con)?;
    ensure_dir_exists(&preinit_dir)?;
    secure(&preinit_dir, 0o755, con)?;

    let manifest_path = preinit_dir.join(defs::PREINIT_MANIFEST_FILE);
    let old_manifest = read_manifest(&manifest_path);

    // Pick the modules that fit, counting every existing copy as free
    let free = available(ksu_dir)? + disk_usage(&preinit_dir);
    let budget = LIMIT_TOTAL.min(free.saturating_sub(RESERVE));
    // preinit/ itself and the manifest
    let mut used = 2 * BLOCK;
    let mut fit = Vec::new();
    for (id, src, entries) in candidates {
        let need = usage(entries);
        if used + need > budget {
            info!(
                "preinit: {id} needs {need} bytes, {} left",
                budget.saturating_sub(used)
            );
            status.push(Status {
                id: id.clone(),
                state: STATE_NO_SPACE,
                message: format!("need {need}, available {}", budget.saturating_sub(used)),
            });
            continue;
        }
        used += need;
        fit.push((id, src, entries, need));
    }

    // Free the space of modules that are gone, did not fit, or were interrupted before copying
    let wanted: BTreeSet<&str> = fit.iter().map(|(id, ..)| id.as_str()).collect();
    remove_unlisted(&preinit_dir, &wanted, changed)?;

    let mut keep = BTreeSet::new();
    let mut manifest = String::new();
    for (id, src, entries, need) in fit {
        let lines: Vec<String> = entries.iter().map(|e| e.manifest_line(id)).collect();
        let dst = preinit_dir.join(id);
        let unchanged = dst.is_dir()
            && (old_manifest.get(id.as_str()) == Some(&lines) || same_copy(&dst, src, entries));
        if !unchanged {
            // The budget counts the old copy as free, but install() writes the new one first:
            // drop the old copy when both would cut into the reserve.
            if available(ksu_dir)? < need + RESERVE && dst.exists() {
                remove_entry(&dst);
                *changed = true;
            }
            let avail = available(ksu_dir)?;
            if avail < need + RESERVE {
                status.push(Status {
                    id: id.clone(),
                    state: STATE_NO_SPACE,
                    message: format!("need {need}, available {}", avail.saturating_sub(RESERVE)),
                });
                continue;
            }
            if let Err(e) = install(&preinit_dir, id, src, entries, con) {
                warn!("preinit: install {id} failed: {e:#}");
                status.push(Status {
                    id: id.clone(),
                    state: STATE_ERROR,
                    message: format!("{e:#}"),
                });
                continue;
            }
            info!("preinit: synced {id}");
            *changed = true;
        }

        keep.insert(id.as_str());
        for line in lines {
            manifest.push_str(&line);
            manifest.push('\n');
        }
        status.push(Status {
            id: id.clone(),
            state: STATE_OK,
            message: String::new(),
        });
    }

    // Drop modules that failed to install
    remove_unlisted(&preinit_dir, &keep, changed)?;

    write_atomic(&manifest_path, &manifest)?;
    if let Err(e) = lsetfilecon(&manifest_path, con) {
        warn!("{e:#}");
    }
    sync_dir(&preinit_dir);
    Ok(())
}

/// Remove everything under `preinit_dir` except the manifest and the modules in `keep`.
fn remove_unlisted(preinit_dir: &Path, keep: &BTreeSet<&str>, changed: &mut bool) -> Result<()> {
    for entry in fs::read_dir(preinit_dir)?.flatten() {
        let name = entry.file_name();
        let name = name.to_string_lossy();
        if name == defs::PREINIT_MANIFEST_FILE || keep.contains(name.as_ref()) {
            continue;
        }
        if remove_entry(&entry.path()) && !name.starts_with('.') {
            info!("preinit: removed {name}");
            *changed = true;
        }
    }
    Ok(())
}

fn bootcount_path() -> PathBuf {
    Path::new(crate::module::preinit_ksu_dir()).join(defs::PREINIT_BOOTCOUNT_FILE)
}

fn read_bootcount() -> u64 {
    fs::read_to_string(bootcount_path())
        .ok()
        .and_then(|s| s.trim().parse().ok())
        .unwrap_or(0)
}

/// Preinit was skipped on this boot because the previous ones did not complete.
fn disabled_by_bootloop() -> bool {
    read_bootcount() > BOOTCOUNT_LIMIT
}

/// Boot completed: the preinit content that ran is fine. A parked counter is kept so that a
/// module which broke boot is not retried on every other boot.
pub fn on_boot_completed() {
    let count = read_bootcount();
    if count > BOOTCOUNT_LIMIT {
        warn!(
            "preinit was disabled after {BOOTCOUNT_LIMIT} failed boots, run `ksud module preinit reset` to retry"
        );
        return;
    }
    if count > 0 {
        fs::remove_file(bootcount_path()).ok();
    }
}

pub fn reset_bootcount() -> Result<()> {
    let path = bootcount_path();
    if path.exists() {
        fs::remove_file(&path).with_context(|| format!("Failed to remove {}", path.display()))?;
    }
    Ok(())
}

/// Per-module preinit state for the next boot: ok, no_space, error or bootloop.
pub fn module_states() -> HashMap<String, String> {
    let bootloop = disabled_by_bootloop();
    let Ok(content) = fs::read_to_string(defs::PREINIT_STATUS_PATH) else {
        return HashMap::new();
    };
    content
        .lines()
        .filter_map(|line| {
            let mut parts = line.splitn(3, ' ');
            let id = parts.next()?;
            let state = parts.next()?;
            let state = if bootloop && state == STATE_OK {
                STATE_BOOTLOOP
            } else {
                state
            };
            Some((id.to_owned(), state.to_owned()))
        })
        .collect()
}

pub fn status_of(id: &str) -> Option<(String, String)> {
    let content = fs::read_to_string(defs::PREINIT_STATUS_PATH).ok()?;
    content.lines().find_map(|line| {
        let mut parts = line.splitn(3, ' ');
        (parts.next()? == id).then(|| {
            (
                parts.next().unwrap_or_default().to_owned(),
                parts.next().unwrap_or_default().trim().to_owned(),
            )
        })
    })
}

pub fn print_status() {
    let count = read_bootcount();
    if count > BOOTCOUNT_LIMIT {
        println!(
            "preinit: disabled after {BOOTCOUNT_LIMIT} failed boots (reset with `ksud module preinit reset`)"
        );
    } else {
        println!("preinit: bootcount {count}/{BOOTCOUNT_LIMIT}");
    }
    let content = fs::read_to_string(defs::PREINIT_STATUS_PATH).unwrap_or_default();
    print!("{content}");
}
