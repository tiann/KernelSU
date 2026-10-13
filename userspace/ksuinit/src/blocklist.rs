//! Keep first stage init from loading the modules named in `block_modules=`.
//!
//! The kernel refuses a blacklisted module with EPERM, but first stage init
//! treats any module from `modules.load` that fails to load as fatal and
//! bootloops, unless the module is in `modules.blocklist`; modprobe skips those
//! (and every module depending on them) instead. ksuinit runs before first stage
//! init while the ramdisk is still writable, so it adds the names there, and the
//! kernel only installs its blacklist once second stage init starts.

use std::collections::{BTreeSet, HashMap};
use std::fs::{self, OpenOptions};
use std::io::{ErrorKind, Write};
use std::path::Path;

use anyhow::{Context, Result};

const MODULE_BASE_DIR: &str = "/lib/modules";

/// libmodprobe's MakeCanonical: file name without `.ko`, `-` replaced by `_`.
fn canonical_name(path: &str) -> String {
    let name = path.rsplit('/').next().unwrap_or(path);
    let name = name.strip_suffix(".ko").unwrap_or(name);
    name.replace('-', "_")
}

/// Same rule as ksud's `--block-module`, anything else is ignored.
fn is_valid_name(name: &str) -> bool {
    !name.is_empty()
        && name.len() <= 64
        && name.bytes().all(|c| c.is_ascii_alphanumeric() || c == b'_')
}

/// The module names of the `block_modules=` entry in the ksu_config params.
pub fn block_modules_from_params(params: &str) -> BTreeSet<String> {
    params
        .split_ascii_whitespace()
        .filter_map(|p| p.strip_prefix("block_modules="))
        .flat_map(|v| v.split(','))
        .filter(|n| is_valid_name(n))
        .map(str::to_owned)
        .collect()
}

/// `modules.dep`: module -> all modules it depends on (depmod lists them transitively).
fn parse_modules_dep(content: &str) -> HashMap<String, BTreeSet<String>> {
    content
        .lines()
        .filter_map(|line| line.split_once(':'))
        .map(|(module, deps)| {
            let deps = deps.split_ascii_whitespace().map(canonical_name).collect();
            (canonical_name(module.trim()), deps)
        })
        .collect()
}

fn parse_blocklist(content: &str) -> BTreeSet<String> {
    content
        .lines()
        .filter_map(|line| {
            let mut words = line.split_ascii_whitespace();
            match (words.next(), words.next()) {
                (Some("blocklist"), Some(name)) => Some(canonical_name(name)),
                _ => None,
            }
        })
        .collect()
}

#[derive(Debug, Default, PartialEq, Eq)]
pub struct Plan {
    /// Names to append to modules.blocklist.
    pub block: Vec<String>,
    /// Names left alone because other modules in this dir depend on them:
    /// modprobe would silently skip those too, possibly a driver the device needs.
    pub skipped: Vec<(String, Vec<String>)>,
}

/// Decide which of `names` to blocklist for one module dir. A name is only
/// blocked if it does not make modprobe skip any module that would load
/// otherwise, i.e. every module depending on it is blocked as well or already
/// skipped because of the existing blocklist.
pub fn plan(
    deps: &HashMap<String, BTreeSet<String>>,
    blocklist: &BTreeSet<String>,
    names: &BTreeSet<String>,
) -> Plan {
    // libmodprobe IsBlocklisted(): the module or any of its dependencies is blocklisted.
    let already_skipped = |module: &str, module_deps: &BTreeSet<String>| {
        blocklist.contains(module) || !module_deps.is_disjoint(blocklist)
    };
    let mut blocked: BTreeSet<&str> = names
        .iter()
        .filter(|n| deps.contains_key(n.as_str()))
        .map(String::as_str)
        .collect();
    let mut skipped = Vec::new();

    // Dropping a name can leave another one with an unblocked dependent, so repeat.
    loop {
        let unsafe_name = blocked.iter().find_map(|&name| {
            let dependents: Vec<String> = deps
                .iter()
                .filter(|(module, module_deps)| {
                    module.as_str() != name
                        && module_deps.contains(name)
                        && !blocked.contains(module.as_str())
                        && !already_skipped(module, module_deps)
                })
                .map(|(module, _)| module.clone())
                .collect();
            (!dependents.is_empty()).then_some((name, dependents))
        });
        let Some((name, mut dependents)) = unsafe_name else {
            break;
        };
        dependents.sort();
        blocked.remove(name);
        skipped.push((name.to_owned(), dependents));
    }

    Plan {
        block: blocked
            .into_iter()
            .filter(|n| !blocklist.contains(*n))
            .map(str::to_owned)
            .collect(),
        skipped,
    }
}

fn read_optional(path: &Path) -> Result<String> {
    match fs::read_to_string(path) {
        Ok(content) => Ok(content),
        Err(e) if e.kind() == ErrorKind::NotFound => Ok(String::new()),
        Err(e) => Err(e).with_context(|| format!("Cannot read {}", path.display())),
    }
}

fn apply_to_dir(dir: &Path, names: &BTreeSet<String>) -> Result<()> {
    let dep_path = dir.join("modules.dep");
    let deps = parse_modules_dep(&read_optional(&dep_path)?);
    if deps.is_empty() {
        return Ok(());
    }
    let blocklist_path = dir.join("modules.blocklist");
    let existing = read_optional(&blocklist_path)?;
    let plan = plan(&deps, &parse_blocklist(&existing), names);

    for (name, dependents) in &plan.skipped {
        log::warn!(
            "Not blocking module {name} in {}: needed by {}",
            dir.display(),
            dependents.join(", ")
        );
    }
    if plan.block.is_empty() {
        return Ok(());
    }

    let separator = if existing.is_empty() || existing.ends_with('\n') {
        ""
    } else {
        "\n"
    };
    let lines: String = std::iter::once(separator.to_owned())
        .chain(plan.block.iter().map(|name| format!("blocklist {name}\n")))
        .collect();
    OpenOptions::new()
        .create(true)
        .append(true)
        .open(&blocklist_path)
        .and_then(|mut f| f.write_all(lines.as_bytes()))
        .with_context(|| format!("Cannot write {}", blocklist_path.display()))?;
    log::info!(
        "Blocklisted modules in {}: {}",
        dir.display(),
        plan.block.join(", ")
    );
    Ok(())
}

/// Blocklist `names` in /lib/modules and each of its subdirectories; first
/// stage init picks one of them depending on the kernel release.
pub fn apply(names: &BTreeSet<String>) {
    if names.is_empty() {
        return;
    }
    let base = Path::new(MODULE_BASE_DIR);
    let mut dirs = vec![base.to_path_buf()];
    if let Ok(entries) = fs::read_dir(base) {
        dirs.extend(
            entries
                .flatten()
                .filter(|e| e.file_type().is_ok_and(|t| t.is_dir()))
                .map(|e| e.path()),
        );
    }
    for dir in dirs {
        if let Err(e) = apply_to_dir(&dir, names) {
            log::error!("Cannot blocklist modules in {}: {e:?}", dir.display());
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn names(list: &[&str]) -> BTreeSet<String> {
        list.iter().map(|s| (*s).to_owned()).collect()
    }

    const DEP: &str = "\
kernel/a.ko:
kernel/vr.ko:
kernel/mkp.ko:
kernel/display-drv.ko: kernel/mkp.ko kernel/a.ko
kernel/guard.ko: kernel/vr.ko
";

    #[test]
    fn params() {
        assert_eq!(
            block_modules_from_params("allow_shell=1 block_modules=vr,bad-name,,mkp norc=1"),
            names(&["mkp", "vr"])
        );
        assert!(block_modules_from_params("allow_shell=1").is_empty());
    }

    #[test]
    fn modules_dep_is_canonical() {
        let deps = parse_modules_dep(DEP);
        assert_eq!(deps["display_drv"], names(&["mkp", "a"]));
        assert!(deps["vr"].is_empty());
    }

    #[test]
    fn blocks_leaf_and_skips_needed() {
        let deps = parse_modules_dep(DEP);
        let plan = plan(
            &deps,
            &BTreeSet::new(),
            &names(&["mkp", "vr", "guard", "absent"]),
        );
        // guard depends on vr, but guard is blocked too; display_drv needs mkp.
        assert_eq!(plan.block, vec!["guard".to_owned(), "vr".to_owned()]);
        assert_eq!(
            plan.skipped,
            vec![("mkp".to_owned(), vec!["display_drv".to_owned()])]
        );
    }

    #[test]
    fn skipping_cascades() {
        let deps = parse_modules_dep(DEP);
        // vr is only safe while guard is blocked; guard is fine on its own.
        let plan = plan(&deps, &BTreeSet::new(), &names(&["vr"]));
        assert!(plan.block.is_empty());
        assert_eq!(
            plan.skipped,
            vec![("vr".to_owned(), vec!["guard".to_owned()])]
        );
    }

    #[test]
    fn existing_blocklist() {
        let deps = parse_modules_dep(DEP);
        let existing = parse_blocklist("blocklist guard\n# comment\nblocklist vr\n");
        assert_eq!(existing, names(&["guard", "vr"]));
        // Already blocklisted: nothing to add, and guard no longer counts as a dependent.
        let plan = plan(&deps, &existing, &names(&["vr"]));
        assert_eq!(plan, Plan::default());
    }

    #[test]
    fn writes_blocklist() {
        let dir = std::env::temp_dir().join(format!("ksuinit-blocklist-{}", std::process::id()));
        fs::create_dir_all(&dir).unwrap();
        fs::write(dir.join("modules.dep"), DEP).unwrap();
        fs::write(dir.join("modules.blocklist"), "blocklist a").unwrap();
        apply_to_dir(&dir, &names(&["vr", "guard", "mkp"])).unwrap();
        let content = fs::read_to_string(dir.join("modules.blocklist")).unwrap();
        fs::remove_dir_all(&dir).unwrap();
        // display_drv also needs the blocklisted a, so it is skipped anyway and mkp can go.
        assert_eq!(
            content,
            "blocklist a\nblocklist guard\nblocklist mkp\nblocklist vr\n"
        );
    }
}
