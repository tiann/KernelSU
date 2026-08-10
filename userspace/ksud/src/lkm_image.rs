// SPDX-License-Identifier: GPL-2.0-only

#![allow(
    clippy::many_single_char_names,
    clippy::similar_names,
    clippy::struct_field_names
)]

use std::collections::{BTreeMap, BTreeSet, HashMap, HashSet};
use std::fs;
use std::io::{Cursor, Write};
use std::path::{Path, PathBuf};

use android_bootimg::parser::BootImage;
use android_bootimg::patcher::BootImagePatchOption;
use anyhow::{Context, Result, anyhow, bail, ensure};

use crate::{assets, boot_patch};

// Image, capsule, ELF, and kallsyms wire constants.

const ARM64_IMAGE_MAGIC_OFFSET: usize = 0x38;
const ARM64_IMAGE_MAGIC: &[u8; 4] = b"ARM\x64";
const ARM64_IMAGE_SIZE_OFFSET: usize = 0x10;
const CAPSULE_MAGIC: &[u8; 8] = b"KSULKM1\0";
const CAPSULE_VERSION: u32 = 1;
const CAPSULE_ALIGNMENT: usize = 4096;
const CAPSULE_HEADER_SIZE: usize = 96;
const CAPSULE_FLAG_SHN_ABS_FIXUPS: u64 = 1;
const EM_AARCH64: u16 = 183;
const ET_REL: u16 = 1;
const SHT_PROGBITS: u32 = 1;
const SHT_SYMTAB: u32 = 2;
const SHT_STRTAB: u32 = 3;
const SHT_RELA: u32 = 4;
const SHT_NOBITS: u32 = 8;
const SHT_REL: u32 = 9;
const SHF_WRITE: u64 = 1;
const SHF_ALLOC: u64 = 2;
const SHF_EXECINSTR: u64 = 4;
const SHN_UNDEF: u16 = 0;
const SHN_ABS: u16 = 0xfff1;
const R_AARCH64_ABS64: u32 = 257;
const R_AARCH64_ABS32: u32 = 258;
const R_AARCH64_JUMP26: u32 = 282;
const R_AARCH64_CALL26: u32 = 283;
const R_AARCH64_ADR_PREL_PG_HI21: u32 = 275;
const R_AARCH64_ADD_ABS_LO12_NC: u32 = 277;
const ELF64_SECTION_SIZE: usize = 64;
const ELF64_SYMBOL_SIZE: usize = 24;
const ELF64_RELA_SIZE: usize = 24;
const KALLSYMS_ALIGNMENT: usize = 8;
const KALLSYMS_TOKEN_COUNT: usize = 256;
const KALLSYMS_TOKEN_INDEX_SIZE: usize = KALLSYMS_TOKEN_COUNT * 2;
const KALLSYMS_MAX_TOKEN_LENGTH: usize = 256;
const KALLSYMS_MARKER_SEARCH_WINDOW: usize = 4 * 1024 * 1024;
const KALLSYMS_NAME_TAIL_SEARCH: usize = 0x40000;
const KALLSYMS_MIN_MARKERS: usize = 8;
const KALLSYMS_MAX_MARKERS: usize = 4096;
const TEXT_CAVE_ALIGNMENT: usize = 16;
const TEXT_CAVE_PREFERRED_ALIGNMENT: usize = 4096;
const BOOTSTRAP_OBJECT: &[u8] = include_bytes!(concat!(env!("OUT_DIR"), "/lkm_image_bootstrap.o"));

// CLI and top-level result types.

#[derive(clap::Args, Debug)]
pub struct BootPatchV2Args {
    /// Source boot image
    #[arg(short, long)]
    pub boot: PathBuf,

    /// Exact kernelsu.ko; auto-select the embedded KMI build when omitted
    #[arg(short, long)]
    pub module: Option<PathBuf>,

    /// Patched boot image output
    #[arg(short, long)]
    pub output: PathBuf,

    /// Replace an existing output file
    #[arg(long, default_value = "false")]
    pub force: bool,
}

#[derive(Clone, Debug)]
struct MapSymbol {
    address: u64,
    name: String,
}

#[derive(Debug)]
struct SymbolMap {
    entries: Vec<MapSymbol>,
    by_name: HashMap<String, Vec<usize>>,
    by_normalized_name: HashMap<String, Vec<usize>>,
}

impl SymbolMap {
    fn new(mut entries: Vec<MapSymbol>) -> Result<Self> {
        entries.retain(|entry| entry.address != 0);
        ensure!(
            !entries.is_empty(),
            "symbol map has no non-zero kernel addresses"
        );
        entries.sort_by(|left, right| {
            (left.address, left.name.as_str()).cmp(&(right.address, right.name.as_str()))
        });
        let mut by_name: HashMap<String, Vec<usize>> = HashMap::new();
        let mut by_normalized_name: HashMap<String, Vec<usize>> = HashMap::new();
        for (index, entry) in entries.iter().enumerate() {
            by_name.entry(entry.name.clone()).or_default().push(index);
            by_normalized_name
                .entry(normalize_symbol(&entry.name).to_owned())
                .or_default()
                .push(index);
        }
        Ok(Self {
            entries,
            by_name,
            by_normalized_name,
        })
    }

    fn variants(&self, requested_name: &str) -> Vec<MapSymbol> {
        let mut unique = BTreeMap::<u64, MapSymbol>::new();
        if let Some(indices) = self
            .by_normalized_name
            .get(normalize_symbol(requested_name))
        {
            for &index in indices {
                let candidate = &self.entries[index];
                unique
                    .entry(candidate.address)
                    .or_insert_with(|| candidate.clone());
            }
        }
        unique.into_values().collect()
    }

    fn resolve(&self, requested_name: &str) -> Result<MapSymbol> {
        if let Some(indices) = self.by_name.get(requested_name) {
            let mut exact = BTreeMap::<u64, MapSymbol>::new();
            for &index in indices {
                let candidate = &self.entries[index];
                exact
                    .entry(candidate.address)
                    .or_insert_with(|| candidate.clone());
            }
            if exact.len() == 1 {
                return Ok(exact.into_values().next().expect("one exact symbol"));
            }
        }
        let candidates = self.variants(requested_name);
        if candidates.len() == 1 {
            return Ok(candidates[0].clone());
        }
        let rendered = candidates
            .iter()
            .take(8)
            .map(|candidate| format!("{}@0x{:x}", candidate.name, candidate.address))
            .collect::<Vec<_>>()
            .join(", ");
        if rendered.is_empty() {
            bail!("symbol {requested_name:?} was not found")
        }
        bail!("symbol {requested_name:?} is not unique: {rendered}")
    }

    fn resolve_module_symbol(&self, requested_name: &str) -> Option<MapSymbol> {
        self.resolve(requested_name).ok()
    }

    fn next_address(&self, address: u64) -> Option<u64> {
        self.entries
            .iter()
            .find(|entry| entry.address > address)
            .map(|entry| entry.address)
    }
}

#[derive(Debug)]
struct RequiredSymbols {
    image_base: MapSymbol,
    image_end: MapSymbol,
    text_start: MapSymbol,
    text_end: MapSymbol,
    linux_banner: MapSymbol,
    arm64_memblock_init: MapSymbol,
    memblock_reserve: MapSymbol,
    memstart_addr: MapSymbol,
    kimage_voffset: MapSymbol,
    kernel_init: MapSymbol,
    async_synchronize_full: MapSymbol,
    load_module: MapSymbol,
    strndup_user: MapSymbol,
    capable: MapSymbol,
    modules_disabled: MapSymbol,
    security_kernel_load_data: MapSymbol,
    security_kernel_post_load_data: MapSymbol,
    vmalloc: MapSymbol,
    memcpy: MapSymbol,
    vfree: MapSymbol,
    kstrdup: MapSymbol,
}

impl RequiredSymbols {
    fn resolve(symbols: &SymbolMap) -> Result<Self> {
        let vmalloc = symbols.resolve("vmalloc").or_else(|primary_error| {
            symbols
                .resolve("vmalloc_noprof")
                .with_context(|| primary_error.to_string())
        })?;
        Ok(Self {
            image_base: symbols.resolve("_text")?,
            image_end: symbols.resolve("_end")?,
            text_start: symbols.resolve("_stext")?,
            text_end: symbols.resolve("_etext")?,
            linux_banner: symbols.resolve("linux_banner")?,
            arm64_memblock_init: symbols.resolve("arm64_memblock_init")?,
            memblock_reserve: symbols.resolve("memblock_reserve")?,
            memstart_addr: symbols.resolve("memstart_addr")?,
            kimage_voffset: symbols.resolve("kimage_voffset")?,
            kernel_init: symbols.resolve("kernel_init")?,
            async_synchronize_full: symbols.resolve("async_synchronize_full")?,
            load_module: symbols.resolve("load_module")?,
            strndup_user: symbols.resolve("strndup_user")?,
            capable: symbols.resolve("capable")?,
            modules_disabled: symbols.resolve("modules_disabled")?,
            security_kernel_load_data: symbols.resolve("security_kernel_load_data")?,
            security_kernel_post_load_data: symbols.resolve("security_kernel_post_load_data")?,
            vmalloc,
            memcpy: symbols.resolve("memcpy")?,
            vfree: symbols.resolve("vfree")?,
            kstrdup: symbols.resolve("kstrdup")?,
        })
    }

    const fn image_base(&self) -> u64 {
        self.image_base.address
    }

    fn validate_image_bounds(&self, image_size: usize) -> Result<()> {
        let image_base = self.image_base();
        let image_end_offset = self
            .image_end
            .address
            .checked_sub(image_base)
            .context("_end is below _text")?;
        ensure!(
            image_end_offset == image_size as u64,
            "image_size=0x{image_size:x} does not match {}-{}=0x{image_end_offset:x}",
            self.image_end.name,
            self.image_base.name
        );

        for (key, symbol, allow_end) in self.entries() {
            let offset = symbol.address.checked_sub(image_base).ok_or_else(|| {
                anyhow!(
                    "required symbol {key}={}@0x{:x} is below the Image base",
                    symbol.name,
                    symbol.address
                )
            })?;
            let maximum_offset = if allow_end {
                image_size as u64
            } else {
                image_size.saturating_sub(1) as u64
            };
            ensure!(
                offset <= maximum_offset,
                "required symbol {key}={}@0x{:x} is outside image_size",
                symbol.name,
                symbol.address
            );
        }
        Ok(())
    }

    const fn entries(&self) -> [(&'static str, &MapSymbol, bool); 21] {
        [
            ("image_base", &self.image_base, false),
            ("image_end", &self.image_end, true),
            ("text_start", &self.text_start, false),
            ("text_end", &self.text_end, false),
            ("linux_banner", &self.linux_banner, false),
            ("arm64_memblock_init", &self.arm64_memblock_init, false),
            ("memblock_reserve", &self.memblock_reserve, false),
            ("memstart_addr", &self.memstart_addr, false),
            ("kimage_voffset", &self.kimage_voffset, false),
            ("kernel_init", &self.kernel_init, false),
            (
                "async_synchronize_full",
                &self.async_synchronize_full,
                false,
            ),
            ("load_module", &self.load_module, false),
            ("strndup_user", &self.strndup_user, false),
            ("capable", &self.capable, false),
            ("modules_disabled", &self.modules_disabled, false),
            (
                "security_kernel_load_data",
                &self.security_kernel_load_data,
                false,
            ),
            (
                "security_kernel_post_load_data",
                &self.security_kernel_post_load_data,
                false,
            ),
            ("vmalloc", &self.vmalloc, false),
            ("memcpy", &self.memcpy, false),
            ("vfree", &self.vfree, false),
            ("kstrdup", &self.kstrdup, false),
        ]
    }
}

#[derive(Clone, Copy, Debug)]
struct CallSite {
    file_offset: usize,
    address: u64,
    target: u64,
}

#[derive(Clone, Copy, Debug)]
struct PatchSites {
    async_call: CallSite,
    strndup_call: CallSite,
    memblock_reserve_call: CallSite,
    page_offset: u64,
}

#[derive(Debug)]
struct Fixup {
    symbol_file_offset: usize,
    kernel_offset: u64,
}

#[derive(Debug)]
struct Capsule {
    data: Vec<u8>,
    file_offset: usize,
    image_size: usize,
    module_offset: usize,
    fixup_offset: usize,
}

#[derive(Debug)]
struct RecoveredKallsyms {
    symbols: SymbolMap,
    layout: &'static str,
    count: usize,
}

#[derive(Clone, Copy, Debug)]
struct GkiAbi {
    load_info_size: u64,
    load_info_hdr_offset: u64,
    load_info_len_offset: u64,
    cap_sys_module: u64,
    loading_module_id: u64,
    gfp_kernel: u64,
}

const GKI_ABI: GkiAbi = GkiAbi {
    load_info_size: 256,
    load_info_hdr_offset: 16,
    load_info_len_offset: 24,
    cap_sys_module: 16,
    loading_module_id: 2,
    gfp_kernel: 0xcc0,
};

impl GkiAbi {
    fn validate(self) -> Result<()> {
        ensure!(
            self.load_info_size > 0
                && self.load_info_size <= 4096
                && self.load_info_size.is_multiple_of(16),
            "invalid built-in GKI load_info size"
        );
        for (field, offset) in [
            ("hdr_offset", self.load_info_hdr_offset),
            ("len_offset", self.load_info_len_offset),
        ] {
            ensure!(
                offset.is_multiple_of(8) && offset.saturating_add(8) <= self.load_info_size,
                "invalid built-in GKI load_info {field}"
            );
        }
        Ok(())
    }
}

#[derive(Debug)]
struct ImageInjectionReport {
    kernel_release: String,
    kallsyms_layout: &'static str,
    kallsyms_count: usize,
    code_offset: usize,
    code_size: usize,
    memblock_call_offset: usize,
    page_offset: u64,
    fixup_count: usize,
    unresolved: Vec<String>,
    image_size: usize,
}

// Little-endian buffer helpers.

fn normalize_symbol(name: &str) -> &str {
    let mut end = name.len();
    for marker in ["$", ".llvm."] {
        if let Some(position) = name.find(marker) {
            end = end.min(position);
        }
    }
    let name = &name[..end];
    name.strip_suffix(".cfi_jt").unwrap_or(name)
}

const fn align_up(value: usize, alignment: usize) -> usize {
    (value + alignment - 1) & !(alignment - 1)
}

fn get_slice(data: &[u8], offset: usize, size: usize) -> Result<&[u8]> {
    data.get(offset..offset.saturating_add(size))
        .ok_or_else(|| anyhow!("offset 0x{offset:x} size 0x{size:x} is outside input"))
}

fn read_u16(data: &[u8], offset: usize) -> Result<u16> {
    Ok(u16::from_le_bytes(get_slice(data, offset, 2)?.try_into()?))
}

fn read_u32(data: &[u8], offset: usize) -> Result<u32> {
    Ok(u32::from_le_bytes(get_slice(data, offset, 4)?.try_into()?))
}

fn read_i32(data: &[u8], offset: usize) -> Result<i32> {
    Ok(i32::from_le_bytes(get_slice(data, offset, 4)?.try_into()?))
}

fn read_u64(data: &[u8], offset: usize) -> Result<u64> {
    Ok(u64::from_le_bytes(get_slice(data, offset, 8)?.try_into()?))
}

fn read_i64(data: &[u8], offset: usize) -> Result<i64> {
    Ok(i64::from_le_bytes(get_slice(data, offset, 8)?.try_into()?))
}

fn write_u32(data: &mut [u8], offset: usize, value: u32) -> Result<()> {
    get_slice_mut(data, offset, 4)?.copy_from_slice(&value.to_le_bytes());
    Ok(())
}

fn write_u64(data: &mut [u8], offset: usize, value: u64) -> Result<()> {
    get_slice_mut(data, offset, 8)?.copy_from_slice(&value.to_le_bytes());
    Ok(())
}

fn get_slice_mut(data: &mut [u8], offset: usize, size: usize) -> Result<&mut [u8]> {
    let end = offset
        .checked_add(size)
        .ok_or_else(|| anyhow!("offset overflow"))?;
    data.get_mut(offset..end)
        .ok_or_else(|| anyhow!("offset 0x{offset:x} size 0x{size:x} is outside output"))
}

fn find_subslice(data: &[u8], needle: &[u8], start: usize) -> Option<usize> {
    if needle.is_empty() || start > data.len() {
        return None;
    }
    data[start..]
        .windows(needle.len())
        .position(|window| window == needle)
        .map(|position| position + start)
}

// Kallsyms recovery.

fn parse_kallsyms_token_table_at(
    image: &[u8],
    start: usize,
    digit_offset: usize,
) -> Option<(Vec<Vec<u8>>, usize)> {
    if !start.is_multiple_of(KALLSYMS_ALIGNMENT) {
        return None;
    }

    let mut tokens = Vec::with_capacity(KALLSYMS_TOKEN_COUNT);
    let mut token_offsets = Vec::with_capacity(KALLSYMS_TOKEN_COUNT);
    let mut token_starts = Vec::with_capacity(KALLSYMS_TOKEN_COUNT);
    let mut position = start;
    for _ in 0..KALLSYMS_TOKEN_COUNT {
        token_starts.push(position);
        let search_end = position
            .checked_add(KALLSYMS_MAX_TOKEN_LENGTH + 1)?
            .min(image.len());
        let length = image
            .get(position..search_end)?
            .iter()
            .position(|byte| *byte == 0)?;
        if length == 0 {
            return None;
        }
        let end = position.checked_add(length)?;
        let token = image.get(position..end)?;
        if token.iter().any(|byte| !(0x20..=0x7e).contains(byte)) {
            return None;
        }
        token_offsets.push(position.checked_sub(start)?);
        tokens.push(token.to_vec());
        position = end.checked_add(1)?;
    }

    if *token_starts.get(b'0' as usize)? != digit_offset {
        return None;
    }
    for &value in b"0123456789_abcdefghijklmnopqrstuvwxyzT" {
        if tokens.get(value as usize)?.as_slice() != [value] {
            return None;
        }
    }

    let token_index_offset = align_up(position, KALLSYMS_ALIGNMENT);
    if image
        .get(position..token_index_offset)?
        .iter()
        .any(|byte| *byte != 0)
    {
        return None;
    }
    let token_index_end = token_index_offset.checked_add(KALLSYMS_TOKEN_INDEX_SIZE)?;
    if token_index_end > image.len() || *token_offsets.last()? > u16::MAX as usize {
        return None;
    }
    for (index, &expected) in token_offsets.iter().enumerate() {
        let actual = read_u16(image, token_index_offset + index * 2).ok()?;
        if actual as usize != expected {
            return None;
        }
    }
    Some((tokens, token_index_offset))
}

fn find_kallsyms_token_table(image: &[u8]) -> Result<(Vec<Vec<u8>>, usize, usize)> {
    let mut digit_tokens = Vec::with_capacity(20);
    for value in b'0'..=b'9' {
        digit_tokens.extend_from_slice(&[value, 0]);
    }

    let mut candidates = BTreeMap::<usize, (Vec<Vec<u8>>, usize, usize)>::new();
    let mut search_from = 0;
    while let Some(digit_offset) = find_subslice(image, &digit_tokens, search_from) {
        let earliest = digit_offset.saturating_sub(b'0' as usize * (KALLSYMS_MAX_TOKEN_LENGTH + 1));
        let mut start = align_up(earliest, KALLSYMS_ALIGNMENT);
        while start <= digit_offset {
            if let Some((tokens, token_index_offset)) =
                parse_kallsyms_token_table_at(image, start, digit_offset)
            {
                candidates.insert(start, (tokens, start, token_index_offset));
            }
            start += KALLSYMS_ALIGNMENT;
        }
        search_from = digit_offset.saturating_add(1);
        if search_from >= image.len() {
            break;
        }
    }

    ensure!(
        candidates.len() == 1,
        "cannot uniquely recover kallsyms_token_table from ARM64 Image ({} candidates)",
        candidates.len()
    );
    Ok(candidates.into_values().next().expect("one token table"))
}

fn read_kallsyms_markers(image: &[u8], start: usize, limit: usize) -> Vec<u32> {
    if !start.is_multiple_of(KALLSYMS_ALIGNMENT) || start.saturating_add(8) > limit {
        return Vec::new();
    }
    let Ok(first) = read_u32(image, start) else {
        return Vec::new();
    };
    let Ok(second) = read_u32(image, start + 4) else {
        return Vec::new();
    };
    if first != 0 || !(0x200..=0x40000).contains(&second) {
        return Vec::new();
    }

    let mut markers = vec![first, second];
    let mut position = start + 8;
    while markers.len() < KALLSYMS_MAX_MARKERS && position.saturating_add(4) <= limit {
        let Ok(value) = read_u32(image, position) else {
            break;
        };
        let Some(delta) = value.checked_sub(*markers.last().expect("non-empty markers")) else {
            break;
        };
        if !(0x200..=0x40000).contains(&delta) {
            break;
        }
        markers.push(value);
        position += 4;
    }
    if markers.len() < KALLSYMS_MIN_MARKERS {
        Vec::new()
    } else {
        markers
    }
}

fn parse_kallsyms_name_spans(
    image: &[u8],
    names_offset: usize,
    count: usize,
    markers_offset: usize,
    markers: &[u32],
    uleb128_lengths: bool,
) -> Option<Vec<(usize, usize)>> {
    if count == 0 || markers.len() != count.div_ceil(256) {
        return None;
    }

    let mut spans = Vec::with_capacity(count);
    let mut position = names_offset;
    for index in 0..count {
        if index.is_multiple_of(256)
            && position.checked_sub(names_offset)? != markers[index / 256] as usize
        {
            return None;
        }
        if position >= markers_offset {
            return None;
        }
        let mut length = *image.get(position)? as usize;
        position += 1;
        if uleb128_lengths && length & 0x80 != 0 {
            if position >= markers_offset {
                return None;
            }
            let high = *image.get(position)? as usize;
            if high & 0x80 != 0 {
                return None;
            }
            length = (length & 0x7f) | (high << 7);
            position += 1;
        }
        if length == 0 || length > 0x3fff || position.checked_add(length)? > markers_offset {
            return None;
        }
        spans.push((position, length));
        position += length;
    }

    if align_up(position, KALLSYMS_ALIGNMENT) != markers_offset
        || image
            .get(position..markers_offset)?
            .iter()
            .any(|byte| *byte != 0)
    {
        return None;
    }
    Some(spans)
}

fn decode_kallsyms_names(
    image: &[u8],
    spans: &[(usize, usize)],
    tokens: &[Vec<u8>],
) -> Option<Vec<(u8, String)>> {
    const SYMBOL_TYPES: &[u8] = b"aAbBcCdDeEfFgGiInNpPrRsStTuUvVwW?-";

    let mut decoded = Vec::with_capacity(spans.len());
    for &(position, length) in spans {
        let encoded = image.get(position..position.checked_add(length)?)?;
        let expanded_length = encoded.iter().try_fold(0usize, |total, &index| {
            total.checked_add(tokens.get(index as usize)?.len())
        })?;
        if expanded_length > 4096 {
            return None;
        }
        let mut expanded = Vec::with_capacity(expanded_length);
        for &index in encoded {
            expanded.extend_from_slice(tokens.get(index as usize)?);
        }
        let (&kind, name_bytes) = expanded.split_first()?;
        if name_bytes.is_empty()
            || !SYMBOL_TYPES.contains(&kind)
            || name_bytes.iter().any(|byte| !(0x21..=0x7e).contains(byte))
        {
            return None;
        }
        decoded.push((kind, String::from_utf8(name_bytes.to_vec()).ok()?));
    }

    let names = decoded
        .iter()
        .map(|(_, name)| name.as_str())
        .collect::<HashSet<_>>();
    if ["_text", "_end", "load_module"]
        .iter()
        .any(|required| !names.contains(required))
    {
        return None;
    }
    Some(decoded)
}

type KallsymsNameTable = (usize, usize, usize, Vec<(u8, String)>);

fn find_kallsyms_names(
    image: &[u8],
    tokens: &[Vec<u8>],
    token_table_offset: usize,
) -> Vec<KallsymsNameTable> {
    let lower = token_table_offset.saturating_sub(KALLSYMS_MARKER_SEARCH_WINDOW);
    let mut marker_candidates = Vec::new();
    let mut position = align_up(lower, KALLSYMS_ALIGNMENT);
    while position.saturating_add(8) <= token_table_offset {
        let markers = read_kallsyms_markers(image, position, token_table_offset);
        if !markers.is_empty() {
            marker_candidates.push((position, markers));
        }
        position += KALLSYMS_ALIGNMENT;
    }

    let mut recovered = Vec::new();
    for (markers_offset, markers) in marker_candidates {
        let minimum_count = (markers.len() - 1) * 256 + 1;
        let maximum_count = markers.len() * 256;
        let Some(approximate_names) =
            markers_offset.checked_sub(*markers.last().expect("non-empty markers") as usize)
        else {
            continue;
        };
        let search_start = approximate_names.saturating_sub(KALLSYMS_NAME_TAIL_SEARCH);
        let mut num_syms_offset = approximate_names - approximate_names % KALLSYMS_ALIGNMENT;
        loop {
            if num_syms_offset.saturating_add(8) <= image.len() {
                let count = read_u32(image, num_syms_offset).unwrap_or(0) as usize;
                let padding_is_zero = image
                    .get(num_syms_offset + 4..num_syms_offset + 8)
                    .is_some_and(|padding| padding == [0; 4]);
                if (minimum_count..=maximum_count).contains(&count) && padding_is_zero {
                    let names_offset = num_syms_offset + KALLSYMS_ALIGNMENT;
                    let mut seen_spans: Vec<Vec<(usize, usize)>> = Vec::new();
                    for uleb128_lengths in [true, false] {
                        let Some(spans) = parse_kallsyms_name_spans(
                            image,
                            names_offset,
                            count,
                            markers_offset,
                            &markers,
                            uleb128_lengths,
                        ) else {
                            continue;
                        };
                        if seen_spans.contains(&spans) {
                            continue;
                        }
                        seen_spans.push(spans.clone());
                        if let Some(decoded) = decode_kallsyms_names(image, &spans, tokens) {
                            recovered.push((
                                num_syms_offset,
                                names_offset,
                                markers_offset,
                                decoded,
                            ));
                        }
                    }
                }
            }
            if num_syms_offset < search_start.saturating_add(KALLSYMS_ALIGNMENT) {
                break;
            }
            num_syms_offset -= KALLSYMS_ALIGNMENT;
        }
    }
    recovered
}

const fn is_arm64_kernel_address(address: u64) -> bool {
    address >> 48 == 0xffff && address.is_multiple_of(4096)
}

fn decode_kallsyms_addresses(
    image: &[u8],
    image_size: usize,
    names: &[(u8, String)],
    offsets_offset: usize,
    relative_base_offset: usize,
) -> Option<Vec<MapSymbol>> {
    let count = names.len();
    let offsets_end = offsets_offset.checked_add(count.checked_mul(4)?)?;
    if offsets_end > image.len()
        || relative_base_offset < offsets_end
        || relative_base_offset.checked_add(8)? > image.len()
        || image
            .get(offsets_end..relative_base_offset)?
            .iter()
            .any(|byte| *byte != 0)
    {
        return None;
    }
    let relative_base = read_u64(image, relative_base_offset).ok()?;
    if !is_arm64_kernel_address(relative_base) {
        return None;
    }

    let signed_offsets = (0..count)
        .map(|index| read_i32(image, offsets_offset + index * 4).ok())
        .collect::<Option<Vec<_>>>()?;
    let negative_count = signed_offsets.iter().filter(|offset| **offset < 0).count();
    let addresses = if negative_count * 2 >= count {
        signed_offsets
            .iter()
            .map(|&offset| {
                let address = if offset < 0 {
                    i128::from(relative_base) - 1 - i128::from(offset)
                } else {
                    i128::from(offset)
                };
                u64::try_from(address).ok()
            })
            .collect::<Option<Vec<_>>>()?
    } else {
        (0..count)
            .map(|index| {
                relative_base
                    .checked_add(u64::from(read_u32(image, offsets_offset + index * 4).ok()?))
            })
            .collect::<Option<Vec<_>>>()?
    };

    if addresses.windows(2).any(|pair| pair[0] > pair[1]) {
        return None;
    }
    let text_addresses = addresses
        .iter()
        .zip(names)
        .filter_map(|(&address, (_, name))| (name == "_text").then_some(address))
        .collect::<Vec<_>>();
    let end_addresses = addresses
        .iter()
        .zip(names)
        .filter_map(|(&address, (_, name))| (name == "_end").then_some(address))
        .collect::<Vec<_>>();
    if text_addresses.len() != 1
        || end_addresses.len() != 1
        || text_addresses[0] != relative_base
        || end_addresses[0].checked_sub(text_addresses[0])? != image_size as u64
    {
        return None;
    }

    let ordinary_addresses = addresses
        .iter()
        .zip(names)
        .filter_map(|(&address, (kind, _))| {
            (!b"Aa".contains(kind) && address != 0).then_some(address)
        })
        .collect::<Vec<_>>();
    let in_image = ordinary_addresses
        .iter()
        .filter(|&&address| text_addresses[0] <= address && address <= end_addresses[0])
        .count();
    if ordinary_addresses.is_empty() || in_image * 100 < ordinary_addresses.len() * 90 {
        return None;
    }

    Some(
        addresses
            .into_iter()
            .zip(names)
            .map(|(address, (_, name))| MapSymbol {
                address,
                name: name.clone(),
            })
            .collect(),
    )
}

fn recover_arm64_kallsyms(image: &[u8]) -> Result<RecoveredKallsyms> {
    let image_size = parse_arm64_image_size(image)?;
    let (tokens, token_table_offset, token_index_offset) = find_kallsyms_token_table(image)?;
    let name_tables = find_kallsyms_names(image, &tokens, token_table_offset);
    let mut candidates = Vec::new();

    for (num_syms_offset, _, _, names) in name_tables {
        let count = names.len();
        let old_relative_base_offset = num_syms_offset.saturating_sub(8);
        let old_offsets_offset = old_relative_base_offset.saturating_sub(align_up(count * 4, 8));
        let new_offsets_offset = align_up(
            token_index_offset + KALLSYMS_TOKEN_INDEX_SIZE,
            KALLSYMS_ALIGNMENT,
        );
        let new_relative_base_offset = align_up(new_offsets_offset + count * 4, KALLSYMS_ALIGNMENT);
        for (layout, offsets_offset, relative_base_offset) in [
            ("pre-6.4", old_offsets_offset, old_relative_base_offset),
            ("6.4+", new_offsets_offset, new_relative_base_offset),
        ] {
            let Some(entries) = decode_kallsyms_addresses(
                image,
                image_size,
                &names,
                offsets_offset,
                relative_base_offset,
            ) else {
                continue;
            };
            candidates.push(RecoveredKallsyms {
                symbols: SymbolMap::new(entries)?,
                layout,
                count,
            });
        }
    }

    ensure!(
        candidates.len() == 1,
        "cannot uniquely recover GKI kallsyms from ARM64 Image ({} candidates); CONFIG_KALLSYMS_ALL is required",
        candidates.len()
    );
    Ok(candidates.pop().expect("one kallsyms candidate"))
}

fn parse_arm64_image_size(image: &[u8]) -> Result<usize> {
    ensure!(
        image.len() >= 64,
        "kernel Image is smaller than the ARM64 header"
    );
    ensure!(
        get_slice(image, ARM64_IMAGE_MAGIC_OFFSET, ARM64_IMAGE_MAGIC.len())? == ARM64_IMAGE_MAGIC,
        "kernel input is not an uncompressed ARM64 Image"
    );
    let image_size = read_u64(image, ARM64_IMAGE_SIZE_OFFSET)?;
    ensure!(
        image_size != 0,
        "ARM64 Image header has image_size=0; unsupported input"
    );
    usize::try_from(image_size).context("ARM64 Image image_size does not fit this host")
}

// GKI ABI and ARM64 patch-site analysis.

fn recover_gki_abi(
    image: &[u8],
    image_base: u64,
    linux_banner: &MapSymbol,
) -> Result<(String, GkiAbi)> {
    let offset = address_to_offset(
        linux_banner.address,
        image_base,
        image.len(),
        &linux_banner.name,
    )?;
    let bounded = image
        .get(offset..offset.saturating_add(1024).min(image.len()))
        .context("linux_banner is outside the Image")?;
    let end = bounded
        .iter()
        .position(|byte| *byte == 0)
        .context("linux_banner is not a bounded C string in the Image")?;
    let banner = std::str::from_utf8(&bounded[..end]).context("linux_banner is not ASCII")?;
    let release = banner
        .strip_prefix("Linux version ")
        .and_then(|suffix| suffix.split_whitespace().next())
        .context("cannot recover the kernel release from linux_banner")?;
    let mut parts = release.splitn(3, '.');
    let major = parts
        .next()
        .context("kernel release has no major version")?
        .parse::<u32>()
        .context("kernel major version is invalid")?;
    let minor = parts
        .next()
        .context("kernel release has no minor version")?
        .parse::<u32>()
        .context("kernel minor version is invalid")?;
    ensure!(
        parts.next().is_some_and(|tail| !tail.is_empty()),
        "kernel release has no patch version"
    );
    ensure!(
        matches!((major, minor), (5, 10 | 15) | (6, 1 | 6 | 12)),
        "unsupported GKI kernel series {major}.{minor}; validated series: 5.10, 5.15, 6.1, 6.6, 6.12"
    );
    Ok((release.to_owned(), GKI_ABI))
}

fn decode_bl_target(instruction: u32, source_address: u64) -> Option<u64> {
    if instruction & 0xfc00_0000 != 0x9400_0000 {
        return None;
    }
    let immediate = i64::from((instruction << 6) as i32) >> 4;
    source_address.checked_add_signed(immediate)
}

fn encode_bl(source_address: u64, target_address: u64) -> Result<u32> {
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

fn address_to_offset(
    address: u64,
    image_base: u64,
    file_size: usize,
    label: &str,
) -> Result<usize> {
    let offset = address
        .checked_sub(image_base)
        .and_then(|value| usize::try_from(value).ok())
        .filter(|offset| offset.saturating_add(4) <= file_size)
        .ok_or_else(|| anyhow!("{label} address 0x{address:x} is outside the ARM64 Image file"))?;
    Ok(offset)
}

fn function_scan_range(
    image: &[u8],
    symbols: &SymbolMap,
    image_base: u64,
    function: &MapSymbol,
    maximum_scan_size: usize,
) -> Result<(usize, usize)> {
    let start = address_to_offset(function.address, image_base, image.len(), &function.name)?;
    let maximum_scan_size = u64::try_from(maximum_scan_size)?;
    let mut end_address = function
        .address
        .checked_add(maximum_scan_size)
        .context("function scan range overflow")?;
    if let Some(next_address) = symbols.next_address(function.address) {
        end_address = end_address.min(next_address);
    }
    let end = end_address
        .checked_sub(image_base)
        .and_then(|value| usize::try_from(value).ok())
        .unwrap_or(image.len())
        .min(image.len());
    ensure!(
        end > start,
        "cannot establish a scan range for {}",
        function.name
    );
    Ok((start, end))
}

fn find_unique_direct_call(
    image: &[u8],
    symbols: &SymbolMap,
    image_base: u64,
    function: &MapSymbol,
    accepted_targets: &HashSet<u64>,
    label: &str,
    maximum_scan_size: usize,
) -> Result<CallSite> {
    let (start, end) =
        function_scan_range(image, symbols, image_base, function, maximum_scan_size)?;
    let mut matches = Vec::new();
    for offset in (start..end.saturating_sub(3)).step_by(4) {
        let instruction = read_u32(image, offset)?;
        let address = image_base + offset as u64;
        if let Some(target) = decode_bl_target(instruction, address)
            && accepted_targets.contains(&target)
        {
            matches.push(CallSite {
                file_offset: offset,
                address,
                target,
            });
        }
    }
    ensure!(
        matches.len() == 1,
        "{label}: expected one direct BL in {}, found {}{}",
        function.name,
        matches.len(),
        if matches.is_empty() {
            String::new()
        } else {
            format!(
                " at {}",
                matches
                    .iter()
                    .map(|call| format!("0x{:x}", call.file_offset))
                    .collect::<Vec<_>>()
                    .join(", ")
            )
        }
    );
    Ok(matches[0])
}

fn decode_sub_shifted_register(instruction: u32) -> Option<(u32, u32, u32)> {
    (instruction & 0xffe0_fc00 == 0xcb00_0000).then_some((
        instruction & 31,
        (instruction >> 5) & 31,
        (instruction >> 16) & 31,
    ))
}

fn find_kernel_image_memblock_reserve_call(
    image: &[u8],
    symbols: &SymbolMap,
    image_base: u64,
    caller: &MapSymbol,
    accepted_targets: &HashSet<u64>,
    maximum_scan_size: usize,
) -> Result<CallSite> {
    let (start, end) = function_scan_range(image, symbols, image_base, caller, maximum_scan_size)?;
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

const fn rotate_right_width(value: u64, shift: u32, width: u32) -> u64 {
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

fn decode_orr_immediate(instruction: u32) -> Option<(u64, u32, u32)> {
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

fn infer_page_offset(
    image: &[u8],
    symbols: &SymbolMap,
    image_base: u64,
    function: &MapSymbol,
    maximum_scan_size: usize,
) -> Result<u64> {
    let (start, end) =
        function_scan_range(image, symbols, image_base, function, maximum_scan_size)?;
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

fn symbol_addresses(symbols: &SymbolMap, names: &[&str]) -> HashSet<u64> {
    names
        .iter()
        .flat_map(|name| symbols.variants(name))
        .map(|symbol| symbol.address)
        .collect()
}

fn analyze_patch_sites(
    image: &[u8],
    symbols: &SymbolMap,
    required: &RequiredSymbols,
) -> Result<PatchSites> {
    let image_base = required.image_base();
    let mut async_targets = symbol_addresses(
        symbols,
        &["async_synchronize_full", "async_synchronize_cookie_domain"],
    );
    async_targets.insert(required.async_synchronize_full.address);
    let mut strndup_targets = symbol_addresses(symbols, &["strndup_user"]);
    strndup_targets.insert(required.strndup_user.address);
    let mut memblock_targets = symbol_addresses(symbols, &["memblock_reserve"]);
    memblock_targets.insert(required.memblock_reserve.address);

    let async_call = find_unique_direct_call(
        image,
        symbols,
        image_base,
        &required.kernel_init,
        &async_targets,
        "kernel_init async call",
        0x4000,
    )?;
    let strndup_call = find_unique_direct_call(
        image,
        symbols,
        image_base,
        &required.load_module,
        &strndup_targets,
        "load_module strndup_user call",
        0x10000,
    )?;
    let memblock_reserve_call = find_kernel_image_memblock_reserve_call(
        image,
        symbols,
        image_base,
        &required.arm64_memblock_init,
        &memblock_targets,
        0x8000,
    )?;
    let page_offset = infer_page_offset(
        image,
        symbols,
        image_base,
        &required.arm64_memblock_init,
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

fn is_text_boundary_symbol(name: &str) -> bool {
    name == "_etext"
        || name.starts_with("__stop_")
        || name.contains("___stop_")
        || name.ends_with("_text_end")
}

fn find_text_tail_cave(
    image: &[u8],
    image_size: usize,
    symbols: &SymbolMap,
    image_base: u64,
    text_start: &MapSymbol,
    text_end: &MapSymbol,
    required_size: usize,
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
    while zero_start > start && image[zero_start - 1] == 0 {
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
        if image[candidate_start..end].iter().any(|byte| *byte != 0) {
            continue;
        }
        return Ok((candidate_start, end - candidate_start));
    }

    rejected_symbols.sort();
    rejected_symbols.dedup();
    bail!(
        "cannot find 0x{required_size:x} bytes of proven zero padding before {}{}",
        text_end.name,
        if rejected_symbols.is_empty() {
            String::new()
        } else {
            format!("; symbols in padding: {}", rejected_symbols.join(", "))
        }
    )
}

fn check_non_overlapping(ranges: &[(usize, usize, &str)]) -> Result<()> {
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

// Module ELF fixups and capsule construction.

#[derive(Clone, Copy)]
struct ElfSection {
    section_type: u32,
    offset: usize,
    size: usize,
    link: usize,
    entry_size: usize,
}

fn read_c_string(data: &[u8], offset: usize) -> Option<String> {
    let tail = data.get(offset..)?;
    let end = tail.iter().position(|byte| *byte == 0)?;
    Some(String::from_utf8_lossy(&tail[..end]).into_owned())
}

fn collect_module_fixups(
    module: &[u8],
    symbols: &SymbolMap,
    image_base: u64,
    image_size: usize,
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
        read_u16(module, 16)? == ET_REL && read_u16(module, 18)? == EM_AARCH64,
        "module must be an ARM64 ET_REL object"
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

fn pack_fixups(fixups: &[Fixup]) -> Result<Vec<u8>> {
    let mut output = Vec::with_capacity(fixups.len() * 16);
    for fixup in fixups {
        output.extend_from_slice(&u32::try_from(fixup.symbol_file_offset)?.to_le_bytes());
        output.extend_from_slice(&0u32.to_le_bytes());
        output.extend_from_slice(&fixup.kernel_offset.to_le_bytes());
    }
    Ok(output)
}

fn module_sha256(module: &[u8]) -> Result<[u8; 32]> {
    let digest = sha256::digest(module);
    let mut output = [0u8; 32];
    base16ct::lower::decode(digest.as_bytes(), &mut output)
        .map_err(|error| anyhow!("cannot decode module SHA-256: {error}"))?;
    Ok(output)
}

fn build_capsule(
    image_size: usize,
    module: &[u8],
    fixup_bytes: &[u8],
    fixup_count: usize,
) -> Result<Capsule> {
    ensure!(
        fixup_bytes.len() == fixup_count.saturating_mul(16),
        "fixup table length does not match its entry count"
    );
    let capsule_offset = align_up(image_size, 16);
    let module_relative_offset = CAPSULE_HEADER_SIZE;
    let fixup_relative_offset = module_relative_offset
        .checked_add(align_up(module.len(), 16))
        .context("capsule module offset overflow")?;
    let content_end = fixup_relative_offset
        .checked_add(fixup_bytes.len())
        .context("capsule fixup offset overflow")?;
    let new_image_size = align_up(
        capsule_offset
            .checked_add(content_end)
            .context("capsule image size overflow")?,
        CAPSULE_ALIGNMENT,
    );
    let capsule_size = new_image_size - capsule_offset;
    let flags = if fixup_count == 0 {
        0
    } else {
        CAPSULE_FLAG_SHN_ABS_FIXUPS
    };

    let mut data = vec![0u8; capsule_size];
    data[..8].copy_from_slice(CAPSULE_MAGIC);
    write_u32(&mut data, 8, CAPSULE_VERSION)?;
    write_u32(&mut data, 12, u32::try_from(CAPSULE_HEADER_SIZE)?)?;
    write_u64(&mut data, 16, u64::try_from(capsule_size)?)?;
    write_u64(&mut data, 24, u64::try_from(module_relative_offset)?)?;
    write_u64(&mut data, 32, u64::try_from(module.len())?)?;
    write_u64(&mut data, 40, u64::try_from(fixup_relative_offset)?)?;
    write_u64(&mut data, 48, u64::try_from(fixup_count)?)?;
    write_u64(&mut data, 56, flags)?;
    data[64..CAPSULE_HEADER_SIZE].copy_from_slice(&module_sha256(module)?);
    data[module_relative_offset..module_relative_offset + module.len()].copy_from_slice(module);
    data[fixup_relative_offset..fixup_relative_offset + fixup_bytes.len()]
        .copy_from_slice(fixup_bytes);

    Ok(Capsule {
        data,
        file_offset: capsule_offset,
        image_size: new_image_size,
        module_offset: capsule_offset + module_relative_offset,
        fixup_offset: capsule_offset + fixup_relative_offset,
    })
}

// Build-time embedded bootstrap and minimal runtime ET_REL linker.

#[derive(Clone, Debug)]
struct BootstrapSection {
    name: String,
    section_type: u32,
    flags: u64,
    offset: usize,
    size: usize,
    link: usize,
    info: usize,
    alignment: usize,
    entry_size: usize,
    output_offset: Option<usize>,
}

#[derive(Debug)]
struct BootstrapSymbol {
    name: String,
    value: u64,
    section_index: u16,
}

#[derive(Debug)]
struct BootstrapObject<'a> {
    data: &'a [u8],
    sections: Vec<BootstrapSection>,
    symbols: Vec<BootstrapSymbol>,
    symbol_table_index: usize,
    image_size: usize,
}

#[derive(Debug)]
struct LinkedBootstrap {
    data: Vec<u8>,
    entry_address: u64,
    reserve_wrapper_address: u64,
    strndup_adapter_address: u64,
}

fn checked_align_up(value: usize, alignment: usize) -> Result<usize> {
    ensure!(
        alignment.is_power_of_two(),
        "ELF section alignment is invalid"
    );
    value
        .checked_add(alignment - 1)
        .map(|aligned| aligned & !(alignment - 1))
        .context("ELF section layout overflow")
}

fn read_elf_string(data: &[u8], offset: usize) -> Result<&str> {
    let tail = data
        .get(offset..)
        .with_context(|| format!("ELF string offset 0x{offset:x} is outside its table"))?;
    let end = tail
        .iter()
        .position(|byte| *byte == 0)
        .context("unterminated ELF string")?;
    std::str::from_utf8(&tail[..end]).context("ELF string is not UTF-8")
}

impl<'a> BootstrapObject<'a> {
    fn parse(data: &'a [u8]) -> Result<Self> {
        ensure!(
            data.len() >= 64 && data.starts_with(b"\x7fELF"),
            "embedded bootstrap is not an ELF file"
        );
        ensure!(
            data[4] == 2 && data[5] == 1 && data[6] == 1,
            "embedded bootstrap must be little-endian ELF64"
        );
        ensure!(
            read_u16(data, 16)? == ET_REL && read_u16(data, 18)? == EM_AARCH64,
            "embedded bootstrap must be an AArch64 ET_REL object"
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

    const fn image_size(&self) -> usize {
        self.image_size
    }

    fn section_address(&self, section_index: usize, code_address: u64) -> Result<u64> {
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

    fn symbol_value(
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

    fn named_symbol_value(
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

    fn link(
        &self,
        code_address: u64,
        definitions: &BTreeMap<&str, u64>,
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
                let width = match relocation_type {
                    R_AARCH64_ABS64 => 8,
                    R_AARCH64_ABS32
                    | R_AARCH64_CALL26
                    | R_AARCH64_JUMP26
                    | R_AARCH64_ADR_PREL_PG_HI21
                    | R_AARCH64_ADD_ABS_LO12_NC => 4,
                    _ => bail!("unsupported AArch64 bootstrap relocation {relocation_type}"),
                };
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

fn apply_bootstrap_relocation(
    output: &mut [u8],
    output_offset: usize,
    place_address: u64,
    relocation_type: u32,
    symbol_value: u64,
    addend: i64,
) -> Result<()> {
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

// Raw ARM64 Image injection.

fn inject_image(original_image: &[u8], module: &[u8]) -> Result<(Vec<u8>, ImageInjectionReport)> {
    let mut image = original_image.to_vec();
    let image_size = parse_arm64_image_size(&image)?;
    ensure!(
        image.len() <= image_size,
        "input has bytes beyond ARM64 image_size; appended DTB/metadata is unsupported"
    );

    let RecoveredKallsyms {
        symbols,
        layout: kallsyms_layout,
        count: kallsyms_count,
    } = recover_arm64_kallsyms(&image)?;
    let required = RequiredSymbols::resolve(&symbols)?;
    required.validate_image_bounds(image_size)?;
    let image_base = required.image_base();
    let (kernel_release, gki_abi) = recover_gki_abi(&image, image_base, &required.linux_banner)?;
    gki_abi.validate()?;
    let sites = analyze_patch_sites(&image, &symbols, &required)?;

    let (fixups, unresolved) = collect_module_fixups(module, &symbols, image_base, image_size)?;
    let fixup_bytes = pack_fixups(&fixups)?;
    let capsule = build_capsule(image_size, module, &fixup_bytes, fixups.len())?;
    let reserve_extension = capsule
        .image_size
        .checked_sub(image_size)
        .context("capsule did not extend the Image")?;

    let mut definitions = BTreeMap::<&str, u64>::new();
    definitions.insert(
        "ksu_ext_memblock_reserve",
        sites.memblock_reserve_call.target,
    );
    definitions.insert("ksu_ext_memstart_addr", required.memstart_addr.address);
    definitions.insert("ksu_ext_kimage_voffset", required.kimage_voffset.address);
    definitions.insert("ksu_ext_async_synchronize_full", sites.async_call.target);
    definitions.insert("ksu_ext_capable", required.capable.address);
    definitions.insert(
        "ksu_ext_modules_disabled",
        required.modules_disabled.address,
    );
    definitions.insert(
        "ksu_ext_security_kernel_load_data",
        required.security_kernel_load_data.address,
    );
    definitions.insert("ksu_ext_vmalloc", required.vmalloc.address);
    definitions.insert("ksu_ext_memcpy", required.memcpy.address);
    definitions.insert(
        "ksu_ext_security_kernel_post_load_data",
        required.security_kernel_post_load_data.address,
    );
    definitions.insert("ksu_ext_load_module", required.load_module.address);
    definitions.insert("ksu_ext_vfree", required.vfree.address);
    definitions.insert("ksu_ext_kstrdup", required.kstrdup.address);
    definitions.insert("ksu_ext_strndup_user", sites.strndup_call.target);
    definitions.insert("ksu_image_base", image_base);
    definitions.insert("ksu_capsule_magic", u64::from_le_bytes(*CAPSULE_MAGIC));
    definitions.insert("ksu_capsule_version", u64::from(CAPSULE_VERSION));
    definitions.insert("ksu_capsule_header_size", CAPSULE_HEADER_SIZE as u64);
    definitions.insert("ksu_capsule_image_offset", capsule.file_offset as u64);
    definitions.insert("ksu_capsule_size", capsule.data.len() as u64);
    definitions.insert(
        "ksu_module_capsule_offset",
        capsule.module_offset.saturating_sub(capsule.file_offset) as u64,
    );
    definitions.insert(
        "ksu_fixup_capsule_offset",
        capsule.fixup_offset.saturating_sub(capsule.file_offset) as u64,
    );
    definitions.insert("ksu_reserve_extension", reserve_extension as u64);
    definitions.insert("ksu_page_offset", sites.page_offset);
    definitions.insert("ksu_module_size", module.len() as u64);
    definitions.insert("ksu_fixup_count", fixups.len() as u64);
    definitions.insert("ksu_load_info_size", gki_abi.load_info_size);
    definitions.insert("ksu_load_info_hdr_offset", gki_abi.load_info_hdr_offset);
    definitions.insert("ksu_load_info_len_offset", gki_abi.load_info_len_offset);
    definitions.insert("ksu_loading_module_id", gki_abi.loading_module_id);
    definitions.insert("ksu_cap_sys_module", gki_abi.cap_sys_module);
    definitions.insert("ksu_gfp_kernel", gki_abi.gfp_kernel);

    let bootstrap_object = BootstrapObject::parse(BOOTSTRAP_OBJECT)
        .context("cannot parse the embedded LKM bootstrap")?;
    let (code_offset, code_cave_size) = find_text_tail_cave(
        &image,
        image_size,
        &symbols,
        image_base,
        &required.text_start,
        &required.text_end,
        bootstrap_object.image_size(),
    )?;
    let code_address = image_base
        .checked_add(code_offset as u64)
        .context("bootstrap address overflow")?;
    let bootstrap = bootstrap_object
        .link(code_address, &definitions)
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
            sites.async_call.file_offset + 4,
            "kernel_init patch",
        ),
        (
            sites.strndup_call.file_offset,
            sites.strndup_call.file_offset + 4,
            "load_module patch",
        ),
        (
            sites.memblock_reserve_call.file_offset,
            sites.memblock_reserve_call.file_offset + 4,
            "memblock reserve patch",
        ),
    ])?;

    get_slice_mut(&mut image, code_offset, bootstrap.data.len())?.copy_from_slice(&bootstrap.data);
    write_u32(
        &mut image,
        sites.async_call.file_offset,
        encode_bl(sites.async_call.address, bootstrap_address)?,
    )?;
    write_u32(
        &mut image,
        sites.strndup_call.file_offset,
        encode_bl(sites.strndup_call.address, adapter_address)?,
    )?;
    write_u32(
        &mut image,
        sites.memblock_reserve_call.file_offset,
        encode_bl(sites.memblock_reserve_call.address, reserve_wrapper_address)?,
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
            kernel_release,
            kallsyms_layout,
            kallsyms_count,
            code_offset,
            code_size: bootstrap.data.len(),
            memblock_call_offset: sites.memblock_reserve_call.file_offset,
            page_offset: sites.page_offset,
            fixup_count: fixups.len(),
            unresolved,
            image_size: capsule.image_size,
        },
    ))
}

// boot-patch-v2 orchestration.

fn embedded_module_name(kmi: &str) -> String {
    #[cfg(target_os = "android")]
    {
        format!("{kmi}_kernelsu.ko")
    }
    #[cfg(not(target_os = "android"))]
    {
        format!("aarch64/{kmi}_kernelsu.ko")
    }
}

pub fn patch_boot(args: &BootPatchV2Args) -> Result<()> {
    ensure!(
        args.boot.is_file(),
        "boot image does not exist: {}",
        args.boot.display()
    );
    if let Some(module) = &args.module {
        ensure!(
            module.is_file(),
            "kernel module does not exist: {}",
            module.display()
        );
    }
    if args.output.exists() {
        let output = fs::canonicalize(&args.output)
            .with_context(|| format!("cannot resolve output {}", args.output.display()))?;
        let mut inputs = vec![("boot image", &args.boot)];
        if let Some(module) = &args.module {
            inputs.push(("kernel module", module));
        }
        for (label, input) in inputs {
            let input = fs::canonicalize(input)
                .with_context(|| format!("cannot resolve {label} {}", input.display()))?;
            ensure!(output != input, "refusing to overwrite the input {label}");
        }
        ensure!(
            args.force,
            "output already exists: {}; use --force",
            args.output.display()
        );
    }

    println!("- Reading boot image");
    let boot_data = fs::read(&args.boot)
        .with_context(|| format!("cannot read boot image {}", args.boot.display()))?;
    let boot_image = BootImage::parse(&boot_data).context("cannot parse boot image")?;
    let kernel = boot_image
        .get_blocks()
        .get_kernel()
        .context("boot image does not contain a kernel")?;
    println!("- Decompressing kernel");
    let mut raw_kernel = Vec::new();
    kernel
        .dump(&mut raw_kernel, false)
        .context("cannot decompress boot kernel")?;

    let module = if let Some(module) = &args.module {
        println!("- Module: {}", module.display());
        fs::read(module)
            .with_context(|| format!("cannot read kernel module {}", module.display()))?
    } else {
        let kmi = boot_patch::parse_kmi(&raw_kernel)
            .context("cannot detect KMI from the boot image kernel")?;
        let name = embedded_module_name(&kmi);
        println!("- KMI: {kmi}");
        println!("- Embedded module: {name}");
        assets::get_asset_data(&name)
            .with_context(|| format!("no embedded KernelSU module for KMI {kmi}: {name}"))?
            .into_owned()
    };

    println!("- Recovering kallsyms and injecting module");
    let (patched_kernel, report) = inject_image(&raw_kernel, &module)?;

    println!("- Repacking boot image");
    let mut patcher = BootImagePatchOption::new(&boot_image);
    patcher.replace_kernel(Box::new(Cursor::new(patched_kernel)), false);
    let mut repacked = Cursor::new(Vec::with_capacity(boot_data.len()));
    patcher
        .patch(&mut repacked)
        .context("cannot repack boot image")?;
    let repacked = repacked.into_inner();

    let output_parent = args
        .output
        .parent()
        .filter(|path| !path.as_os_str().is_empty())
        .unwrap_or_else(|| Path::new("."));
    fs::create_dir_all(output_parent)
        .with_context(|| format!("cannot create output directory {}", output_parent.display()))?;
    let mut temporary = tempfile::NamedTempFile::new_in(output_parent)
        .context("cannot create temporary boot image")?;
    temporary
        .write_all(&repacked)
        .context("cannot write patched boot image")?;
    temporary
        .flush()
        .context("cannot flush patched boot image")?;
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;

        let mode = fs::metadata(&args.boot)
            .with_context(|| format!("cannot stat boot image {}", args.boot.display()))?
            .permissions()
            .mode();
        temporary
            .as_file()
            .set_permissions(fs::Permissions::from_mode(mode))
            .context("cannot copy boot image permissions")?;
    }
    temporary.persist(&args.output).map_err(|error| {
        anyhow!(
            "cannot persist patched boot image {}: {}",
            args.output.display(),
            error.error
        )
    })?;

    println!("- Kernel: {}", report.kernel_release);
    println!(
        "- Kallsyms: {} ({} symbols)",
        report.kallsyms_layout, report.kallsyms_count
    );
    println!(
        "- Bootstrap: offset 0x{:x}, {} bytes",
        report.code_offset, report.code_size
    );
    println!(
        "- Memblock patch: offset 0x{:x}",
        report.memblock_call_offset
    );
    println!("- PAGE_OFFSET: 0x{:x}", report.page_offset);
    println!(
        "- Module fixups: {}, unresolved: {}",
        report.fixup_count,
        report.unresolved.len()
    );
    if !report.unresolved.is_empty() {
        println!(
            "- Native resolver symbols: {}",
            report.unresolved.join(", ")
        );
    }
    println!("- Patched Image size: 0x{:x}", report.image_size);
    println!("- Output: {}", args.output.display());
    Ok(())
}

// Focused format and analysis tests.

#[cfg(test)]
mod tests {
    use super::*;

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

    fn build_kallsyms_fixture(layout: &str) -> (Vec<u8>, u64) {
        let base = 0xffff_ffc0_0800_0000;
        let image_size = 0x20_0000usize;
        let count = 2049usize;
        let mut entries = vec![
            (base, b'T', "_text".to_owned()),
            (base + 0x1000, b't', "load_module".to_owned()),
        ];
        for index in 0..count - 3 {
            entries.push((
                base + 0x2000 + index as u64 * 0x20,
                b't',
                format!("fixture_symbol_{index:04}"),
            ));
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
    fn embedded_module_uses_release_asset_layout() {
        let name = embedded_module_name("android12-5.10");
        #[cfg(target_os = "android")]
        assert_eq!(name, "android12-5.10_kernelsu.ko");
        #[cfg(not(target_os = "android"))]
        assert_eq!(name, "aarch64/android12-5.10_kernelsu.ko");
        assert!(!assets::get_asset_data(&name).unwrap().is_empty());
    }

    #[test]
    fn detects_kmi_from_boot_kernel_banner() {
        let kernel = b"Linux version 6.1.157-android14-11-gki-test\0";
        assert_eq!(boot_patch::parse_kmi(kernel).unwrap(), "android14-6.1");
    }

    #[test]
    fn capsule_layout_matches_wire_format() {
        let module = b"module-bytes";
        let fixups = (0u8..16).collect::<Vec<_>>();
        let capsule = build_capsule(0x20_0003, module, &fixups, 1).unwrap();
        assert_eq!(capsule.file_offset, 0x20_0010);
        assert!(capsule.image_size.is_multiple_of(CAPSULE_ALIGNMENT));
        assert_eq!(&capsule.data[..8], CAPSULE_MAGIC);
        assert_eq!(read_u32(&capsule.data, 8).unwrap(), CAPSULE_VERSION);
        assert_eq!(
            read_u32(&capsule.data, 12).unwrap(),
            CAPSULE_HEADER_SIZE as u32
        );
        assert_eq!(read_u64(&capsule.data, 32).unwrap(), module.len() as u64);
        assert_eq!(read_u64(&capsule.data, 48).unwrap(), 1);
        let module_offset = usize::try_from(read_u64(&capsule.data, 24).unwrap()).unwrap();
        assert_eq!(
            &capsule.data[module_offset..module_offset + module.len()],
            module
        );
    }

    #[test]
    fn embedded_bootstrap_links_with_rust_relocator() {
        let image_base = 0xffff_ffc0_0800_0000;
        let object = BootstrapObject::parse(BOOTSTRAP_OBJECT).unwrap();
        let definitions = bootstrap_test_definitions(&object, image_base);
        let linked = object.link(image_base, &definitions).unwrap();
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
            let object = BootstrapObject::parse(&data).unwrap();
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
        let object = BootstrapObject::parse(&data).unwrap();
        let image_base = 0xffff_ffc0_0800_0000;
        let definitions = bootstrap_test_definitions(&object, image_base);
        let error = object.link(image_base, &definitions).unwrap_err();
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
        let recovered = recover_arm64_kallsyms(&image).unwrap();
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
        let recovered = recover_arm64_kallsyms(&image).unwrap();
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
}
