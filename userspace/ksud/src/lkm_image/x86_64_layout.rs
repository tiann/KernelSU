// SPDX-License-Identifier: GPL-2.0-only
//!
//! Mapping between the decompressed x86-64 kernel image (an ELF64 with
//! `p_paddr`-based program headers) and the `_text`-relative addresses the
//! kernel symbol table uses.

use super::bytes::{read_u16, read_u32, read_u64};
use super::elf::EM_X86_64;
use anyhow::{Context, Result, ensure};

use super::image::ImageLayout;

// --- decompressed image layout --------------------------------------------

/// The x86-64 base page size, always exactly 4 KiB.
///
/// This is an architecture constant, not a property of the running system:
/// x86 can only map 4 KiB pages (plus 2 MiB/1 GiB huge pages), so an x86-64
/// kernel image is always linked, and its boot page tables always built, with
/// this granularity.  It is deliberately *not* the page size userspace sees:
/// arm64 kernels can be built with 16 KiB or 64 KiB pages, and the Android
/// x86-64 emulator passes `page_shift=14` (AOSP's `page_size_emulation`) so
/// that the addresses exposed to applications are 16 KiB aligned while the
/// kernel keeps running with 4 KiB pages.  Neither changes the boot stub's
/// page tables nor the `bzImage` layout this module and `build_bz_image`
/// reason about.
pub const X86_PAGE_SIZE: u64 = 4096;

#[derive(Clone, Copy, Debug)]
pub struct ProgramSegment {
    pub file_offset: u64,
    pub memory_offset: u64,
    pub file_size: u64,
    pub memory_size: u64,
}

#[derive(Debug)]
pub struct KernelImageLayout {
    pub segments: Vec<ProgramSegment>,
    pub kernel_total_size: u64,
}

impl ImageLayout for KernelImageLayout {
    fn content_offset_of(&self, address: u64, image_base: u64) -> Option<usize> {
        let memory_offset = address.checked_sub(image_base)?;
        usize::try_from(self.content_offset(memory_offset)?).ok()
    }

    fn address_of(&self, content_offset: usize, image_base: u64) -> Option<u64> {
        self.memory_offset(content_offset as u64)
            .and_then(|memory_offset| image_base.checked_add(memory_offset))
    }

    fn content_end(&self) -> usize {
        Self::content_end(self)
    }
}

impl KernelImageLayout {
    pub fn parse(content: &[u8]) -> Result<Self> {
        ensure!(
            content.len() >= 64
                && content.starts_with(b"\x7fELF")
                && content[4] == 2
                && content[5] == 1,
            "decompressed kernel is not a little-endian ELF64 image"
        );
        ensure!(
            read_u16(content, 16)? == 2,
            "decompressed kernel is not an executable ELF image"
        );
        ensure!(
            read_u16(content, 18)? == EM_X86_64,
            "decompressed kernel is not an x86-64 image"
        );
        let program_offset = usize::try_from(read_u64(content, 0x20)?)
            .context("ELF program header offset does not fit this host")?;
        let program_entry_size = usize::from(read_u16(content, 0x36)?);
        let program_count = usize::from(read_u16(content, 0x38)?);
        ensure!(
            program_entry_size >= 56 && program_count > 0,
            "malformed ELF program header table"
        );
        let table_end = program_offset
            .checked_add(program_entry_size.saturating_mul(program_count))
            .context("ELF program header table overflow")?;
        ensure!(
            table_end <= content.len(),
            "ELF program header table is outside the image"
        );

        let mut segments = Vec::new();
        for index in 0..program_count {
            let offset = program_offset + index * program_entry_size;
            if read_u32(content, offset)? != 1 {
                continue;
            }
            let file_offset = read_u64(content, offset + 8)?;
            let physical = read_u64(content, offset + 24)?;
            let file_size = read_u64(content, offset + 32)?;
            let memory_size = read_u64(content, offset + 40)?;
            ensure!(
                file_offset.saturating_add(file_size) <= content.len() as u64,
                "PT_LOAD segment {index} is outside the decompressed image"
            );
            ensure!(
                memory_size >= file_size,
                "PT_LOAD segment {index} has memsz < filesz"
            );
            segments.push(ProgramSegment {
                file_offset,
                memory_offset: physical,
                file_size,
                memory_size,
            });
        }
        ensure!(!segments.is_empty(), "kernel ELF has no PT_LOAD segment");

        // `parse_elf()` places each segment at `p_paddr - LOAD_PHYSICAL_ADDR`.
        let load_physical_addr = segments
            .iter()
            .map(|segment| segment.memory_offset)
            .min()
            .context("kernel ELF has no PT_LOAD segment")?;
        ensure!(
            load_physical_addr > 0 && load_physical_addr.is_multiple_of(1 << 20),
            "unexpected kernel load address 0x{load_physical_addr:x}"
        );
        for segment in &mut segments {
            segment.memory_offset -= load_physical_addr;
        }

        let image_end = segments
            .iter()
            .map(|segment| segment.memory_offset + segment.memory_size)
            .max()
            .context("kernel ELF has no PT_LOAD segment")?;
        // `_end` is emitted by the linker script aligned to the page size the
        // image was *built* with, so this has to round the same way to compare
        // against `_end - _text`; a kernel built for another page size fails
        // that check loudly instead of being mis-measured here.
        let kernel_total_size = image_end
            .checked_add(X86_PAGE_SIZE - 1)
            .map(|value| value & !(X86_PAGE_SIZE - 1))
            .context("kernel image size overflow")?;

        Ok(Self {
            segments,
            kernel_total_size,
        })
    }

    /// Memory offset of a decompressed content byte, when it is file backed.
    pub fn memory_offset(&self, content_offset: u64) -> Option<u64> {
        self.segments.iter().find_map(|segment| {
            (content_offset >= segment.file_offset
                && content_offset < segment.file_offset + segment.file_size)
                .then(|| content_offset - segment.file_offset + segment.memory_offset)
        })
    }

    /// Content offset of a linked kernel address.
    pub fn content_offset(&self, memory_offset: u64) -> Option<u64> {
        self.segments.iter().find_map(|segment| {
            (memory_offset >= segment.memory_offset
                && memory_offset < segment.memory_offset + segment.file_size)
                .then(|| memory_offset - segment.memory_offset + segment.file_offset)
        })
    }

    /// Content offset just past the last file-backed byte of the image.
    pub fn content_end(&self) -> usize {
        self.segments
            .iter()
            .map(|segment| segment.file_offset + segment.file_size)
            .max()
            .unwrap_or(0)
            .try_into()
            .unwrap_or(usize::MAX)
    }
}
