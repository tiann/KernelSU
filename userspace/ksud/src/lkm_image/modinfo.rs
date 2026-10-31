// SPDX-License-Identifier: GPL-2.0-only
//!
//! The module's `vermagic=` entry.
//!
//! The kernel compares it against its own `VERMAGIC_STRING` (`UTS_RELEASE` plus
//! the SMP/preempt/module-unload/modversions/arch flags) and refuses the module
//! on a mismatch.  `ksuinit` copes with that at run time: after a failing load
//! it reads the required value out of the kernel's
//! `version magic '...' should be '...'` message, rewrites `.modinfo` and
//! retries.  The injector can do the same offline, because it already recovered
//! the target kernel's release string -- but only when the user asks for it
//! (`--force-vermagic`), because a mismatch usually means the module was built
//! for a different KMI.

use anyhow::{Context, Result, bail, ensure};

use super::VermagicRewrite;
use super::bytes::{align_up, get_slice, get_slice_mut, read_u16, read_u32, read_u64, write_u64};
use super::elf::{ELF64_SECTION_SIZE, EM_X86_64, ET_REL, SHT_PROGBITS, read_elf_string};

const VERMAGIC_PREFIX: &str = "vermagic=";

/// The module the stub will carry: unchanged unless the user asked for an
/// override, in which case `vermagic` is rewritten to name `kernel_release`.
pub fn override_module_vermagic(
    module: &[u8],
    machine: u16,
    kernel_release: &str,
    override_vermagic: bool,
) -> Result<(Vec<u8>, Option<VermagicRewrite>)> {
    if !override_vermagic {
        return Ok((module.to_vec(), None));
    }
    let (module, previous, current) = force_module_vermagic(module, machine, kernel_release)?;
    Ok((module, Some(VermagicRewrite { previous, current })))
}

/// Where `.modinfo` lives, as the rewrite needs it.
struct ModinfoSection {
    /// Offset of the section header, for the `sh_offset`/`sh_size` rewrite.
    header_offset: usize,
    data_offset: usize,
    size: usize,
    alignment: usize,
}

/// The section table and the section name string table.
fn section_table(module: &[u8], machine: u16) -> Result<(usize, usize, usize, usize)> {
    ensure!(
        module.len() >= 64 && module.starts_with(b"\x7fELF"),
        "module is not an ELF file"
    );
    ensure!(
        module[4] == 2 && module[5] == 1,
        "module must be little-endian ELF64"
    );
    ensure!(
        read_u16(module, 16)? == ET_REL && read_u16(module, 18)? == machine,
        "module must be a {} ET_REL object",
        if machine == EM_X86_64 {
            "x86-64"
        } else {
            "ARM64"
        }
    );

    let offset = usize::try_from(read_u64(module, 40)?)
        .context("ELF section offset does not fit this host")?;
    let entry_size = usize::from(read_u16(module, 58)?);
    let count = usize::from(read_u16(module, 60)?);
    let names = usize::from(read_u16(module, 62)?);
    ensure!(
        count > 0 && entry_size >= ELF64_SECTION_SIZE,
        "extended or malformed ELF section tables are unsupported"
    );
    let end = offset
        .checked_add(
            entry_size
                .checked_mul(count)
                .context("ELF section table size overflow")?,
        )
        .context("ELF section table offset overflow")?;
    ensure!(
        end <= module.len(),
        "ELF section table is outside the module"
    );
    ensure!(
        names < count,
        "ELF section name table index is out of range"
    );
    Ok((offset, entry_size, count, names))
}

fn find_modinfo(module: &[u8], machine: u16) -> Result<ModinfoSection> {
    let (table_offset, entry_size, count, names_index) = section_table(module, machine)?;
    let names_header = table_offset + names_index * entry_size;
    let names_offset = usize::try_from(read_u64(module, names_header + 24)?)
        .context("ELF section name table offset does not fit this host")?;
    let names_size = usize::try_from(read_u64(module, names_header + 32)?)
        .context("ELF section name table size does not fit this host")?;
    let names = get_slice(module, names_offset, names_size)
        .context("ELF section name table is outside the module")?;

    for index in 0..count {
        let header_offset = table_offset + index * entry_size;
        let name_offset = usize::try_from(read_u32(module, header_offset)?)?;
        if read_elf_string(names, name_offset)? != ".modinfo" {
            continue;
        }
        ensure!(
            read_u32(module, header_offset + 4)? == SHT_PROGBITS,
            ".modinfo is not a PROGBITS section"
        );
        let data_offset = usize::try_from(read_u64(module, header_offset + 24)?)
            .context(".modinfo offset does not fit this host")?;
        let size = usize::try_from(read_u64(module, header_offset + 32)?)
            .context(".modinfo size does not fit this host")?;
        get_slice(module, data_offset, size).context(".modinfo is outside the module")?;
        let alignment = usize::try_from(read_u64(module, header_offset + 48)?)
            .unwrap_or(8)
            .max(1);
        ensure!(
            alignment.is_power_of_two(),
            ".modinfo alignment is not a power of two"
        );
        return Ok(ModinfoSection {
            header_offset,
            data_offset,
            size,
            alignment,
        });
    }
    bail!("module has no .modinfo section")
}

/// The `vermagic=` value of a module, when it has one.
pub fn module_vermagic(module: &[u8], machine: u16) -> Result<Option<String>> {
    let section = find_modinfo(module, machine)?;
    let entries = get_slice(module, section.data_offset, section.size)?;
    for entry in entries.split(|byte| *byte == 0) {
        if let Some(value) = entry.strip_prefix(VERMAGIC_PREFIX.as_bytes()) {
            return Ok(Some(String::from_utf8_lossy(value).into_owned()));
        }
    }
    Ok(None)
}

/// Rewrite the module's `vermagic` so the kernel's version check names
/// `kernel_release`.
///
/// Only the release part is swapped: the flags after it (`SMP`, `preempt`,
/// `mod_unload`, `modversions`, the architecture) describe the kernel
/// configuration and have to match anyway for a KMI compatible module, so they
/// are kept from the module itself.  Returns the rewritten module together with
/// the old and the new value.
pub fn force_module_vermagic(
    module: &[u8],
    machine: u16,
    kernel_release: &str,
) -> Result<(Vec<u8>, String, String)> {
    ensure!(
        !kernel_release.is_empty() && !kernel_release.contains(' '),
        "kernel release {kernel_release:?} is not a UTS_RELEASE string"
    );
    let Some(previous) = module_vermagic(module, machine)? else {
        bail!("module has no vermagic entry to rewrite");
    };
    // `6.12.76-4k SMP preempt mod_unload modversions aarch64` -> keep everything
    // from the first space on.
    let flags = previous.find(' ').map_or("", |space| &previous[space..]);
    let current = format!("{kernel_release}{flags}");
    let section = find_modinfo(module, machine)?;

    let entries = get_slice(module, section.data_offset, section.size)?;
    let mut rebuilt = Vec::with_capacity(section.size + current.len() + 1);
    for entry in entries.split(|byte| *byte == 0) {
        if entry.is_empty() || entry.starts_with(VERMAGIC_PREFIX.as_bytes()) {
            continue;
        }
        rebuilt.extend_from_slice(entry);
        rebuilt.push(0);
    }
    rebuilt.extend_from_slice(VERMAGIC_PREFIX.as_bytes());
    rebuilt.extend_from_slice(current.as_bytes());
    rebuilt.push(0);

    let mut output = module.to_vec();
    if rebuilt.len() <= section.size {
        // It fits where the old entries were: keep the section header untouched
        // and zero the tail so the entry list stays well formed.
        let end = section
            .data_offset
            .checked_add(rebuilt.len())
            .context(".modinfo rewrite overflow")?;
        get_slice_mut(&mut output, section.data_offset, rebuilt.len())?.copy_from_slice(&rebuilt);
        output[end..section.data_offset + section.size].fill(0);
    } else {
        // Append a fresh section and point the header at it, the way `ksuinit`
        // does after the kernel reported the required vermagic.
        let offset = align_up(output.len(), section.alignment);
        output.resize(offset, 0);
        output.extend_from_slice(&rebuilt);
        write_u64(
            &mut output,
            section.header_offset + 24,
            u64::try_from(offset)?,
        )?;
        write_u64(
            &mut output,
            section.header_offset + 32,
            u64::try_from(rebuilt.len())?,
        )?;
    }
    Ok((output, previous, current))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::lkm_image::bytes::{write_u32, write_u64};

    const MACHINE: u16 = super::EM_X86_64;
    const SHSTRTAB: &str = "\0.text\0.modinfo\0.shstrtab\0";

    fn put_u16(data: &mut [u8], offset: usize, value: u16) {
        data[offset..offset + 2].copy_from_slice(&value.to_le_bytes());
    }

    /// A minimal module ELF: the ELF header, a `.text` section, a `.modinfo`
    /// section of `section_size` bytes and the section name table.  That is all
    /// the rewrite walks.
    fn module_elf(entries: &[&str], section_size: usize) -> Vec<u8> {
        let mut modinfo = Vec::new();
        for entry in entries {
            modinfo.extend_from_slice(entry.as_bytes());
            modinfo.push(0);
        }
        assert!(modinfo.len() <= section_size);

        let text_offset = 64;
        let modinfo_offset = text_offset + 4;
        let shstrtab_offset = modinfo_offset + section_size;
        let table_offset = shstrtab_offset + SHSTRTAB.len();
        let mut data = vec![0u8; table_offset + 4 * ELF64_SECTION_SIZE];

        data[..4].copy_from_slice(b"\x7fELF");
        data[4] = 2;
        data[5] = 1;
        data[6] = 1;
        put_u16(&mut data, 16, ET_REL);
        put_u16(&mut data, 18, MACHINE);
        write_u32(&mut data, 20, 1).unwrap();
        write_u64(&mut data, 40, table_offset as u64).unwrap();
        put_u16(&mut data, 52, 64);
        put_u16(&mut data, 58, ELF64_SECTION_SIZE as u16);
        put_u16(&mut data, 60, 4);
        put_u16(&mut data, 62, 3);

        data[text_offset..text_offset + 4].copy_from_slice(&[0x90, 0x90, 0x90, 0xc3]);
        data[modinfo_offset..modinfo_offset + modinfo.len()].copy_from_slice(&modinfo);
        data[shstrtab_offset..shstrtab_offset + SHSTRTAB.len()]
            .copy_from_slice(SHSTRTAB.as_bytes());

        // 0: SHT_NULL, then .text, .modinfo and .shstrtab.
        let section =
            |data: &mut Vec<u8>, index: usize, name: u32, kind: u32, offset: usize, size: usize| {
                let header = table_offset + index * ELF64_SECTION_SIZE;
                write_u32(data, header, name).unwrap();
                write_u32(data, header + 4, kind).unwrap();
                write_u64(data, header + 24, offset as u64).unwrap();
                write_u64(data, header + 32, size as u64).unwrap();
                write_u64(data, header + 48, 1).unwrap();
            };
        section(&mut data, 1, 1, SHT_PROGBITS, text_offset, 4);
        section(&mut data, 2, 7, SHT_PROGBITS, modinfo_offset, section_size);
        section(&mut data, 3, 15, 3, shstrtab_offset, SHSTRTAB.len());
        data
    }

    #[test]
    fn reads_the_module_vermagic() {
        let module = module_elf(
            &["name=kernelsu", "vermagic=6.12.76-4k SMP preempt aarch64"],
            128,
        );
        assert_eq!(
            module_vermagic(&module, MACHINE).unwrap().as_deref(),
            Some("6.12.76-4k SMP preempt aarch64")
        );
    }

    /// The common case: the new entry fits where the old one was, so the module
    /// keeps its shape and only the entry list changes.
    #[test]
    fn rewrites_the_vermagic_in_place() {
        let module = module_elf(
            &[
                "name=kernelsu",
                "vermagic=6.12.76-4k SMP preempt aarch64",
                "srcversion=AB",
            ],
            256,
        );
        let (rewritten, previous, current) = force_module_vermagic(
            &module,
            MACHINE,
            "6.12.69-android16-6-g3986762b85e8-ab15416738",
        )
        .unwrap();

        assert_eq!(previous, "6.12.76-4k SMP preempt aarch64");
        assert_eq!(
            current,
            "6.12.69-android16-6-g3986762b85e8-ab15416738 SMP preempt aarch64"
        );
        assert_eq!(
            rewritten.len(),
            module.len(),
            "in place rewrite grows nothing"
        );
        assert_eq!(
            module_vermagic(&rewritten, MACHINE).unwrap().as_deref(),
            Some(current.as_str())
        );
        // The other entries survive, and the section header was not touched.
        let entries = get_slice(&rewritten, modinfo_offset(&module), modinfo_len(&module)).unwrap();
        assert!(entries.starts_with(b"name=kernelsu\0"));
        assert!(entries.windows(11).any(|w| w == b"srcversion="));
    }

    /// A longer kernel release does not fit into the old section: the rewrite
    /// appends a fresh one and points the header at it.
    #[test]
    fn appends_a_new_section_when_the_entry_does_not_fit() {
        // The old entry fits, the rewritten one (a longer release) does not.
        let module = module_elf(&["vermagic=6.12.76-4k SMP preempt aarch64"], 40);
        let (rewritten, _, current) =
            force_module_vermagic(&module, MACHINE, "6.12.69-android16-6-g3986762b85e8").unwrap();

        assert!(rewritten.len() > module.len());
        assert_eq!(
            module_vermagic(&rewritten, MACHINE).unwrap().as_deref(),
            Some(current.as_str())
        );
        // Everything up to the section table is untouched: the old `.modinfo`
        // bytes stay behind as dead weight, only the header points elsewhere.
        let table_offset = usize::try_from(read_u64(&module, 40).unwrap()).unwrap();
        assert_eq!(&rewritten[..table_offset], &module[..table_offset]);
    }

    #[test]
    fn rejects_a_module_without_a_vermagic() {
        let module = module_elf(&["name=kernelsu"], 64);
        assert!(force_module_vermagic(&module, MACHINE, "6.12.69").is_err());
        assert!(module_vermagic(&module, MACHINE).unwrap().is_none());
    }

    fn modinfo_offset(module: &[u8]) -> usize {
        find_modinfo(module, MACHINE).unwrap().data_offset
    }

    fn modinfo_len(module: &[u8]) -> usize {
        find_modinfo(module, MACHINE).unwrap().size
    }
}
