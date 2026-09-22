// SPDX-License-Identifier: GPL-2.0-only
//!
//! AArch64 (`Image`) LKM injection: kallsyms/BTF recovery, patch site
//! discovery, text tail cave selection and capsule placement.

#![allow(clippy::similar_names)]

use std::collections::{BTreeMap, BTreeSet, HashSet};

use anyhow::{Context, Result, bail, ensure};

use super::bootstrap::{BOOTSTRAP_OBJECT, BootstrapObject};
use super::bytes::{align_up, get_slice, get_slice_mut, read_u32, read_u64, write_u64};
use super::capsule::{build_capsule_at, capsule_wire_definitions};
use super::elf::{EM_AARCH64, collect_module_fixups, pack_fixups};
use super::image::{
    BranchCodec, IdentityLayout, find_unique_direct_call, is_branch, patch_site_scan_range,
    write_branch,
};
use super::modinfo::override_module_vermagic;
use super::restore::{
    ContainerFields, PatchedImage, RestoreCave, RestoreDetails, RestoreRecord, RestoreReport,
    RestoreSite, RestoredImage, Undone, open_injection, open_patched_image,
};
use super::symbols::{
    GkiAbi, MapSymbol, RecoveredKallsyms, RecoveredKernelMetadata, RequiredSymbolSpec,
    RequiredSymbols, SymbolMap, is_text_boundary_symbol, recover_gki_abi, recover_kernel_metadata,
    symbol_addresses,
};
use super::{Arch, BtfReport, CallSite, ImageInjectionReport, InjectionDetails};

/// Everything the arm64 bootstrap resolves from the kernel symbol map.
pub const REQUIRED_SYMBOLS: &[RequiredSymbolSpec] = &[
    RequiredSymbolSpec::new("_text"),
    RequiredSymbolSpec::new("_end").allow_end(),
    RequiredSymbolSpec::new("_stext"),
    RequiredSymbolSpec::new("_etext"),
    RequiredSymbolSpec::new("linux_banner"),
    RequiredSymbolSpec::new("arm64_memblock_init"),
    RequiredSymbolSpec::new("memblock_reserve"),
    RequiredSymbolSpec::new("memstart_addr"),
    RequiredSymbolSpec::new("kimage_voffset"),
    RequiredSymbolSpec::new("kernel_init"),
    RequiredSymbolSpec::new("async_synchronize_full"),
    RequiredSymbolSpec::new("load_module"),
    RequiredSymbolSpec::new("strndup_user"),
    RequiredSymbolSpec::new("vmalloc").or("vmalloc_noprof"),
    RequiredSymbolSpec::new("memcpy"),
    RequiredSymbolSpec::new("kstrdup"),
    // The bootstrap logs through the kernel's printk; `_printk` is the symbol
    // since 5.15 (older kernels keep a real `printk`).  Missing means the
    // injection still succeeds, only without its boot messages.
    RequiredSymbolSpec::new("_printk").or("printk").optional(),
];

// Image, capsule, ELF, and kallsyms wire constants.
pub const ARM64_IMAGE_MAGIC_OFFSET: usize = 0x38;
pub const ARM64_IMAGE_MAGIC: &[u8; 4] = b"ARM\x64";
pub const ARM64_IMAGE_SIZE_OFFSET: usize = 0x10;

/// `int3`-style padding the compiler leaves between functions; the arm64 `.text`
/// tail is zero filled.
pub const TEXT_PADDING_BYTE: u8 = 0;

pub const TEXT_CAVE_ALIGNMENT: usize = 16;
pub const TEXT_CAVE_PREFERRED_ALIGNMENT: usize = 4096;

#[derive(Clone, Copy, Debug)]
pub struct PatchSites {
    pub async_call: CallSite,
    pub strndup_call: CallSite,
    pub memblock_reserve_call: CallSite,
    pub page_offset: u64,
}

pub fn recover_arm64_kernel_metadata(image: &[u8]) -> Result<RecoveredKernelMetadata> {
    recover_kernel_metadata(image, parse_arm64_image_size(image)?, "ARM64 Image")
}

/// Whether `data` carries the ARM64 `Image` header, i.e. whether it is an
/// uncompressed arm64 kernel rather than an Android boot image.
pub fn is_arm64_image(data: &[u8]) -> bool {
    data.len() >= ARM64_IMAGE_MAGIC_OFFSET + ARM64_IMAGE_MAGIC.len()
        && &data[ARM64_IMAGE_MAGIC_OFFSET..ARM64_IMAGE_MAGIC_OFFSET + ARM64_IMAGE_MAGIC.len()]
            == ARM64_IMAGE_MAGIC
}

pub fn parse_arm64_image_size(image: &[u8]) -> Result<usize> {
    ensure!(
        image.len() >= 64,
        "kernel Image is smaller than the ARM64 header"
    );
    ensure!(
        is_arm64_image(image),
        "kernel input is not an uncompressed ARM64 Image"
    );
    let image_size = read_u64(image, ARM64_IMAGE_SIZE_OFFSET)?;
    ensure!(
        image_size != 0,
        "ARM64 Image header has image_size=0; unsupported input"
    );
    usize::try_from(image_size).context("ARM64 Image image_size does not fit this host")
}

pub fn decode_bl_target(instruction: u32, source_address: u64) -> Option<u64> {
    if instruction & 0xfc00_0000 != 0x9400_0000 {
        return None;
    }
    let immediate = i64::from((instruction << 6) as i32) >> 4;
    source_address.checked_add_signed(immediate)
}

pub fn encode_bl(source_address: u64, target_address: u64) -> Result<u32> {
    let displacement = i128::from(target_address) - i128::from(source_address);
    ensure!(
        displacement % 4 == 0,
        "ARM64 BL target is not 4-byte aligned"
    );
    let immediate = displacement >> 2;
    ensure!(
        (-(1i128 << 25)..(1i128 << 25)).contains(&immediate),
        "ARM64 BL from 0x{source_address:x} to 0x{target_address:x} is out of range"
    );
    Ok(0x9400_0000 | (u32::try_from(immediate & 0x03ff_ffff)?))
}

/// `BL` is a 26 bit signed word offset in a 4 byte instruction, so the scan
/// only ever looks at 4 byte aligned words.
pub struct BlBranch;

impl BranchCodec for BlBranch {
    const LENGTH: usize = 4;
    const ALIGNMENT: usize = 4;
    const NAME: &'static str = "BL";

    fn decode(content: &[u8], offset: usize, address: u64) -> Option<u64> {
        decode_bl_target(read_u32(content, offset).ok()?, address)
    }

    fn encode(source_address: u64, target_address: u64) -> Result<Vec<u8>> {
        Ok(encode_bl(source_address, target_address)?
            .to_le_bytes()
            .to_vec())
    }
}

pub fn decode_sub_shifted_register(instruction: u32) -> Option<(u32, u32, u32)> {
    (instruction & 0xffe0_fc00 == 0xcb00_0000).then_some((
        instruction & 31,
        (instruction >> 5) & 31,
        (instruction >> 16) & 31,
    ))
}

pub fn find_kernel_image_memblock_reserve_call(
    image: &[u8],
    symbols: &SymbolMap,
    image_base: u64,
    caller: &MapSymbol,
    accepted_targets: &HashSet<u64>,
    maximum_scan_size: usize,
) -> Result<CallSite> {
    let (start, end) = patch_site_scan_range::<IdentityLayout, BlBranch>(
        &IdentityLayout::new(image.len()),
        image,
        symbols,
        image_base,
        caller,
        maximum_scan_size,
    )?;
    let mut direct_calls = Vec::new();
    let mut semantic_matches = Vec::new();
    for offset in (start..end.saturating_sub(3)).step_by(4) {
        let instruction = read_u32(image, offset)?;
        let address = image_base + offset as u64;
        let Some(target) = decode_bl_target(instruction, address) else {
            continue;
        };
        if !accepted_targets.contains(&target) {
            continue;
        }
        let call = CallSite {
            file_offset: offset,
            address,
            target,
        };
        direct_calls.push(call);
        if offset < start + 8 {
            continue;
        }
        let Some((size_destination, _, kernel_start)) =
            decode_sub_shifted_register(read_u32(image, offset - 8)?)
        else {
            continue;
        };
        let Some((start_destination, start_left, _)) =
            decode_sub_shifted_register(read_u32(image, offset - 4)?)
        else {
            continue;
        };
        if size_destination == 1
            && start_destination == 0
            && kernel_start == start_left
            && kernel_start != 31
        {
            semantic_matches.push(call);
        }
    }

    let calls = direct_calls
        .iter()
        .map(|call| format!("0x{:x}", call.file_offset))
        .collect::<Vec<_>>()
        .join(", ");
    let matches = semantic_matches
        .iter()
        .map(|call| format!("0x{:x}", call.file_offset))
        .collect::<Vec<_>>()
        .join(", ");
    ensure!(
        semantic_matches.len() == 1,
        "cannot uniquely identify the kernel-image memblock_reserve() call from x0/x1 construction ({} matches){}{}",
        semantic_matches.len(),
        if calls.is_empty() {
            String::new()
        } else {
            format!("; direct calls: {calls}")
        },
        if matches.is_empty() {
            String::new()
        } else {
            format!("; semantic matches: {matches}")
        }
    );
    Ok(semantic_matches[0])
}

pub const fn rotate_right_width(value: u64, shift: u32, width: u32) -> u64 {
    let mask = if width == 64 {
        u64::MAX
    } else {
        (1u64 << width) - 1
    };
    let shift = shift % width;
    if shift == 0 {
        value & mask
    } else {
        ((value >> shift) | (value << (width - shift))) & mask
    }
}

pub fn decode_orr_immediate(instruction: u32) -> Option<(u64, u32, u32)> {
    if instruction >> 31 != 1 || instruction & 0x7f80_0000 != 0x3200_0000 {
        return None;
    }
    let n = (instruction >> 22) & 1;
    let immr = (instruction >> 16) & 0x3f;
    let imms = (instruction >> 10) & 0x3f;
    let length_source = (n << 6) | ((!imms) & 0x3f);
    if length_source == 0 {
        return None;
    }
    let length = length_source.ilog2();
    if length < 1 {
        return None;
    }
    let levels = (1u32 << length) - 1;
    let size = imms & levels;
    let rotate = immr & levels;
    if size == levels {
        return None;
    }
    let element_width = 1u32 << length;
    let element = rotate_right_width((1u64 << (size + 1)) - 1, rotate, element_width);
    let mut immediate = 0u64;
    for bit in (0..64).step_by(element_width as usize) {
        immediate |= element << bit;
    }
    Some((immediate, (instruction >> 5) & 31, instruction & 31))
}

pub fn infer_page_offset(
    image: &[u8],
    symbols: &SymbolMap,
    image_base: u64,
    function: &MapSymbol,
    maximum_scan_size: usize,
) -> Result<u64> {
    let (start, end) = patch_site_scan_range::<IdentityLayout, BlBranch>(
        &IdentityLayout::new(image.len()),
        image,
        symbols,
        image_base,
        function,
        maximum_scan_size,
    )?;
    let mut candidates = BTreeSet::<u64>::new();
    for offset in (start..end.saturating_sub(3)).step_by(4) {
        let Some((immediate, source, destination)) = decode_orr_immediate(read_u32(image, offset)?)
        else {
            continue;
        };
        let magnitude = 0u64.wrapping_sub(immediate);
        if source == destination
            && immediate >> 63 == 1
            && magnitude.is_power_of_two()
            && (36..=52).contains(&magnitude.trailing_zeros())
            && image_base & immediate == immediate
        {
            candidates.insert(immediate);
        }
    }
    ensure!(
        candidates.len() == 1,
        "cannot uniquely recover PAGE_OFFSET from {} ({} candidates){}",
        function.name,
        candidates.len(),
        if candidates.is_empty() {
            String::new()
        } else {
            format!(
                ": {}",
                candidates
                    .iter()
                    .map(|value| format!("0x{value:x}"))
                    .collect::<Vec<_>>()
                    .join(", ")
            )
        }
    );
    Ok(*candidates.iter().next().expect("one PAGE_OFFSET"))
}

pub fn analyze_patch_sites(
    image: &[u8],
    symbols: &SymbolMap,
    required: &RequiredSymbols,
) -> Result<PatchSites> {
    let image_base = required.image_base();
    let mut async_targets = symbol_addresses(
        symbols,
        &["async_synchronize_full", "async_synchronize_cookie_domain"],
    );
    async_targets.insert(required.get("async_synchronize_full").address);
    let mut strndup_targets = symbol_addresses(symbols, &["strndup_user"]);
    strndup_targets.insert(required.get("strndup_user").address);
    let mut memblock_targets = symbol_addresses(symbols, &["memblock_reserve"]);
    memblock_targets.insert(required.get("memblock_reserve").address);

    let async_call = find_unique_direct_call::<IdentityLayout, BlBranch>(
        &IdentityLayout::new(image.len()),
        image,
        symbols,
        image_base,
        required.get("kernel_init"),
        &async_targets,
        "kernel_init async call",
        0x4000,
    )?;
    let strndup_call = find_unique_direct_call::<IdentityLayout, BlBranch>(
        &IdentityLayout::new(image.len()),
        image,
        symbols,
        image_base,
        required.get("load_module"),
        &strndup_targets,
        "load_module strndup_user call",
        0x10000,
    )?;
    let memblock_reserve_call = find_kernel_image_memblock_reserve_call(
        image,
        symbols,
        image_base,
        required.get("arm64_memblock_init"),
        &memblock_targets,
        0x8000,
    )?;
    let page_offset = infer_page_offset(
        image,
        symbols,
        image_base,
        required.get("arm64_memblock_init"),
        0x8000,
    )?;
    ensure!(
        page_offset >= 1 << 63 && page_offset.is_multiple_of(4096),
        "recovered page_offset is not a page-aligned high-half u64"
    );

    Ok(PatchSites {
        async_call,
        strndup_call,
        memblock_reserve_call,
        page_offset,
    })
}

#[allow(clippy::too_many_arguments)]
pub fn find_text_tail_cave(
    image: &[u8],
    image_size: usize,
    symbols: &SymbolMap,
    image_base: u64,
    text_start: &MapSymbol,
    text_end: &MapSymbol,
    required_size: usize,
    padding_byte: u8,
) -> Result<(usize, usize)> {
    let start = text_start
        .address
        .checked_sub(image_base)
        .and_then(|value| usize::try_from(value).ok())
        .context("_stext is below the Image base")?;
    let end = text_end
        .address
        .checked_sub(image_base)
        .and_then(|value| usize::try_from(value).ok())
        .context("_etext is below the Image base")?;
    ensure!(
        end > start && end <= image_size && end <= image.len() && required_size > 0,
        "cannot establish a file-backed permanent-text range"
    );

    let mut zero_start = end;
    while zero_start > start && image[zero_start - 1] == padding_byte {
        zero_start -= 1;
    }
    let aligned_start = align_up(zero_start, TEXT_CAVE_ALIGNMENT);
    let preferred_start = align_up(zero_start, TEXT_CAVE_PREFERRED_ALIGNMENT);
    let mut starts = Vec::with_capacity(2);
    if preferred_start < end {
        starts.push(preferred_start);
    }
    if !starts.contains(&aligned_start) {
        starts.push(aligned_start);
    }

    let mut rejected_symbols = Vec::new();
    for candidate_start in starts {
        if candidate_start.saturating_add(required_size) > end {
            continue;
        }
        let occupants = symbols
            .entries
            .iter()
            .filter(|entry| {
                entry
                    .address
                    .checked_sub(image_base)
                    .and_then(|value| usize::try_from(value).ok())
                    .is_some_and(|offset| {
                        candidate_start <= offset
                            && offset < end
                            && !is_text_boundary_symbol(&entry.name)
                    })
            })
            .take(8)
            .collect::<Vec<_>>();
        if !occupants.is_empty() {
            rejected_symbols.extend(occupants.into_iter().map(|entry| entry.name.clone()));
            continue;
        }
        if image[candidate_start..end]
            .iter()
            .any(|byte| *byte != padding_byte)
        {
            continue;
        }
        return Ok((candidate_start, end - candidate_start));
    }

    rejected_symbols.sort();
    rejected_symbols.dedup();
    bail!(
        "cannot find 0x{required_size:x} bytes of proven 0x{padding_byte:02x} padding before {}{}",
        text_end.name,
        if rejected_symbols.is_empty() {
            String::new()
        } else {
            format!("; symbols in padding: {}", rejected_symbols.join(", "))
        }
    )
}

pub fn check_non_overlapping(ranges: &[(usize, usize, &str)]) -> Result<()> {
    for (index, &(start, end, label)) in ranges.iter().enumerate() {
        for &(other_start, other_end, other_label) in &ranges[index + 1..] {
            ensure!(
                start.max(other_start) >= end.min(other_end),
                "{label} overlaps {other_label}"
            );
        }
    }
    Ok(())
}

// Raw ARM64 Image injection.
pub fn inject_image(
    original_image: &[u8],
    module: &[u8],
    load_args: &str,
    override_vermagic: bool,
) -> Result<(Vec<u8>, ImageInjectionReport)> {
    let image = original_image.to_vec();
    // Patching an already patched image starts from the original bytes: the
    // record inside the old capsule says what those were, and it is carried
    // into the new one so repeated updates stay stable.
    let (mut image, replaced, original_container) = match undo_injection(&image)? {
        Some(undone) => (
            undone.content,
            Some(undone.replaced),
            Some(undone.original_container),
        ),
        None => (image, None, None),
    };
    let image_size = parse_arm64_image_size(&image)?;
    ensure!(
        image.len() <= image_size,
        "input has bytes beyond ARM64 image_size; appended DTB/metadata is unsupported"
    );

    let RecoveredKernelMetadata {
        kallsyms:
            RecoveredKallsyms {
                symbols,
                layout: kallsyms_layout,
                count: kallsyms_count,
            },
        btf: kernel_btf,
    } = recover_arm64_kernel_metadata(&image)?;
    let required = RequiredSymbols::resolve(REQUIRED_SYMBOLS, &symbols)?;
    required.validate_image_bounds(image_size)?;
    let image_base = required.image_base();
    let (kernel_release, gki_abi) = recover_gki_abi(
        &image,
        image_base,
        required.get("linux_banner"),
        kernel_btf.as_ref(),
    )?;
    let sites = analyze_patch_sites(&image, &symbols, &required)?;

    let (module, vermagic) =
        override_module_vermagic(module, EM_AARCH64, &kernel_release, override_vermagic)?;
    let (fixups, unresolved) =
        collect_module_fixups(&module, &symbols, image_base, image_size, EM_AARCH64)?;
    let fixup_bytes = pack_fixups(&fixups)?;
    let capsule_offset = align_up(image_size, 16);

    let bootstrap_object = BootstrapObject::parse(BOOTSTRAP_OBJECT, EM_AARCH64)
        .context("cannot parse the embedded LKM bootstrap")?;
    let (code_offset, code_cave_size) = find_text_tail_cave(
        &image,
        image_size,
        &symbols,
        image_base,
        required.get("_stext"),
        required.get("_etext"),
        bootstrap_object.image_size(),
        TEXT_PADDING_BYTE,
    )?;

    // What a restore has to put back: the image the patch starts from -- the
    // restored one when this is an update -- before the stub and the branches
    // are written into it.
    let mut original_content = image.clone();
    original_content.resize(image_size, 0);
    let mut patched_sites = Vec::with_capacity(3);
    for site in [
        sites.async_call,
        sites.strndup_call,
        sites.memblock_reserve_call,
    ] {
        patched_sites.push(RestoreSite::new(
            site.file_offset,
            get_slice(&original_content, site.file_offset, BlBranch::LENGTH)?,
        )?);
    }
    let restore_record = RestoreRecord::new(
        &original_content,
        original_container.unwrap_or(ContainerFields::Arm64 {
            image_size: u64::try_from(image_size)?,
        }),
        patched_sites,
        RestoreCave::new(code_offset, code_cave_size, TEXT_PADDING_BYTE)?,
        capsule_offset,
    )?;
    let restore_bytes = restore_record.encode()?;

    let capsule = build_capsule_at(
        capsule_offset,
        image_size,
        &module,
        &fixup_bytes,
        fixups.len(),
        load_args,
        Some(&restore_bytes),
    )?;
    let reserve_extension = capsule
        .image_size
        .checked_sub(image_size)
        .context("capsule did not extend the Image")?;

    let code_address = image_base
        .checked_add(code_offset as u64)
        .context("bootstrap address overflow")?;
    let printk_address = bootstrap_printk_address(&required, &bootstrap_object, code_address)?;
    let definitions = bootstrap_definitions(&BootstrapDefinitions {
        required: &required,
        sites: &sites,
        abi: &gki_abi,
        capsule_file_offset: capsule.file_offset,
        capsule_data_len: capsule.data.len(),
        capsule_module_offset: capsule.module_offset.saturating_sub(capsule.file_offset),
        capsule_fixup_offset: capsule.fixup_offset.saturating_sub(capsule.file_offset),
        capsule_load_args_offset: capsule.load_args_offset.saturating_sub(capsule.file_offset),
        reserve_extension,
        module_size: module.len(),
        fixup_count: fixups.len(),
        printk_address,
    });
    let bootstrap = bootstrap_object
        .link(code_address, &definitions, Arch::Aarch64)
        .context("cannot relocate the embedded LKM bootstrap")?;
    let bootstrap_address = bootstrap.entry_address;
    let adapter_address = bootstrap.strndup_adapter_address;
    let reserve_wrapper_address = bootstrap.reserve_wrapper_address;
    ensure!(
        bootstrap_address == code_address,
        "linked bootstrap address does not match the selected text cave"
    );
    ensure!(
        bootstrap.data.len() <= code_cave_size,
        "code cave needs 0x{:x} bytes, cave has 0x{code_cave_size:x}",
        bootstrap.data.len()
    );
    check_non_overlapping(&[
        (code_offset, code_offset + code_cave_size, "code cave"),
        (
            sites.async_call.file_offset,
            sites.async_call.file_offset + BlBranch::LENGTH,
            "kernel_init patch",
        ),
        (
            sites.strndup_call.file_offset,
            sites.strndup_call.file_offset + BlBranch::LENGTH,
            "load_module patch",
        ),
        (
            sites.memblock_reserve_call.file_offset,
            sites.memblock_reserve_call.file_offset + BlBranch::LENGTH,
            "memblock reserve patch",
        ),
    ])?;

    get_slice_mut(&mut image, code_offset, bootstrap.data.len())?.copy_from_slice(&bootstrap.data);
    write_branch::<BlBranch>(
        &mut image,
        sites.async_call.file_offset,
        sites.async_call.address,
        bootstrap_address,
    )?;
    write_branch::<BlBranch>(
        &mut image,
        sites.strndup_call.file_offset,
        sites.strndup_call.address,
        adapter_address,
    )?;
    write_branch::<BlBranch>(
        &mut image,
        sites.memblock_reserve_call.file_offset,
        sites.memblock_reserve_call.address,
        reserve_wrapper_address,
    )?;
    image.resize(image_size, 0);
    image.resize(capsule.file_offset, 0);
    image.extend_from_slice(&capsule.data);
    ensure!(
        image.len() == capsule.image_size,
        "internal error: capsule length does not match new image_size"
    );
    write_u64(
        &mut image,
        ARM64_IMAGE_SIZE_OFFSET,
        capsule.image_size as u64,
    )?;

    Ok((
        image,
        ImageInjectionReport {
            arch: Arch::Aarch64,
            kernel_release,
            replaced,
            btf: kernel_btf.map(|btf| BtfReport {
                file_offset: btf.file_offset,
                size: btf.size,
                type_count: btf.type_count,
            }),
            kallsyms_layout,
            kallsyms_count,
            bootstrap_offset: code_offset,
            bootstrap_size: bootstrap.data.len(),
            fixup_count: fixups.len(),
            unresolved,
            vermagic,
            details: InjectionDetails::Arm64 {
                load_info_structure_size: gki_abi.load_info_structure_size,
                load_info_storage_size: gki_abi.load_info_storage_size,
                load_info_hdr_offset: gki_abi.load_info_hdr_offset,
                load_info_len_offset: gki_abi.load_info_len_offset,
                memblock_reserve_call: sites.memblock_reserve_call.file_offset,
                page_offset: sites.page_offset,
                image_size: capsule.image_size,
            },
        },
    ))
}

/// Everything the arm64 link definitions are derived from.
pub struct BootstrapDefinitions<'a> {
    pub required: &'a RequiredSymbols,
    pub sites: &'a PatchSites,
    pub abi: &'a GkiAbi,
    /// Capsule offsets are relative to the image, the stub wants the offset
    /// from the capsule start.
    pub capsule_file_offset: usize,
    pub capsule_data_len: usize,
    pub capsule_module_offset: usize,
    pub capsule_fixup_offset: usize,
    /// Capsule relative offset of the NUL terminated `load_module()` args.
    pub capsule_load_args_offset: usize,
    pub reserve_extension: usize,
    pub module_size: usize,
    pub fixup_count: usize,
    /// Where `ksu_ext_printk` points: the kernel's printk, or the cave-local
    /// `ksu_no_printk` stub on a kernel that does not export one.  The stub
    /// branches to it directly, so this has to be an address in the same
    /// (link-time) image space as the other definitions.
    pub printk_address: u64,
}

/// Where the bootstrap's logger branches to.
///
/// The stub uses a direct, PC-relative branch, which stays valid under KASLR
/// only while the target is in the same image as the cave.  A kernel without
/// `printk` therefore gets the cave-local `ksu_no_printk` `ret` instead of a
/// zero address: the boot messages disappear, the injection still works.
pub fn bootstrap_printk_address(
    required: &RequiredSymbols,
    object: &BootstrapObject<'_>,
    code_address: u64,
) -> Result<u64> {
    required.optional("_printk").map_or_else(
        || object.named_symbol_value("ksu_no_printk", code_address, &BTreeMap::new()),
        |symbol| Ok(symbol.address),
    )
}

/// The definitions the embedded arm64 bootstrap object is linked against.
pub fn bootstrap_definitions(context: &BootstrapDefinitions<'_>) -> BTreeMap<&'static str, u64> {
    let BootstrapDefinitions {
        required,
        sites,
        abi,
        capsule_file_offset,
        capsule_data_len,
        capsule_module_offset,
        capsule_fixup_offset,
        capsule_load_args_offset,
        reserve_extension,
        module_size,
        fixup_count,
        printk_address,
    } = *context;
    let mut definitions = BTreeMap::from(capsule_wire_definitions());
    definitions.insert(
        "ksu_ext_memblock_reserve",
        sites.memblock_reserve_call.target,
    );
    definitions.insert(
        "ksu_ext_memstart_addr",
        required.get("memstart_addr").address,
    );
    definitions.insert(
        "ksu_ext_kimage_voffset",
        required.get("kimage_voffset").address,
    );
    definitions.insert("ksu_ext_async_synchronize_full", sites.async_call.target);
    definitions.insert("ksu_ext_vmalloc", required.get("vmalloc").address);
    definitions.insert("ksu_ext_memcpy", required.get("memcpy").address);
    definitions.insert("ksu_ext_load_module", required.get("load_module").address);
    definitions.insert("ksu_ext_kstrdup", required.get("kstrdup").address);
    definitions.insert("ksu_ext_strndup_user", sites.strndup_call.target);
    // Boot-time logging: the caller picked printk or the cave-local no-op.
    definitions.insert("ksu_ext_printk", printk_address);
    definitions.insert("ksu_image_base", required.image_base());
    definitions.insert("ksu_capsule_image_offset", capsule_file_offset as u64);
    definitions.insert("ksu_capsule_size", capsule_data_len as u64);
    definitions.insert("ksu_module_capsule_offset", capsule_module_offset as u64);
    definitions.insert("ksu_fixup_capsule_offset", capsule_fixup_offset as u64);
    definitions.insert(
        "ksu_load_args_capsule_offset",
        capsule_load_args_offset as u64,
    );
    definitions.insert("ksu_reserve_extension", reserve_extension as u64);
    definitions.insert("ksu_page_offset", sites.page_offset);
    definitions.insert("ksu_module_size", module_size as u64);
    definitions.insert("ksu_fixup_count", fixup_count as u64);
    definitions.insert("ksu_load_info_size", abi.load_info_storage_size);
    definitions.insert("ksu_load_info_hdr_offset", abi.load_info_hdr_offset);
    definitions.insert("ksu_load_info_len_offset", abi.load_info_len_offset);
    definitions.insert("ksu_gfp_kernel", abi.gfp_kernel);
    definitions
}

/// Undo a `boot-patch-v2` injection on an uncompressed arm64 `Image`.
///
/// The result is byte-identical to the image the injection started from: the
/// capsule (and the padding in front of it) is truncated away, the three
/// branches and the text tail cave go back to their original bytes, and the
/// header's `image_size` is written back.
pub fn restore_image(original_image: &[u8]) -> Result<RestoredImage> {
    let patched = open_patched_image(original_image, EM_AARCH64, "the arm64 Image")?;
    let image = undo_opened(original_image, &patched)?;
    let ContainerFields::Arm64 { image_size } = patched.record.container else {
        bail!("restore record is not an arm64 record");
    };
    Ok(RestoredImage {
        image,
        report: RestoreReport {
            arch: Arch::Aarch64,
            capsule_offset: patched.capsule_offset,
            capsule_size: patched.capsule_size,
            patched_sites: patched.record.sites.len(),
            original_content_len: patched.record.original_content_len,
            details: RestoreDetails::Arm64 { image_size },
        },
    })
}

/// Undo the injection the image carries, so that patching always starts from
/// the original bytes.  `None` means the image was never patched.
fn undo_injection(image: &[u8]) -> Result<Option<Undone>> {
    match open_injection(image, EM_AARCH64)? {
        Some(patched) => {
            let content = undo_opened(image, &patched)?;
            Ok(Some(Undone::new(content, &patched)))
        }
        None => Ok(None),
    }
}

/// The original bytes behind an already located capsule, verified against the
/// record before it is returned.
fn undo_opened(image: &[u8], patched: &PatchedImage) -> Result<Vec<u8>> {
    let record = &patched.record;
    let original_len = usize::try_from(record.original_content_len)?;
    ensure!(
        patched.capsule_offset >= original_len,
        "the capsule starts inside the original image"
    );
    ensure!(
        original_len <= image.len(),
        "the recorded image is 0x{original_len:x} bytes, the input only has 0x{:x}",
        image.len()
    );

    let mut content = image[..original_len].to_vec();
    for site in &record.sites {
        let range = site.range()?;
        // The branch has to still point somewhere, i.e. the injection has to
        // still be in place.  Restoring an already restored image would write
        // the same bytes back, but it should be reported, not silently redone.
        ensure!(
            is_branch::<BlBranch>(&content, range.start),
            "site 0x{:x} does not hold a BL; the image is not patched with v2",
            site.offset
        );
    }
    record.apply(&mut content)?;

    let ContainerFields::Arm64 { image_size } = record.container else {
        bail!("restore record is not an arm64 record");
    };
    ensure!(
        image_size == record.original_content_len,
        "restore record disagrees about the original image size"
    );
    write_u64(&mut content, ARM64_IMAGE_SIZE_OFFSET, image_size)?;
    record.verify(&content)?;
    Ok(content)
}

// boot-patch-v2 orchestration.
// boot-patch-v2 orchestration.
// Focused format and analysis tests.

#[cfg(test)]
pub mod tests {
    use super::*;

    use crate::lkm_image::bootstrap::assert_definition_contract;
    use crate::lkm_image::btf::{KernelBtf, LoadInfoLayout, find_btf_candidates};
    use crate::lkm_image::bytes::write_u32;
    use crate::lkm_image::capsule::{CAPSULE_ALIGNMENT, CAPSULE_MAGIC};
    use crate::lkm_image::elf::{SHN_UNDEF, SHT_RELA};
    use crate::lkm_image::symbols::GKI_ABI;
    use crate::lkm_image::symbols::{
        KALLSYMS_ALIGNMENT, KALLSYMS_TOKEN_INDEX_SIZE, validate_kallsyms_btf_boundaries,
    };
    use crate::{assets, boot_patch};

    fn bootstrap_test_definitions<'a>(
        object: &'a BootstrapObject<'a>,
        image_base: u64,
    ) -> BTreeMap<&'a str, u64> {
        let mut definitions = BTreeMap::new();
        for (index, symbol) in object
            .symbols
            .iter()
            .filter(|symbol| symbol.section_index == SHN_UNDEF && !symbol.name.is_empty())
            .enumerate()
        {
            let value = if symbol.name.starts_with("ksu_ext_") || symbol.name == "ksu_image_base" {
                image_base + 0x10_000 + index as u64 * 0x100
            } else {
                index as u64 + 1
            };
            definitions.insert(symbol.name.as_str(), value);
        }
        definitions
    }

    fn append_aligned(image: &mut [u8], position: &mut usize, data: &[u8]) -> usize {
        *position = align_up(*position, KALLSYMS_ALIGNMENT);
        let offset = *position;
        image[offset..offset + data.len()].copy_from_slice(data);
        *position += data.len();
        offset
    }

    fn build_test_btf(types: &[u8], strings: &[u8]) -> Vec<u8> {
        assert!(!strings.is_empty());
        let mut output = Vec::new();
        output.extend_from_slice(&0xeb9fu16.to_le_bytes());
        output.extend_from_slice(&[1, 0]);
        for value in [
            24u32,
            0,
            u32::try_from(types.len()).unwrap(),
            u32::try_from(types.len()).unwrap(),
            u32::try_from(strings.len()).unwrap(),
        ] {
            output.extend_from_slice(&value.to_le_bytes());
        }
        output.extend_from_slice(types);
        output.extend_from_slice(strings);
        output
    }

    fn minimal_test_btf() -> Vec<u8> {
        build_test_btf(&[], &[0])
    }

    fn unusable_load_info_test_btf() -> Vec<u8> {
        let mut types = Vec::new();
        types.extend_from_slice(&1u32.to_le_bytes());
        types.extend_from_slice(&(4u32 << 24).to_le_bytes());
        types.extend_from_slice(&8u32.to_le_bytes());
        build_test_btf(&types, b"\0load_info\0")
    }

    fn build_kallsyms_fixture_with_btf(
        layout: &str,
        btf_range: Option<(usize, usize)>,
    ) -> (Vec<u8>, u64) {
        let base = 0xffff_ffc0_0800_0000;
        let image_size = 0x20_0000usize;
        let count = 2049usize;
        let mut entries = vec![
            (base, b'T', "_text".to_owned()),
            (base + 0x1000, b't', "load_module".to_owned()),
        ];
        let boundary_symbol_count = usize::from(btf_range.is_some()) * 2;
        for index in 0..count - 3 - boundary_symbol_count {
            entries.push((
                base + 0x2000 + index as u64 * 0x20,
                b't',
                format!("fixture_symbol_{index:04}"),
            ));
        }
        if let Some((btf_offset, btf_size)) = btf_range {
            let btf_end = btf_offset.checked_add(btf_size).unwrap();
            assert!(btf_end < image_size);
            assert!(base + btf_offset as u64 > entries.last().unwrap().0);
            entries.push((base + btf_offset as u64, b'R', "__start_BTF".to_owned()));
            entries.push((base + btf_end as u64, b'R', "__stop_BTF".to_owned()));
        }
        entries.push((base + image_size as u64, b'B', "_end".to_owned()));

        let mut token_table = Vec::new();
        let mut token_offsets = Vec::with_capacity(256);
        for value in 0u8..=u8::MAX {
            token_offsets.push(u16::try_from(token_table.len()).unwrap());
            token_table.push(if (0x20..=0x7e).contains(&value) {
                value
            } else {
                b'x'
            });
            token_table.push(0);
        }
        let mut token_index = Vec::with_capacity(KALLSYMS_TOKEN_INDEX_SIZE);
        for offset in token_offsets {
            token_index.extend_from_slice(&offset.to_le_bytes());
        }

        let mut names = Vec::new();
        let mut markers = Vec::new();
        for (index, (_, kind, name)) in entries.iter().enumerate() {
            if index.is_multiple_of(256) {
                markers.push(u32::try_from(names.len()).unwrap());
            }
            let mut expanded = Vec::with_capacity(name.len() + 1);
            expanded.push(*kind);
            expanded.extend_from_slice(name.as_bytes());
            names.push(u8::try_from(expanded.len()).unwrap());
            names.extend_from_slice(&expanded);
        }
        let mut marker_bytes = Vec::new();
        for marker in markers {
            marker_bytes.extend_from_slice(&marker.to_le_bytes());
        }
        let mut offsets = Vec::new();
        for (address, _, _) in &entries {
            offsets.extend_from_slice(&u32::try_from(address - base).unwrap().to_le_bytes());
        }
        let sequences = (0..count * 3)
            .map(|index| u8::try_from(index % 251 + 1).unwrap())
            .collect::<Vec<_>>();

        let mut image = vec![0u8; image_size];
        write_u64(&mut image, ARM64_IMAGE_SIZE_OFFSET, image_size as u64).unwrap();
        image[ARM64_IMAGE_MAGIC_OFFSET..ARM64_IMAGE_MAGIC_OFFSET + 4]
            .copy_from_slice(ARM64_IMAGE_MAGIC);
        let mut position = 0x10000;
        match layout {
            "pre-6.4" => {
                append_aligned(&mut image, &mut position, &offsets);
                append_aligned(&mut image, &mut position, &base.to_le_bytes());
                append_aligned(
                    &mut image,
                    &mut position,
                    &u32::try_from(count).unwrap().to_le_bytes(),
                );
                append_aligned(&mut image, &mut position, &names);
                append_aligned(&mut image, &mut position, &marker_bytes);
                append_aligned(&mut image, &mut position, &sequences);
                append_aligned(&mut image, &mut position, &token_table);
                append_aligned(&mut image, &mut position, &token_index);
            }
            "6.4+" => {
                append_aligned(
                    &mut image,
                    &mut position,
                    &u32::try_from(count).unwrap().to_le_bytes(),
                );
                append_aligned(&mut image, &mut position, &names);
                append_aligned(&mut image, &mut position, &marker_bytes);
                append_aligned(&mut image, &mut position, &token_table);
                append_aligned(&mut image, &mut position, &token_index);
                append_aligned(&mut image, &mut position, &offsets);
                append_aligned(&mut image, &mut position, &base.to_le_bytes());
                append_aligned(&mut image, &mut position, &sequences);
            }
            _ => panic!("unknown fixture layout"),
        }
        (image, base)
    }

    fn build_kallsyms_fixture(layout: &str) -> (Vec<u8>, u64) {
        build_kallsyms_fixture_with_btf(layout, None)
    }

    #[test]
    fn arm64_image_size_uses_header_offset_0x10() {
        let mut image = vec![0u8; 64];
        image[ARM64_IMAGE_MAGIC_OFFSET..ARM64_IMAGE_MAGIC_OFFSET + 4]
            .copy_from_slice(ARM64_IMAGE_MAGIC);
        write_u64(&mut image, ARM64_IMAGE_SIZE_OFFSET, 0x20_0000).unwrap();
        write_u64(&mut image, 0x18, 0xa).unwrap();
        assert_eq!(parse_arm64_image_size(&image).unwrap(), 0x20_0000);
    }

    #[test]
    fn bl_round_trip_in_both_directions() {
        for (source, target) in [(0x1000, 0x2000), (0x4000, 0x1000)] {
            let instruction = encode_bl(source, target).unwrap();
            assert_eq!(decode_bl_target(instruction, source), Some(target));
        }
        assert!(encode_bl(0, 1 << 27).is_err());
    }

    #[test]
    fn page_offset_logical_immediate_decode() {
        assert_eq!(
            decode_orr_immediate(0xb259_6129),
            Some((0xffff_ff80_0000_0000, 9, 9))
        );
    }

    #[test]
    fn bundled_module_uses_release_asset_layout() {
        let name = crate::lkm_image::bundled_module_name(Arch::Aarch64, "android12-5.10");
        #[cfg(target_os = "android")]
        assert_eq!(name, "android12-5.10_kernelsu.ko");
        #[cfg(not(target_os = "android"))]
        assert_eq!(name, "aarch64/android12-5.10_kernelsu.ko");
        // The asset itself only exists in a release build; the naming
        // convention is what this test is about.
        if let Ok(asset) = assets::get_asset_data(&name) {
            assert!(!asset.is_empty());
        }
    }

    #[test]
    fn detects_kmi_from_boot_kernel_banner() {
        let kernel = b"Linux version 6.1.157-android14-11-gki-test\0";
        assert_eq!(boot_patch::parse_kmi(kernel).unwrap(), "android14-6.1");
    }

    /// `KSU_LKM_TEST_IMAGE` is a boot image or a raw kernel; the test only runs
    /// when the kernel inside it is an arm64 `Image`.
    fn test_fixture() -> Option<(Vec<u8>, Vec<u8>)> {
        let image = crate::lkm_image::test_kernel_from(&std::env::var("KSU_LKM_TEST_IMAGE").ok()?)?;
        if parse_arm64_image_size(&image).is_err() {
            return None;
        }
        let module = std::fs::read(std::env::var("KSU_LKM_TEST_MODULE").ok()?).ok()?;
        Some((image, module))
    }

    /// Full round trip against a real arm64 `Image`, e.g. the one inside a GKI
    /// boot image (`gki-boot_16k-android16-6.12-*-lz4.zip`).  Set
    /// `KSU_LKM_TEST_IMAGE` and `KSU_LKM_TEST_MODULE` to enable it;
    /// `KSU_LKM_TEST_OUT` additionally writes the patched image, which is what
    /// a boot test consumes.
    #[test]
    fn patch_and_restore_a_real_image() {
        let Some((original, module)) = test_fixture() else {
            return;
        };
        let image_size = parse_arm64_image_size(&original).unwrap();
        // A kernel `Image` may be shorter than the `image_size` its header
        // declares: the tail is zero padding the injection works on, so the
        // round trip has to be compared against the padded image.
        let mut expected = original.clone();
        expected.resize(image_size, 0);

        // Analyse the original image so the patched offsets are known.
        let symbols = recover_arm64_kernel_metadata(&original)
            .unwrap()
            .kallsyms
            .symbols;
        let required = RequiredSymbols::resolve(REQUIRED_SYMBOLS, &symbols).unwrap();
        let image_base = required.image_base();
        let sites = analyze_patch_sites(&original, &symbols, &required).unwrap();

        let (patched, report) = inject_image(&original, &module, "", false).unwrap();
        if let Ok(output) = std::env::var("KSU_LKM_TEST_OUT") {
            std::fs::write(output, &patched).unwrap();
        }
        assert!(report.replaced.is_none());

        // The capsule is appended where the image used to end, and the header's
        // `image_size` grows to cover it.
        let capsule_offset = align_up(image_size, 16);
        let new_image_size = parse_arm64_image_size(&patched).unwrap();
        assert!(new_image_size > image_size);
        assert!(new_image_size.is_multiple_of(CAPSULE_ALIGNMENT));
        assert_eq!(patched.len(), new_image_size);
        assert_eq!(&patched[capsule_offset..capsule_offset + 8], CAPSULE_MAGIC);

        // Only the three patched branches and the bootstrap cave may differ,
        // and the branches have to land in the padding the cave was taken from.
        let patched_sites = [
            sites.async_call,
            sites.strndup_call,
            sites.memblock_reserve_call,
        ];
        assert_eq!(
            read_u64(&patched, ARM64_IMAGE_SIZE_OFFSET).unwrap(),
            u64::try_from(new_image_size).unwrap(),
            "the header has to describe the grown image"
        );
        let header_size_field = ARM64_IMAGE_SIZE_OFFSET..ARM64_IMAGE_SIZE_OFFSET + 8;
        let mut changed = 0usize;
        for offset in 0..image_size {
            if patched[offset] == expected[offset] || header_size_field.contains(&offset) {
                continue;
            }
            changed += 1;
            let is_site = patched_sites.iter().any(|site| {
                offset >= site.file_offset && offset < site.file_offset + BlBranch::LENGTH
            });
            assert!(
                is_site || expected[offset] == TEXT_PADDING_BYTE,
                "unexpected change at 0x{offset:x}"
            );
        }
        assert!(changed >= 3, "expected at least three patched sites");
        for site in patched_sites {
            let target = BlBranch::decode(&patched, site.file_offset, site.address).unwrap();
            let target_offset = usize::try_from(target - image_base).unwrap();
            assert_eq!(
                expected[target_offset], TEXT_PADDING_BYTE,
                "the patched branch must enter the bootstrap cave"
            );
        }

        // Restoring has to bring the whole image back byte for byte.
        let restored = restore_image(&patched).unwrap();
        assert_eq!(restored.report.patched_sites, 3);
        assert_eq!(restored.image, expected);
        // Once restored there is no capsule left to work from, and neither has
        // an image that was never patched.
        for image in [&restored.image, &original] {
            let error = restore_image(image).unwrap_err();
            assert!(error.to_string().contains("not patched"), "{error}");
        }

        // Patching the patched image undoes the previous injection first and
        // then reproduces it byte for byte, which is what makes "update" a
        // one-command operation.
        let (again, report) = inject_image(&patched, &module, "", false).unwrap();
        assert_eq!(report.replaced.unwrap().patched_sites, 3);
        assert_eq!(
            again, patched,
            "re-patching an already patched image has to be a no-op byte for byte"
        );
    }

    /// The embedded object and the definition table have to agree in both
    /// directions: a definition the assembly stopped using, or a symbol it
    /// started referencing without one, used to fail silently at boot.
    #[test]
    fn bootstrap_definitions_match_the_object() {
        let object = BootstrapObject::parse(BOOTSTRAP_OBJECT, EM_AARCH64).unwrap();
        let required = RequiredSymbols::synthetic(REQUIRED_SYMBOLS);
        let call_site = |file_offset: usize, address: u64, target: u64| CallSite {
            file_offset,
            address,
            target,
        };
        let sites = PatchSites {
            async_call: call_site(0x1000, 0xffff_ff80_0000_1000, 0xffff_ff80_0000_2000),
            strndup_call: call_site(0x1100, 0xffff_ff80_0000_1100, 0xffff_ff80_0000_2100),
            memblock_reserve_call: call_site(0x1200, 0xffff_ff80_0000_1200, 0xffff_ff80_0000_2200),
            page_offset: 0xffff_ff80_0000_0000,
        };
        let definitions = bootstrap_definitions(&BootstrapDefinitions {
            required: &required,
            sites: &sites,
            abi: &GKI_ABI,
            capsule_file_offset: 0x20_0000,
            capsule_data_len: 0x1000,
            capsule_module_offset: 0x40,
            capsule_fixup_offset: 0x60,
            capsule_load_args_offset: 0x80,
            reserve_extension: 0x1000,
            module_size: 0x100,
            fixup_count: 1,
            printk_address: required.optional("_printk").expect("synthetic").address,
        });
        assert_definition_contract("aarch64", &object, &definitions);
        assert_ne!(definitions["ksu_ext_printk"], 0);

        // `printk` is the one dependency the stub can live without: a kernel
        // that does not export it still has to link, because the logger branch
        // goes to a `ret` inside the cave instead.
        let required = RequiredSymbols::synthetic_without_optionals(REQUIRED_SYMBOLS);
        let code_address = 0xffff_ff80_0000_0000;
        let printk_address = bootstrap_printk_address(&required, &object, code_address).unwrap();
        let definitions = bootstrap_definitions(&BootstrapDefinitions {
            required: &required,
            sites: &sites,
            abi: &GKI_ABI,
            capsule_file_offset: 0x20_0000,
            capsule_data_len: 0x1000,
            capsule_module_offset: 0x40,
            capsule_fixup_offset: 0x60,
            capsule_load_args_offset: 0x80,
            reserve_extension: 0x1000,
            module_size: 0x100,
            fixup_count: 1,
            printk_address,
        });
        assert!(required.optional("_printk").is_none());
        assert_definition_contract("aarch64 without printk", &object, &definitions);
        // The synthetic definitions in this test all live just above
        // 0xffff_ff80_0000_0000, so the link has to happen there for the
        // branch ranges to be meaningful.
        object
            .link(code_address, &definitions, Arch::Aarch64)
            .expect("a kernel without printk has to stay injectable");
    }

    #[test]
    fn embedded_bootstrap_links_with_rust_relocator() {
        let image_base = 0xffff_ffc0_0800_0000;
        let object = BootstrapObject::parse(BOOTSTRAP_OBJECT, EM_AARCH64).unwrap();
        let definitions = bootstrap_test_definitions(&object, image_base);
        let linked = object
            .link(image_base, &definitions, Arch::Aarch64)
            .unwrap();
        assert_eq!(linked.data.len(), object.image_size());
        assert_eq!(linked.entry_address, image_base);
        assert!(
            (image_base..image_base + linked.data.len() as u64)
                .contains(&linked.reserve_wrapper_address)
        );
        assert!(
            (image_base..image_base + linked.data.len() as u64)
                .contains(&linked.strndup_adapter_address)
        );
        assert_eq!(read_u32(&linked.data, 0).unwrap(), 0xd503_245f);
    }

    #[test]
    fn embedded_bootstrap_rejects_unknown_relocation() {
        let mut data = BOOTSTRAP_OBJECT.to_vec();
        let relocation_info_offset = {
            let object = BootstrapObject::parse(&data, EM_AARCH64).unwrap();
            object
                .sections
                .iter()
                .find(|section| section.section_type == SHT_RELA)
                .unwrap()
                .offset
                + 8
        };
        let info = read_u64(&data, relocation_info_offset).unwrap();
        write_u64(
            &mut data,
            relocation_info_offset,
            (info & !u64::from(u32::MAX)) | 0xffff,
        )
        .unwrap();
        let object = BootstrapObject::parse(&data, EM_AARCH64).unwrap();
        let image_base = 0xffff_ffc0_0800_0000;
        let definitions = bootstrap_test_definitions(&object, image_base);
        let error = object
            .link(image_base, &definitions, Arch::Aarch64)
            .unwrap_err();
        assert!(error.to_string().contains("unsupported AArch64"));
    }

    #[test]
    fn memblock_kernel_reservation_semantic_match() {
        let base = 0xffff_ffc0_0800_0000;
        let caller = MapSymbol {
            address: base + 0x20,
            name: "arm64_memblock_init".to_owned(),
        };
        let target = MapSymbol {
            address: base + 0x180,
            name: "memblock_reserve".to_owned(),
        };
        let symbols = SymbolMap::new(vec![caller.clone(), target.clone()]).unwrap();
        let mut image = vec![0u8; 0x200];
        let call_offset = 0x60;
        write_u32(
            &mut image,
            call_offset - 8,
            0xcb00_0000 | (3 << 16) | (2 << 5) | 1,
        )
        .unwrap();
        write_u32(
            &mut image,
            call_offset - 4,
            0xcb00_0000 | (4 << 16) | (3 << 5),
        )
        .unwrap();
        write_u32(
            &mut image,
            call_offset,
            encode_bl(base + call_offset as u64, target.address).unwrap(),
        )
        .unwrap();
        let call = find_kernel_image_memblock_reserve_call(
            &image,
            &symbols,
            base,
            &caller,
            &HashSet::from([target.address]),
            0x100,
        )
        .unwrap();
        assert_eq!(call.file_offset, call_offset);
    }

    #[test]
    fn recovers_pre_6_4_kallsyms() {
        let (image, base) = build_kallsyms_fixture("pre-6.4");
        let recovered = recover_arm64_kernel_metadata(&image).unwrap();
        assert!(recovered.btf.is_none());
        let recovered = recovered.kallsyms;
        assert_eq!(recovered.layout, "pre-6.4");
        assert_eq!(recovered.count, 2049);
        assert_eq!(recovered.symbols.resolve("_text").unwrap().address, base);
        assert_eq!(
            recovered
                .symbols
                .resolve("fixture_symbol_1024")
                .unwrap()
                .address,
            base + 0x2000 + 1024 * 0x20
        );
    }

    #[test]
    fn recovers_6_4_plus_kallsyms() {
        let (image, base) = build_kallsyms_fixture("6.4+");
        let recovered = recover_arm64_kernel_metadata(&image).unwrap();
        assert!(recovered.btf.is_none());
        let recovered = recovered.kallsyms;
        assert_eq!(recovered.layout, "6.4+");
        assert_eq!(recovered.count, 2049);
        assert_eq!(recovered.symbols.resolve("_text").unwrap().address, base);
        assert_eq!(
            recovered
                .symbols
                .resolve("fixture_symbol_1024")
                .unwrap()
                .address,
            base + 0x2000 + 1024 * 0x20
        );
    }

    #[test]
    fn selects_vmlinux_btf_by_kallsyms_boundaries() {
        let unrelated_btf = unusable_load_info_test_btf();
        let kernel_btf = minimal_test_btf();
        let unrelated_offset = 0x17_0000usize;
        let kernel_offset = 0x18_0000usize;
        let (mut image, _) =
            build_kallsyms_fixture_with_btf("6.4+", Some((kernel_offset, kernel_btf.len())));
        image[unrelated_offset..unrelated_offset + unrelated_btf.len()]
            .copy_from_slice(&unrelated_btf);
        image[kernel_offset..kernel_offset + kernel_btf.len()].copy_from_slice(&kernel_btf);

        assert_eq!(find_btf_candidates(&image).len(), 2);
        let recovered = recover_arm64_kernel_metadata(&image).unwrap();
        let selected = recovered.btf.unwrap();
        assert_eq!(selected.file_offset, kernel_offset);
        assert_eq!(selected.size, kernel_btf.len());
    }

    #[test]
    fn ignores_unmatched_embedded_btf() {
        let unrelated_btf = unusable_load_info_test_btf();
        let unrelated_offset = 0x17_0000usize;
        let (mut image, _) = build_kallsyms_fixture("6.4+");
        image[unrelated_offset..unrelated_offset + unrelated_btf.len()]
            .copy_from_slice(&unrelated_btf);

        assert_eq!(find_btf_candidates(&image).len(), 1);
        let recovered = recover_arm64_kernel_metadata(&image).unwrap();
        assert!(recovered.btf.is_none());
    }

    #[test]
    fn validates_btf_boundaries_against_kallsyms() {
        let base = 0xffff_ffc0_0800_0000;
        let btf = KernelBtf {
            file_offset: 0x1234,
            size: 0x5678,
            type_count: 1,
            load_info: None,
        };
        let symbols = SymbolMap::new(vec![
            MapSymbol {
                address: base,
                name: "_text".to_owned(),
            },
            MapSymbol {
                address: base + btf.file_offset as u64,
                name: "__start_BTF".to_owned(),
            },
            MapSymbol {
                address: base + (btf.file_offset + btf.size) as u64,
                name: "__stop_BTF".to_owned(),
            },
        ])
        .unwrap();
        validate_kallsyms_btf_boundaries(&symbols, btf.file_offset, btf.size).unwrap();

        let mismatched = KernelBtf {
            size: btf.size + 1,
            ..btf
        };
        assert!(
            validate_kallsyms_btf_boundaries(&symbols, mismatched.file_offset, mismatched.size)
                .is_err()
        );
    }

    #[test]
    fn btf_overlays_load_info_abi_with_aligned_storage() {
        let current = KernelBtf {
            file_offset: 0,
            size: 1,
            type_count: 1,
            load_info: Some(LoadInfoLayout {
                structure_size: 136,
                hdr_offset: 16,
                len_offset: 24,
            }),
        };
        let current_abi = GKI_ABI.apply_btf(&current).unwrap();
        assert_eq!(current_abi.load_info_storage_size, 256);

        let btf = KernelBtf {
            file_offset: 0,
            size: 1,
            type_count: 1,
            load_info: Some(LoadInfoLayout {
                structure_size: 264,
                hdr_offset: 24,
                len_offset: 32,
            }),
        };
        let abi = GKI_ABI.apply_btf(&btf).unwrap();
        assert_eq!(abi.load_info_structure_size, Some(264));
        assert_eq!(abi.load_info_storage_size, 272);
        assert_eq!(abi.load_info_hdr_offset, 24);
        assert_eq!(abi.load_info_len_offset, 32);
    }
}
