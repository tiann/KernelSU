use std::cell::RefCell;
use std::collections::BTreeMap;
use std::path::PathBuf;
use std::rc::Rc;
use anyhow::{bail, ensure, Result};
#[cfg(any(target_os = "linux", target_os = "android"))]
use procfs::process::{MountInfo, Process};

#[cfg(any(target_os = "linux", target_os = "android"))]
pub type MountTree = Rc<MountNode>;

#[derive(Debug)]
#[cfg(any(target_os = "linux", target_os = "android"))]
pub struct MountNode {
    pub mount_info: MountInfo,
    pub children: RefCell<Vec<MountTree>>
}

#[cfg(any(target_os = "linux", target_os = "android"))]
impl MountNode {
    #[allow(dead_code)]
    #[cfg(any(target_os = "linux", target_os = "android"))]
    pub(crate) fn print(&self) {
        println!("{} {} {}", self.mount_info.mnt_id, self.mount_info.pid, self.mount_info.mount_point.to_str().expect("wtf"));
    }

    #[allow(dead_code)]
    #[cfg(any(target_os = "linux", target_os = "android"))]
    pub(crate) fn print_tree(&self, level: usize) {
        print!("{}", " ".repeat(level));
        self.print();
        for child in self.children.borrow().iter() {
            child.print_tree(level + 1);
        }
    }

    #[cfg(any(target_os = "linux", target_os = "android"))]
    pub(crate) fn get_tree() -> Result<MountTree> {
        let process = Process::myself()?;
        let mounts = process.mountinfo()?;
        let mut root: Option<MountTree> = None;
        let mount_map = mounts.into_iter()
            .map(|mount|  (mount.mnt_id, Rc::new(MountNode {
                mount_info: mount,
                children: RefCell::new(vec![])
            })))
            .collect::<BTreeMap<_, _>>();
        for (id, mount) in &mount_map {
            let mut is_root = *id == mount.mount_info.pid;
            match mount_map.get(&mount.mount_info.pid) {
                Some(parent) => {
                    parent.children.borrow_mut().push(mount.clone());
                }
                None => {
                    is_root = true;
                }
            }
            if is_root {
                match root {
                    Some(..) => bail!("multiple root mount found!"),
                    None => {
                        root = Some(mount.clone())
                    }
                }
            }
        }
        ensure!(root.is_some(), "no root mount found!");
        Ok(root.unwrap())
    }

    #[cfg(any(target_os = "linux", target_os = "android"))]
    pub(crate) fn get_mount_for_path(tree: &MountTree, path: &PathBuf) -> Option<MountTree> {
        if path.starts_with(&tree.mount_info.mount_point) {
            for sub_tree in tree.children.borrow().iter() {
                if let Some(t) = MountNode::get_mount_for_path(sub_tree, path) {
                    return Some(t);
                }
            }
            return Some(tree.clone());
        }
        None
    }

    #[cfg(any(target_os = "linux", target_os = "android"))]
    pub(crate) fn get_child_mounts_for_path(child_mounts: &mut Vec<MountTree>, tree: &MountTree, path: &PathBuf) {
        if let Some(mount) = MountNode::get_mount_for_path(tree, path) {
            for child in mount.children.borrow().iter() {
                let path_with_slash = format!("{}/", path.to_str().expect("wtf"));
                if child.mount_info.mount_point.starts_with(path_with_slash) {
                    if let Some(child_mount) = MountNode::get_mount_for_path(child, &child.mount_info.mount_point) {
                        child_mounts.push(child_mount);
                    }
                }
            }
        }
    }

    #[cfg(any(target_os = "linux", target_os = "android"))]
    pub(crate) fn get_top_mounts_under_path(child_mounts: &mut Vec<MountTree>, tree: &MountTree, path: &PathBuf) {
        let mut children: Vec<MountTree> = Vec::new();
        MountNode::get_child_mounts_for_path(&mut children, tree, path);
        for child in children {
            MountNode::get_top_mounts_under_path(child_mounts, &child, &child.mount_info.mount_point);
        }
        child_mounts.push(tree.clone());
    }
}
