use anyhow::{Context, Result, bail};
use rustix::{
    cstr,
    fd::AsFd,
    system::finit_module,
};
use std::{
    collections::{HashMap, HashSet},
    fs,
    path::{Path, PathBuf},
};

/// Load the requested kernel modules in dependency order using finit_module.
///
/// `module_dir` should contain `.ko` files and a `modules.dep` file.
/// `requested_modules` entries follow the same path rules as `modules.dep`:
/// absolute paths are used as-is, and relative paths are resolved against `module_dir`.
pub fn load_modules_in_dependency_order(
    module_dir: &Path,
    requested_modules: &[impl AsRef<str>],
) -> Result<()> {
    let dep_graph = parse_modules_dep(module_dir)?;
    let mut ordered = Vec::new();
    let mut visiting = HashSet::new();
    let mut visited = HashSet::new();

    for module in requested_modules {
        let module_path = resolve_module_path(module_dir, module.as_ref());
        dfs_visit(
            &module_path,
            &dep_graph,
            &mut visiting,
            &mut visited,
            &mut ordered,
        )?;
    }

    for module in ordered {
        load_single_module(&module)?;
    }

    Ok(())
}

fn parse_modules_dep(module_dir: &Path) -> Result<HashMap<PathBuf, Vec<PathBuf>>> {
    let dep_file = module_dir.join("modules.dep");
    let content = fs::read_to_string(&dep_file)
        .with_context(|| format!("Failed to read {}", dep_file.display()))?;

    let mut graph = HashMap::new();
    for (line_no, line) in content.lines().enumerate() {
        let line = line.trim();
        if line.is_empty() {
            continue;
        }

        let Some((module, deps)) = line.split_once(':') else {
            bail!("Invalid modules.dep format at line {}: {}", line_no + 1, line);
        };

        let module_path = resolve_module_path(module_dir, module.trim());
        let dependencies = deps
            .split_whitespace()
            .map(|dep| resolve_module_path(module_dir, dep))
            .collect::<Vec<_>>();

        graph.insert(module_path, dependencies);
    }

    Ok(graph)
}

fn resolve_module_path(module_dir: &Path, module: &str) -> PathBuf {
    let path = Path::new(module);
    if path.is_absolute() {
        path.to_path_buf()
    } else {
        module_dir.join(path)
    }
}

fn dfs_visit(
    module: &Path,
    dep_graph: &HashMap<PathBuf, Vec<PathBuf>>,
    visiting: &mut HashSet<PathBuf>,
    visited: &mut HashSet<PathBuf>,
    ordered: &mut Vec<PathBuf>,
) -> Result<()> {
    if visited.contains(module) {
        return Ok(());
    }

    if !visiting.insert(module.to_path_buf()) {
        bail!("Cyclic module dependency detected at {}", module.display());
    }

    if let Some(deps) = dep_graph.get(module) {
        for dep in deps {
            dfs_visit(dep, dep_graph, visiting, visited, ordered)?;
        }
    }

    visiting.remove(module);
    visited.insert(module.to_path_buf());
    ordered.push(module.to_path_buf());
    Ok(())
}

fn load_single_module(module_path: &Path) -> Result<()> {
    let file = fs::File::open(module_path)
        .with_context(|| format!("Failed to open module {}", module_path.display()))?;

    // We intentionally pass empty parameters and default init flags for regular module loading.
    finit_module(file.as_fd(), cstr!(""), 0)
        .with_context(|| format!("finit_module failed for {}", module_path.display()))?;

    Ok(())
}
