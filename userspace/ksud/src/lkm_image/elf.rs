// SPDX-License-Identifier: GPL-2.0-only
//!
//! ELF64 constants, section/symbol access and module relocation fixups.

use std::collections::HashSet;

use anyhow::{Context, Result, ensure};

use super::bytes::{read_c_string, read_u16, read_u32, read_u64};
use super::symbols::SymbolMap;

pub const EM_AARCH64: u16 = 183;
pub const EM_X86_64: u16 = 62;
pub const R_X86_64_64: u32 = 1;
pub const R_X86_64_PC32: u32 = 2;
pub const R_X86_64_PLT32: u32 = 4;
pub const R_X86_64_32: u32 = 10;
pub const R_X86_64_32S: u32 = 11;
pub const R_X86_64_PC64: u32 = 24;
pub const R_X86_64_GOTPCRELX: u32 = 41;
pub const R_X86_64_REX_GOTPCRELX: u32 = 42;

pub const ET_REL: u16 = 1;
pub const SHT_PROGBITS: u32 = 1;
pub const SHT_SYMTAB: u32 = 2;
pub const SHT_STRTAB: u32 = 3;
pub const SHT_RELA: u32 = 4;
pub const SHT_NOBITS: u32 = 8;
pub const SHT_REL: u32 = 9;
pub const SHF_WRITE: u64 = 1;
pub const SHF_ALLOC: u64 = 2;
pub const SHF_EXECINSTR: u64 = 4;
pub const SHN_UNDEF: u16 = 0;
pub const SHN_ABS: u16 = 0xfff1;
pub const R_AARCH64_ABS64: u32 = 257;
pub const R_AARCH64_ABS32: u32 = 258;
pub const R_AARCH64_JUMP26: u32 = 282;
pub const R_AARCH64_CALL26: u32 = 283;
pub const R_AARCH64_ADR_PREL_PG_HI21: u32 = 275;
pub const R_AARCH64_ADD_ABS_LO12_NC: u32 = 277;
pub const ELF64_SECTION_SIZE: usize = 64;
pub const ELF64_SYMBOL_SIZE: usize = 24;
pub const ELF64_RELA_SIZE: usize = 24;

#[derive(Debug)]
pub struct Fixup {
    pub symbol_file_offset: usize,
    pub kernel_offset: u64,
}

// Module ELF fixups and capsule construction.

#[derive(Clone, Copy)]
pub struct ElfSection {
    section_type: u32,
    offset: usize,
    size: usize,
    link: usize,
    entry_size: usize,
}

pub fn collect_module_fixups(
    module: &[u8],
    symbols: &SymbolMap,
    image_base: u64,
    image_size: usize,
    machine: u16,
) -> Result<(Vec<Fixup>, Vec<String>)> {
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

    let section_offset = usize::try_from(read_u64(module, 40)?)
        .context("ELF section offset does not fit this host")?;
    let section_entry_size = usize::from(read_u16(module, 58)?);
    let section_count = usize::from(read_u16(module, 60)?);
    ensure!(
        section_count > 0 && section_entry_size >= 64,
        "extended or malformed ELF section tables are unsupported"
    );
    let section_end = section_offset
        .checked_add(
            section_entry_size
                .checked_mul(section_count)
                .context("ELF section table size overflow")?,
        )
        .context("ELF section table offset overflow")?;
    ensure!(
        section_offset <= module.len() && section_end <= module.len(),
        "ELF section table is outside the module"
    );

    let mut sections = Vec::with_capacity(section_count);
    for index in 0..section_count {
        let offset = section_offset + index * section_entry_size;
        let data_offset = usize::try_from(read_u64(module, offset + 24)?)
            .context("ELF section data offset does not fit this host")?;
        let size = usize::try_from(read_u64(module, offset + 32)?)
            .context("ELF section size does not fit this host")?;
        sections.push(ElfSection {
            section_type: read_u32(module, offset + 4)?,
            offset: data_offset,
            size,
            link: usize::try_from(read_u32(module, offset + 40)?)?,
            entry_size: usize::try_from(read_u64(module, offset + 56)?)
                .context("ELF section entry size does not fit this host")?,
        });
    }

    let mut fixups = Vec::new();
    let mut unresolved = HashSet::new();
    let mut seen_offsets = HashSet::new();
    for section in sections
        .iter()
        .filter(|section| section.section_type == SHT_SYMTAB)
    {
        ensure!(
            section.entry_size >= 24 && section.size % section.entry_size == 0,
            "malformed ELF symbol table"
        );
        let strings_section = sections
            .get(section.link)
            .context("ELF symbol table has an invalid string table")?;
        let strings_end = strings_section
            .offset
            .checked_add(strings_section.size)
            .context("ELF string table size overflow")?;
        ensure!(
            strings_end <= module.len(),
            "ELF string table is outside the module"
        );
        let strings = &module[strings_section.offset..strings_end];

        for index in 1..section.size / section.entry_size {
            let symbol_file_offset = section
                .offset
                .checked_add(index * section.entry_size)
                .context("ELF symbol offset overflow")?;
            ensure!(
                symbol_file_offset.saturating_add(24) <= module.len(),
                "ELF symbol is outside the module"
            );
            if read_u16(module, symbol_file_offset + 6)? != SHN_UNDEF {
                continue;
            }
            let name_offset = usize::try_from(read_u32(module, symbol_file_offset)?)?;
            let Some(name) = read_c_string(strings, name_offset).filter(|name| !name.is_empty())
            else {
                continue;
            };
            let Some(target) = symbols.resolve_module_symbol(&name) else {
                unresolved.insert(name);
                continue;
            };
            let Some(kernel_offset) = target.address.checked_sub(image_base) else {
                unresolved.insert(name);
                continue;
            };
            if kernel_offset >= image_size as u64 {
                unresolved.insert(name);
                continue;
            }
            ensure!(
                u32::try_from(symbol_file_offset).is_ok(),
                "module symbol offset does not fit the capsule fixup format"
            );
            if seen_offsets.insert(symbol_file_offset) {
                fixups.push(Fixup {
                    symbol_file_offset,
                    kernel_offset,
                });
            }
        }
    }

    ensure!(
        !fixups.is_empty() || !unresolved.is_empty(),
        "module has no undefined symbols; unexpected kernel module format"
    );
    let mut unresolved = unresolved.into_iter().collect::<Vec<_>>();
    unresolved.sort();
    Ok((fixups, unresolved))
}

pub fn pack_fixups(fixups: &[Fixup]) -> Result<Vec<u8>> {
    let mut output = Vec::with_capacity(fixups.len() * 16);
    for fixup in fixups {
        output.extend_from_slice(&u32::try_from(fixup.symbol_file_offset)?.to_le_bytes());
        output.extend_from_slice(&0u32.to_le_bytes());
        output.extend_from_slice(&fixup.kernel_offset.to_le_bytes());
    }
    Ok(output)
}

pub fn read_elf_string(data: &[u8], offset: usize) -> Result<&str> {
    let tail = data
        .get(offset..)
        .with_context(|| format!("ELF string offset 0x{offset:x} is outside its table"))?;
    let end = tail
        .iter()
        .position(|byte| *byte == 0)
        .context("unterminated ELF string")?;
    std::str::from_utf8(&tail[..end]).context("ELF string is not UTF-8")
}
