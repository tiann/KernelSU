// SPDX-License-Identifier: GPL-2.0-only
//!
//! Links the cross-arch `bootstrap.o` into the kernel image: section layout,
//! symbol resolution against the recovered kernel symbol map, and relocation.

use std::collections::BTreeMap;

#[cfg(test)]
use std::collections::BTreeSet;

use anyhow::{Context, Result, bail, ensure};

use super::Arch;
use super::bytes::{
    checked_align_up, get_slice, get_slice_mut, read_i64, read_u16, read_u32, read_u64, write_u32,
    write_u64,
};
use super::elf::{
    ELF64_RELA_SIZE, ELF64_SECTION_SIZE, ELF64_SYMBOL_SIZE, EM_X86_64, ET_REL, R_AARCH64_ABS32,
    R_AARCH64_ABS64, R_AARCH64_ADD_ABS_LO12_NC, R_AARCH64_ADR_PREL_PG_HI21, R_AARCH64_CALL26,
    R_AARCH64_JUMP26, R_X86_64_32, R_X86_64_32S, R_X86_64_64, R_X86_64_GOTPCRELX, R_X86_64_PC32,
    R_X86_64_PC64, R_X86_64_PLT32, R_X86_64_REX_GOTPCRELX, SHF_ALLOC, SHF_EXECINSTR, SHF_WRITE,
    SHN_ABS, SHN_UNDEF, SHT_NOBITS, SHT_PROGBITS, SHT_REL, SHT_RELA, SHT_STRTAB, SHT_SYMTAB,
    read_elf_string,
};

pub const BOOTSTRAP_OBJECT: &[u8] =
    include_bytes!(concat!(env!("OUT_DIR"), "/lkm_image_bootstrap.o"));
pub const BOOTSTRAP_X86_64_OBJECT: &[u8] =
    include_bytes!(concat!(env!("OUT_DIR"), "/lkm_image_bootstrap_x86_64.o"));

// Build-time embedded bootstrap and minimal runtime ET_REL linker.

#[derive(Clone, Debug)]
pub struct BootstrapSection {
    pub name: String,
    pub section_type: u32,
    pub flags: u64,
    pub offset: usize,
    pub size: usize,
    pub link: usize,
    pub info: usize,
    pub alignment: usize,
    pub entry_size: usize,
    pub output_offset: Option<usize>,
}

#[derive(Debug)]
pub struct BootstrapSymbol {
    pub name: String,
    pub value: u64,
    pub section_index: u16,
}

#[derive(Debug)]
pub struct BootstrapObject<'a> {
    data: &'a [u8],
    pub sections: Vec<BootstrapSection>,
    pub symbols: Vec<BootstrapSymbol>,
    symbol_table_index: usize,
    image_size: usize,
}

#[derive(Debug)]
pub struct LinkedBootstrap {
    pub data: Vec<u8>,
    pub entry_address: u64,
    pub reserve_wrapper_address: u64,
    pub strndup_adapter_address: u64,
}

impl<'a> BootstrapObject<'a> {
    pub fn parse(data: &'a [u8], machine: u16) -> Result<Self> {
        ensure!(
            data.len() >= 64 && data.starts_with(b"\x7fELF"),
            "embedded bootstrap is not an ELF file"
        );
        ensure!(
            data[4] == 2 && data[5] == 1 && data[6] == 1,
            "embedded bootstrap must be little-endian ELF64"
        );
        ensure!(
            read_u16(data, 16)? == ET_REL && read_u16(data, 18)? == machine,
            "embedded bootstrap must be a {} ET_REL object",
            if machine == EM_X86_64 {
                "x86-64"
            } else {
                "AArch64"
            }
        );
        ensure!(read_u32(data, 20)? == 1, "unsupported ELF version");
        ensure!(
            usize::from(read_u16(data, 52)?) >= 64 && read_u16(data, 56)? == 0,
            "malformed bootstrap ELF header"
        );

        let section_offset = usize::try_from(read_u64(data, 40)?)
            .context("bootstrap section offset does not fit this host")?;
        let section_entry_size = usize::from(read_u16(data, 58)?);
        let section_count = usize::from(read_u16(data, 60)?);
        let section_names_index = usize::from(read_u16(data, 62)?);
        ensure!(
            section_count > 0
                && section_entry_size == ELF64_SECTION_SIZE
                && section_names_index < section_count,
            "extended or malformed bootstrap section tables are unsupported"
        );
        let section_table_size = section_entry_size
            .checked_mul(section_count)
            .context("bootstrap section table size overflow")?;
        ensure!(
            section_offset
                .checked_add(section_table_size)
                .is_some_and(|end| end <= data.len()),
            "bootstrap section table is outside the object"
        );

        let mut sections = Vec::with_capacity(section_count);
        let mut name_offsets = Vec::with_capacity(section_count);
        for index in 0..section_count {
            let offset = section_offset + index * section_entry_size;
            let section_type = read_u32(data, offset + 4)?;
            let data_offset = usize::try_from(read_u64(data, offset + 24)?)
                .context("bootstrap section data offset does not fit this host")?;
            let size = usize::try_from(read_u64(data, offset + 32)?)
                .context("bootstrap section size does not fit this host")?;
            let alignment = usize::try_from(read_u64(data, offset + 48)?)
                .context("bootstrap section alignment does not fit this host")?
                .max(1);
            ensure!(
                alignment.is_power_of_two(),
                "bootstrap section {index} has invalid alignment {alignment}"
            );
            if section_type != SHT_NOBITS {
                ensure!(
                    data_offset
                        .checked_add(size)
                        .is_some_and(|end| end <= data.len()),
                    "bootstrap section {index} is outside the object"
                );
            }
            name_offsets.push(usize::try_from(read_u32(data, offset)?)?);
            sections.push(BootstrapSection {
                name: String::new(),
                section_type,
                flags: read_u64(data, offset + 8)?,
                offset: data_offset,
                size,
                link: usize::try_from(read_u32(data, offset + 40)?)?,
                info: usize::try_from(read_u32(data, offset + 44)?)?,
                alignment,
                entry_size: usize::try_from(read_u64(data, offset + 56)?)
                    .context("bootstrap section entry size does not fit this host")?,
                output_offset: None,
            });
        }

        let section_names = sections
            .get(section_names_index)
            .context("invalid bootstrap section-name table")?;
        ensure!(
            section_names.section_type == SHT_STRTAB,
            "bootstrap section-name table is not a string table"
        );
        let section_name_data = get_slice(data, section_names.offset, section_names.size)?;
        for (section, name_offset) in sections.iter_mut().zip(name_offsets) {
            read_elf_string(section_name_data, name_offset)?.clone_into(&mut section.name);
        }

        let find_unique_section = |name: &str| -> Result<usize> {
            let matches = sections
                .iter()
                .enumerate()
                .filter(|(_, section)| section.name == name)
                .map(|(index, _)| index)
                .collect::<Vec<_>>();
            ensure!(
                matches.len() == 1,
                "bootstrap must contain exactly one {name} section"
            );
            Ok(matches[0])
        };
        let text_index = find_unique_section(".text.ksu_bootstrap")?;
        let rodata_index = find_unique_section(".rodata.ksu_bootstrap")?;
        let text = &sections[text_index];
        let rodata = &sections[rodata_index];
        ensure!(
            text.section_type == SHT_PROGBITS
                && text.flags & (SHF_ALLOC | SHF_EXECINSTR) == (SHF_ALLOC | SHF_EXECINSTR)
                && text.flags & SHF_WRITE == 0,
            "bootstrap text section has unsafe flags or type"
        );
        ensure!(
            rodata.section_type == SHT_PROGBITS
                && rodata.flags & SHF_ALLOC != 0
                && rodata.flags & (SHF_WRITE | SHF_EXECINSTR) == 0,
            "bootstrap rodata section has unsafe flags or type"
        );
        for (index, section) in sections.iter().enumerate() {
            ensure!(
                section.size == 0
                    || section.flags & SHF_ALLOC == 0
                    || index == text_index
                    || index == rodata_index,
                "unsupported allocatable bootstrap section {}",
                section.name
            );
        }

        let rodata_output = checked_align_up(text.size, rodata.alignment)?;
        let image_size = rodata_output
            .checked_add(rodata.size)
            .context("bootstrap image size overflow")?;
        ensure!(image_size > 0, "bootstrap has no loadable bytes");
        sections[text_index].output_offset = Some(0);
        sections[rodata_index].output_offset = Some(rodata_output);

        let symbol_tables = sections
            .iter()
            .enumerate()
            .filter(|(_, section)| section.section_type == SHT_SYMTAB)
            .map(|(index, _)| index)
            .collect::<Vec<_>>();
        ensure!(
            symbol_tables.len() == 1,
            "bootstrap must contain exactly one symbol table"
        );
        let symbol_table_index = symbol_tables[0];
        let symbol_table = &sections[symbol_table_index];
        ensure!(
            symbol_table.entry_size == ELF64_SYMBOL_SIZE
                && symbol_table.size.is_multiple_of(ELF64_SYMBOL_SIZE),
            "malformed bootstrap symbol table"
        );
        let symbol_strings = sections
            .get(symbol_table.link)
            .context("bootstrap symbol table has an invalid string table")?;
        ensure!(
            symbol_strings.section_type == SHT_STRTAB,
            "bootstrap symbol names are not in a string table"
        );
        let symbol_string_data = get_slice(data, symbol_strings.offset, symbol_strings.size)?;
        let symbol_count = symbol_table.size / ELF64_SYMBOL_SIZE;
        ensure!(
            symbol_table.info <= symbol_count,
            "bootstrap symbol table has invalid local symbol count"
        );
        let mut symbols = Vec::with_capacity(symbol_count);
        for index in 0..symbol_count {
            let offset = symbol_table.offset + index * ELF64_SYMBOL_SIZE;
            let name = read_elf_string(
                symbol_string_data,
                usize::try_from(read_u32(data, offset)?)?,
            )?
            .to_owned();
            let section_index = read_u16(data, offset + 6)?;
            ensure!(
                section_index == SHN_UNDEF
                    || section_index == SHN_ABS
                    || usize::from(section_index) < sections.len(),
                "bootstrap symbol {name:?} uses an unsupported section index"
            );
            let value = read_u64(data, offset + 8)?;
            if section_index != SHN_UNDEF && section_index != SHN_ABS {
                ensure!(
                    value <= sections[usize::from(section_index)].size as u64,
                    "bootstrap symbol {name:?} is outside its section"
                );
            }
            symbols.push(BootstrapSymbol {
                name,
                value,
                section_index,
            });
        }

        let mut relocation_count = 0usize;
        ensure!(
            sections
                .iter()
                .all(|section| section.section_type != SHT_REL),
            "bootstrap uses unsupported REL relocations"
        );
        for section in sections
            .iter()
            .filter(|section| section.section_type == SHT_RELA)
        {
            ensure!(
                section.entry_size == ELF64_RELA_SIZE
                    && section.size.is_multiple_of(ELF64_RELA_SIZE)
                    && section.link == symbol_table_index,
                "malformed bootstrap relocation section {}",
                section.name
            );
            let target = sections
                .get(section.info)
                .context("bootstrap relocation section has an invalid target")?;
            ensure!(
                target.output_offset.is_some(),
                "bootstrap relocation section {} targets non-loadable section {}",
                section.name,
                target.name
            );
            relocation_count = relocation_count
                .checked_add(section.size / ELF64_RELA_SIZE)
                .context("bootstrap relocation count overflow")?;
        }
        ensure!(relocation_count > 0, "bootstrap has no relocations");

        Ok(Self {
            data,
            sections,
            symbols,
            symbol_table_index,
            image_size,
        })
    }

    pub const fn image_size(&self) -> usize {
        self.image_size
    }

    pub fn section_address(&self, section_index: usize, code_address: u64) -> Result<u64> {
        let section = self
            .sections
            .get(section_index)
            .context("bootstrap symbol has an invalid section")?;
        let output_offset = section.output_offset.with_context(|| {
            format!(
                "bootstrap symbol refers to non-loadable section {}",
                section.name
            )
        })?;
        code_address
            .checked_add(output_offset as u64)
            .context("bootstrap section address overflow")
    }

    pub fn symbol_value(
        &self,
        symbol_index: usize,
        code_address: u64,
        definitions: &BTreeMap<&str, u64>,
    ) -> Result<u64> {
        let symbol = self
            .symbols
            .get(symbol_index)
            .context("bootstrap relocation has an invalid symbol index")?;
        match symbol.section_index {
            SHN_UNDEF => definitions
                .get(symbol.name.as_str())
                .copied()
                .with_context(|| {
                    format!("bootstrap requires missing definition {:?}", symbol.name)
                }),
            SHN_ABS => Ok(symbol.value),
            section_index => self
                .section_address(usize::from(section_index), code_address)?
                .checked_add(symbol.value)
                .context("bootstrap symbol address overflow"),
        }
    }

    pub fn named_symbol_value(
        &self,
        name: &str,
        code_address: u64,
        definitions: &BTreeMap<&str, u64>,
    ) -> Result<u64> {
        let matches = self
            .symbols
            .iter()
            .enumerate()
            .filter(|(_, symbol)| symbol.name == name)
            .map(|(index, _)| index)
            .collect::<Vec<_>>();
        ensure!(
            matches.len() == 1,
            "bootstrap must define exactly one symbol {name}"
        );
        let symbol = &self.symbols[matches[0]];
        ensure!(
            symbol.section_index != SHN_UNDEF,
            "bootstrap symbol {name} is undefined"
        );
        self.symbol_value(matches[0], code_address, definitions)
    }

    pub fn link(
        &self,
        code_address: u64,
        definitions: &BTreeMap<&str, u64>,
        arch: Arch,
    ) -> Result<LinkedBootstrap> {
        for section in self
            .sections
            .iter()
            .filter(|section| section.output_offset.is_some())
        {
            let address = code_address
                .checked_add(section.output_offset.expect("checked above") as u64)
                .context("bootstrap section address overflow")?;
            ensure!(
                address.is_multiple_of(section.alignment as u64),
                "bootstrap section {} is not aligned at 0x{address:x}",
                section.name
            );
        }

        let mut output = vec![0u8; self.image_size];
        for section in self
            .sections
            .iter()
            .filter(|section| section.output_offset.is_some())
        {
            let output_offset = section.output_offset.expect("checked above");
            get_slice_mut(&mut output, output_offset, section.size)?.copy_from_slice(get_slice(
                self.data,
                section.offset,
                section.size,
            )?);
        }

        for relocation_section in self
            .sections
            .iter()
            .filter(|section| section.section_type == SHT_RELA)
        {
            ensure!(
                relocation_section.link == self.symbol_table_index,
                "bootstrap relocation uses an unexpected symbol table"
            );
            let target_section = &self.sections[relocation_section.info];
            let target_output = target_section
                .output_offset
                .context("bootstrap relocation target is not loadable")?;
            let target_address = self.section_address(relocation_section.info, code_address)?;
            for index in 0..relocation_section.size / ELF64_RELA_SIZE {
                let offset = relocation_section.offset + index * ELF64_RELA_SIZE;
                let target_offset = usize::try_from(read_u64(self.data, offset)?)
                    .context("bootstrap relocation offset does not fit this host")?;
                let info = read_u64(self.data, offset + 8)?;
                let relocation_type = info as u32;
                let symbol_index = usize::try_from(info >> 32)?;
                let addend = read_i64(self.data, offset + 16)?;
                let width = bootstrap_relocation_width(arch, relocation_type)?;
                ensure!(
                    target_offset
                        .checked_add(width)
                        .is_some_and(|end| end <= target_section.size),
                    "bootstrap relocation is outside section {}",
                    target_section.name
                );
                let output_offset = target_output
                    .checked_add(target_offset)
                    .context("bootstrap relocation output offset overflow")?;
                let place_address = target_address
                    .checked_add(target_offset as u64)
                    .context("bootstrap relocation address overflow")?;
                let symbol_value = self.symbol_value(symbol_index, code_address, definitions)?;
                let symbol_name = &self.symbols[symbol_index].name;
                apply_bootstrap_relocation(
                    &mut output,
                    output_offset,
                    place_address,
                    arch,
                    relocation_type,
                    symbol_value,
                    addend,
                )
                .with_context(|| {
                    format!(
                        "cannot relocate {}+0x{target_offset:x} against {symbol_name:?}",
                        target_section.name
                    )
                })?;
            }
        }

        Ok(LinkedBootstrap {
            entry_address: self.named_symbol_value("ksu_bootstrap", code_address, definitions)?,
            reserve_wrapper_address: self.named_symbol_value(
                "ksu_memblock_reserve_wrapper",
                code_address,
                definitions,
            )?,
            strndup_adapter_address: self.named_symbol_value(
                "ksu_strndup_user_adapter",
                code_address,
                definitions,
            )?,
            data: output,
        })
    }
}

pub fn bootstrap_relocation_width(arch: Arch, relocation_type: u32) -> Result<usize> {
    match (arch, relocation_type) {
        (Arch::Aarch64, R_AARCH64_ABS64) | (Arch::X86_64, R_X86_64_64 | R_X86_64_PC64) => Ok(8),
        (
            Arch::Aarch64,
            R_AARCH64_ABS32
            | R_AARCH64_CALL26
            | R_AARCH64_JUMP26
            | R_AARCH64_ADR_PREL_PG_HI21
            | R_AARCH64_ADD_ABS_LO12_NC,
        )
        | (
            Arch::X86_64,
            R_X86_64_PC32
            | R_X86_64_PLT32
            | R_X86_64_32
            | R_X86_64_32S
            | R_X86_64_GOTPCRELX
            | R_X86_64_REX_GOTPCRELX,
        ) => Ok(4),
        _ => bail!(
            "unsupported {} bootstrap relocation {relocation_type}",
            arch.label()
        ),
    }
}

pub fn apply_x86_64_bootstrap_relocation(
    output: &mut [u8],
    output_offset: usize,
    place_address: u64,
    relocation_type: u32,
    symbol_value: u64,
    addend: i64,
) -> Result<()> {
    let value = i128::from(symbol_value) + i128::from(addend);
    match relocation_type {
        R_X86_64_64 => {
            ensure!(
                output_offset.is_multiple_of(8),
                "unaligned R_X86_64_64 relocation"
            );
            write_u64(
                output,
                output_offset,
                u64::try_from(value).context("R_X86_64_64 relocation overflow")?,
            )
        }
        R_X86_64_PC32 | R_X86_64_PLT32 => {
            let delta = value - i128::from(place_address);
            let encoded = i32::try_from(delta).context("R_X86_64_PC32 relocation overflow")?;
            write_u32(output, output_offset, encoded as u32)
        }
        R_X86_64_32 => {
            // `R_X86_64_32` zero extends, so the full unsigned range is valid;
            // `R_X86_64_32S` sign extends and only allows the signed half.
            // Constants like 0x8000_0000 are reachable through `movl`.
            let encoded = u32::try_from(value).context("R_X86_64_32 relocation overflow")?;
            write_u32(output, output_offset, encoded)
        }
        R_X86_64_32S => {
            let encoded = i32::try_from(value).context("R_X86_64_32S relocation overflow")?;
            write_u32(output, output_offset, encoded as u32)
        }
        R_X86_64_PC64 => {
            let delta = value - i128::from(place_address);
            write_u64(
                output,
                output_offset,
                u64::try_from(delta).context("R_X86_64_PC64 relocation overflow")?,
            )
        }
        _ => bail!("unsupported x86-64 bootstrap relocation {relocation_type}"),
    }
}

pub fn apply_bootstrap_relocation(
    output: &mut [u8],
    output_offset: usize,
    place_address: u64,
    arch: Arch,
    relocation_type: u32,
    symbol_value: u64,
    addend: i64,
) -> Result<()> {
    if arch == Arch::X86_64 {
        return apply_x86_64_bootstrap_relocation(
            output,
            output_offset,
            place_address,
            relocation_type,
            symbol_value,
            addend,
        );
    }
    let value = i128::from(symbol_value) + i128::from(addend);
    match relocation_type {
        R_AARCH64_ABS64 => {
            ensure!(
                output_offset.is_multiple_of(8),
                "unaligned ABS64 relocation"
            );
            write_u64(
                output,
                output_offset,
                u64::try_from(value).context("ABS64 relocation overflow")?,
            )
        }
        R_AARCH64_ABS32 => {
            ensure!(
                output_offset.is_multiple_of(4),
                "unaligned ABS32 relocation"
            );
            write_u32(
                output,
                output_offset,
                u32::try_from(value).context("ABS32 relocation overflow")?,
            )
        }
        R_AARCH64_CALL26 | R_AARCH64_JUMP26 => {
            let delta = value - i128::from(place_address);
            ensure!(delta % 4 == 0, "branch target is not instruction-aligned");
            let immediate = delta / 4;
            ensure!(
                (-(1i128 << 25)..(1i128 << 25)).contains(&immediate),
                "branch target is outside the AArch64 26-bit range"
            );
            let instruction = read_u32(output, output_offset)?;
            let opcode = if relocation_type == R_AARCH64_CALL26 {
                0x9400_0000
            } else {
                0x1400_0000
            };
            ensure!(
                instruction & 0xfc00_0000 == opcode,
                "branch relocation does not target the expected instruction"
            );
            let encoded = u32::try_from((immediate as i64 as u64) & 0x03ff_ffff)?;
            write_u32(output, output_offset, opcode | encoded)
        }
        R_AARCH64_ADR_PREL_PG_HI21 => {
            let target = u64::try_from(value).context("ADRP relocation overflow")?;
            let page_delta =
                (i128::from(target & !0xfff) - i128::from(place_address & !0xfff)) / 4096;
            ensure!(
                (-(1i128 << 20)..(1i128 << 20)).contains(&page_delta),
                "ADRP target is outside the AArch64 21-bit page range"
            );
            let instruction = read_u32(output, output_offset)?;
            ensure!(
                instruction & 0x9f00_0000 == 0x9000_0000,
                "ADRP relocation does not target an ADRP instruction"
            );
            let immediate = (page_delta as i64 as u64) & 0x1f_ffff;
            let immlo = u32::try_from((immediate & 0x3) << 29)?;
            let immhi = u32::try_from(((immediate >> 2) & 0x7_ffff) << 5)?;
            write_u32(
                output,
                output_offset,
                (instruction & !0x60ff_ffe0) | immlo | immhi,
            )
        }
        R_AARCH64_ADD_ABS_LO12_NC => {
            let target = u64::try_from(value).context("ADD relocation overflow")?;
            let instruction = read_u32(output, output_offset)?;
            ensure!(
                instruction & 0x7f40_0000 == 0x1100_0000,
                "ADD relocation does not target an unshifted ADD-immediate instruction"
            );
            let immediate = u32::try_from((target & 0xfff) << 10)?;
            write_u32(
                output,
                output_offset,
                (instruction & !0x003f_fc00) | immediate,
            )
        }
        _ => bail!("unsupported AArch64 bootstrap relocation {relocation_type}"),
    }
}

/// The symbols an embedded bootstrap object expects the kernel image to
/// define.
#[cfg(test)]
pub fn undefined_symbols<'a>(object: &'a BootstrapObject<'a>) -> BTreeSet<&'a str> {
    object
        .symbols
        .iter()
        .filter(|symbol| symbol.section_index == SHN_UNDEF && !symbol.name.is_empty())
        .map(|symbol| symbol.name.as_str())
        .collect()
}

/// Both directions of the link contract: a definition the assembly never uses
/// is dead weight, and an undefined symbol without a definition used to fail
/// silently at runtime instead of loudly here.
#[cfg(test)]
pub fn assert_definition_contract(
    label: &str,
    object: &BootstrapObject<'_>,
    definitions: &BTreeMap<&str, u64>,
) {
    let undefined = undefined_symbols(object);
    let provided: BTreeSet<&str> = definitions.keys().copied().collect();
    let missing: Vec<&str> = undefined.difference(&provided).copied().collect();
    let unused: Vec<&str> = provided.difference(&undefined).copied().collect();
    assert!(
        missing.is_empty(),
        "{label}: the bootstrap object needs undefined symbols {missing:?}"
    );
    assert!(
        unused.is_empty(),
        "{label}: definitions {unused:?} are never referenced by the object"
    );
}
