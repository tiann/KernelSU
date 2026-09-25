// SPDX-License-Identifier: GPL-2.0-only
//!
//! Kernel symbol maps (kallsyms recovery, BTF-assisted GKI ABI recovery) and
//! the list of symbols the bootstrap stub needs.

use std::collections::{BTreeMap, HashMap, HashSet};

use anyhow::{Context, Result, anyhow, bail, ensure};

use crate::lkm_image::btf::{KernelBtf, find_btf_candidates};

use super::bytes::{align_up, align_up_u64, find_subslice, read_i32, read_u16, read_u32, read_u64};
use super::image::{IdentityLayout, ImageLayout};

pub const KALLSYMS_ALIGNMENT: usize = 8;
pub const KALLSYMS_TOKEN_COUNT: usize = 256;
pub const KALLSYMS_TOKEN_INDEX_SIZE: usize = KALLSYMS_TOKEN_COUNT * 2;
pub const KALLSYMS_MAX_TOKEN_LENGTH: usize = 256;
pub const KALLSYMS_MARKER_SEARCH_WINDOW: usize = 4 * 1024 * 1024;
pub const KALLSYMS_NAME_TAIL_SEARCH: usize = 0x40000;
pub const KALLSYMS_MIN_MARKERS: usize = 8;
pub const KALLSYMS_MAX_MARKERS: usize = 4096;

pub const MINIMUM_LOAD_INFO_STORAGE_SIZE: u64 = 256;
pub const MAXIMUM_LOAD_INFO_STORAGE_SIZE: u64 = 4096;

#[derive(Clone, Debug)]
pub struct MapSymbol {
    pub address: u64,
    pub name: String,
}

#[derive(Debug)]
pub struct SymbolMap {
    pub entries: Vec<MapSymbol>,
    by_name: HashMap<String, Vec<usize>>,
    by_normalized_name: HashMap<String, Vec<usize>>,
}

impl SymbolMap {
    pub fn new(mut entries: Vec<MapSymbol>) -> Result<Self> {
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

    pub fn variants(&self, requested_name: &str) -> Vec<MapSymbol> {
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

    pub fn resolve(&self, requested_name: &str) -> Result<MapSymbol> {
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

    pub fn resolve_module_symbol(&self, requested_name: &str) -> Option<MapSymbol> {
        self.resolve(requested_name).ok()
    }

    pub fn next_address(&self, address: u64) -> Option<u64> {
        self.entries
            .iter()
            .find(|entry| entry.address > address)
            .map(|entry| entry.address)
    }
}

/// One kernel symbol the injected bootstrap needs.
///
/// `key` is both the map key and the primary symbol name; `fallback` covers the
/// renames a kernel may have picked up (`vmalloc` became `vmalloc_noprof` in
/// 6.10) and `hint` turns a missing symbol into an actionable error.
#[derive(Clone, Copy, Debug)]
pub struct RequiredSymbolSpec {
    key: &'static str,
    fallback: Option<&'static str>,
    hint: Option<&'static str>,
    allow_end: bool,
    optional: bool,
}

impl RequiredSymbolSpec {
    pub const fn new(key: &'static str) -> Self {
        Self {
            key,
            fallback: None,
            hint: None,
            allow_end: false,
            optional: false,
        }
    }

    /// Also try `fallback` when `key` is not in the kernel.
    pub const fn or(self, fallback: &'static str) -> Self {
        Self {
            key: self.key,
            fallback: Some(fallback),
            hint: self.hint,
            allow_end: self.allow_end,
            optional: self.optional,
        }
    }

    /// Explain what to do when the symbol is missing.
    pub const fn hint(self, hint: &'static str) -> Self {
        Self {
            key: self.key,
            fallback: self.fallback,
            hint: Some(hint),
            allow_end: self.allow_end,
            optional: self.optional,
        }
    }

    /// The symbol may sit exactly at `_end`.
    pub const fn allow_end(self) -> Self {
        Self {
            key: self.key,
            fallback: self.fallback,
            hint: self.hint,
            allow_end: true,
            optional: self.optional,
        }
    }

    /// The kernel may not have the symbol at all.
    ///
    /// A missing optional symbol never fails the injection: the architecture
    /// derives a zero definition from it and the bootstrap has to cope with
    /// that (that is how `printk` stays a best-effort dependency).  Optional
    /// symbols are also kept out of [`RequiredSymbols::validate_image_bounds`],
    /// which would otherwise reject the zero address.
    pub const fn optional(self) -> Self {
        Self {
            key: self.key,
            fallback: self.fallback,
            hint: self.hint,
            allow_end: self.allow_end,
            optional: true,
        }
    }
}

struct ResolvedSymbol {
    symbol: MapSymbol,
    allow_end: bool,
}

/// The symbols an architecture needs, resolved once against the kernel map.
///
/// Fields are looked up by the [`RequiredSymbolSpec::key`] the architecture
/// declared, so [`Self::get`] panics only when a key is asked for that
/// `resolve` was never given.
pub struct RequiredSymbols {
    symbols: BTreeMap<&'static str, ResolvedSymbol>,
    /// Specs declared [`RequiredSymbolSpec::optional`]; `None` means the kernel
    /// does not have them.
    optionals: BTreeMap<&'static str, Option<MapSymbol>>,
}

impl RequiredSymbols {
    pub fn resolve(specs: &[RequiredSymbolSpec], symbols: &SymbolMap) -> Result<Self> {
        let mut resolved = BTreeMap::new();
        let mut optionals = BTreeMap::new();
        for spec in specs {
            match resolve_with_fallback(symbols, spec) {
                Ok(symbol) => {
                    if spec.optional {
                        optionals.insert(spec.key, Some(symbol));
                    } else {
                        resolved.insert(
                            spec.key,
                            ResolvedSymbol {
                                symbol,
                                allow_end: spec.allow_end,
                            },
                        );
                    }
                }
                Err(error) => {
                    ensure!(
                        spec.optional,
                        "the kernel image does not provide required symbol {:?}: {error:#}",
                        spec.key
                    );
                    optionals.insert(spec.key, None);
                }
            }
        }
        Ok(Self {
            symbols: resolved,
            optionals,
        })
    }

    #[track_caller]
    pub fn get(&self, key: &str) -> &MapSymbol {
        &self
            .symbols
            .get(key)
            .unwrap_or_else(|| panic!("required symbol {key:?} is not in the spec list"))
            .symbol
    }

    /// An [`RequiredSymbolSpec::optional`] symbol, or `None` when the kernel
    /// does not provide it.
    #[track_caller]
    pub fn optional(&self, key: &str) -> Option<&MapSymbol> {
        self.optionals
            .get(key)
            .unwrap_or_else(|| panic!("optional symbol {key:?} is not in the spec list"))
            .as_ref()
    }

    pub fn image_base(&self) -> u64 {
        self.get("_text").address
    }

    pub fn image_end(&self) -> u64 {
        self.get("_end").address
    }

    /// `_end - _text` has to be the image size, and every other symbol has to
    /// fall inside it.
    pub fn validate_image_bounds(&self, image_size: usize) -> Result<()> {
        let image_base = self.image_base();
        let image_end = self.get("_end").address;
        let image_end_offset = image_end
            .checked_sub(image_base)
            .context("_end is below _text")?;
        ensure!(
            image_end_offset == image_size as u64,
            "image_size=0x{image_size:x} does not match {}-{}=0x{image_end_offset:x}",
            self.get("_end").name,
            self.get("_text").name
        );

        for (key, resolved) in &self.symbols {
            let offset = resolved
                .symbol
                .address
                .checked_sub(image_base)
                .ok_or_else(|| {
                    anyhow!(
                        "required symbol {key}={}@0x{:x} is below the Image base",
                        resolved.symbol.name,
                        resolved.symbol.address
                    )
                })?;
            let maximum = if resolved.allow_end {
                image_size as u64
            } else {
                image_size.saturating_sub(1) as u64
            };
            ensure!(
                offset <= maximum,
                "required symbol {key}={}@0x{:x} is outside image_size",
                resolved.symbol.name,
                resolved.symbol.address
            );
        }
        Ok(())
    }
}

#[cfg(test)]
impl RequiredSymbols {
    /// Every spec resolved to a distinct synthetic address, for the tests that
    /// only care about which symbols an architecture asks for.
    pub fn synthetic(specs: &[RequiredSymbolSpec]) -> Self {
        let mut symbols = BTreeMap::new();
        let mut optionals = BTreeMap::new();
        for (index, spec) in specs.iter().enumerate() {
            let address = 0xffff_ff80_0000_0000_u64 + 0x1000 * (index as u64 + 1);
            let symbol = MapSymbol {
                address,
                name: spec.key.to_owned(),
            };
            if spec.optional {
                optionals.insert(spec.key, Some(symbol));
            } else {
                symbols.insert(
                    spec.key,
                    ResolvedSymbol {
                        symbol,
                        allow_end: spec.allow_end,
                    },
                );
            }
        }
        Self { symbols, optionals }
    }

    /// [`Self::synthetic`], but every optional symbol is missing: this is what
    /// a kernel without `printk` looks like.
    pub fn synthetic_without_optionals(specs: &[RequiredSymbolSpec]) -> Self {
        let mut resolved = Self::synthetic(specs);
        for spec in specs.iter().filter(|spec| spec.optional) {
            resolved.optionals.insert(spec.key, None);
        }
        resolved
    }
}

fn resolve_with_fallback(symbols: &SymbolMap, spec: &RequiredSymbolSpec) -> Result<MapSymbol> {
    let primary = match symbols.resolve(spec.key) {
        Ok(symbol) => return Ok(symbol),
        Err(error) => error,
    };
    let error = if let Some(fallback) = spec.fallback {
        match symbols.resolve(fallback) {
            Ok(symbol) => return Ok(symbol),
            Err(fallback_error) => fallback_error.context(primary.to_string()),
        }
    } else {
        primary
    };
    match spec.hint {
        Some(hint) => Err(error.context(hint)),
        None => Err(error),
    }
}

#[derive(Debug)]
pub struct RecoveredKallsyms {
    pub symbols: SymbolMap,
    pub layout: &'static str,
    pub count: usize,
}

#[derive(Debug)]
pub struct RecoveredKernelMetadata {
    pub kallsyms: RecoveredKallsyms,
    pub btf: Option<KernelBtf>,
}

#[derive(Clone, Copy, Debug)]
pub struct GkiAbi {
    pub load_info_structure_size: Option<u64>,
    pub load_info_storage_size: u64,
    pub load_info_hdr_offset: u64,
    pub load_info_len_offset: u64,
    pub gfp_kernel: u64,
}

pub const GKI_ABI: GkiAbi = GkiAbi {
    load_info_structure_size: None,
    load_info_storage_size: MINIMUM_LOAD_INFO_STORAGE_SIZE,
    load_info_hdr_offset: 16,
    load_info_len_offset: 24,
    gfp_kernel: 0xcc0,
};

impl GkiAbi {
    pub fn apply_btf(mut self, btf: &KernelBtf) -> Result<Self> {
        if let Some(layout) = btf.load_info {
            self.load_info_structure_size = Some(layout.structure_size);
            self.load_info_storage_size = align_up_u64(
                layout.structure_size.max(MINIMUM_LOAD_INFO_STORAGE_SIZE),
                16,
            )
            .context("BTF load_info storage size overflow")?;
            self.load_info_hdr_offset = layout.hdr_offset;
            self.load_info_len_offset = layout.len_offset;
        }
        self.validate()?;
        Ok(self)
    }

    pub fn validate(self) -> Result<()> {
        ensure!(
            self.load_info_storage_size > 0
                && self.load_info_storage_size <= MAXIMUM_LOAD_INFO_STORAGE_SIZE
                && self.load_info_storage_size.is_multiple_of(16),
            "invalid GKI load_info storage size"
        );
        let layout_size = self
            .load_info_structure_size
            .unwrap_or(self.load_info_storage_size);
        ensure!(
            layout_size > 0 && layout_size <= self.load_info_storage_size,
            "invalid GKI load_info structure size"
        );
        for (field, offset) in [
            ("hdr_offset", self.load_info_hdr_offset),
            ("len_offset", self.load_info_len_offset),
        ] {
            ensure!(
                offset.is_multiple_of(8) && offset.saturating_add(8) <= layout_size,
                "invalid GKI load_info {field}"
            );
        }
        Ok(())
    }
}

// Little-endian buffer helpers.
pub fn normalize_symbol(name: &str) -> &str {
    let mut end = name.len();
    for marker in ["$", ".llvm."] {
        if let Some(position) = name.find(marker) {
            end = end.min(position);
        }
    }
    let name = &name[..end];
    name.strip_suffix(".cfi_jt").unwrap_or(name)
}

// Kallsyms recovery.
pub fn parse_kallsyms_token_table_at(
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

pub fn find_kallsyms_token_tables(image: &[u8]) -> Vec<(Vec<Vec<u8>>, usize, usize)> {
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

    candidates.into_values().collect()
}

pub fn read_kallsyms_markers(image: &[u8], start: usize, limit: usize) -> Vec<u32> {
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

pub fn parse_kallsyms_name_spans(
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

pub fn decode_kallsyms_names(
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

pub type KallsymsNameTable = (usize, usize, usize, Vec<(u8, String)>);

pub fn find_kallsyms_names(
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

pub const fn is_arm64_kernel_address(address: u64) -> bool {
    address >> 48 == 0xffff && address.is_multiple_of(4096)
}

pub fn decode_kallsyms_addresses(
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

pub fn validate_kallsyms_btf_boundaries(
    symbols: &SymbolMap,
    btf_file_offset: usize,
    btf_size: usize,
) -> Result<()> {
    let image_base = symbols.resolve("_text")?;
    let btf_start = symbols.resolve("__start_BTF")?;
    let btf_stop = symbols.resolve("__stop_BTF")?;
    let recovered_offset = btf_start
        .address
        .checked_sub(image_base.address)
        .context("__start_BTF is below _text")?;
    let recovered_size = btf_stop
        .address
        .checked_sub(btf_start.address)
        .context("__stop_BTF is below __start_BTF")?;
    ensure!(
        recovered_offset == btf_file_offset as u64,
        "kallsyms __start_BTF offset 0x{recovered_offset:x} does not match BTF file offset 0x{btf_file_offset:x}"
    );
    ensure!(
        recovered_size == btf_size as u64,
        "kallsyms BTF size 0x{recovered_size:x} does not match parsed BTF size 0x{btf_size:x}"
    );
    Ok(())
}

/// Recover the GKI kallsyms table and, when it exists, the vmlinux BTF blob
/// that bounds it.
///
/// `image` is the decompressed kernel, `image_size` is `_end - _text` and
/// `container` names it in the diagnostics ("ARM64 Image", "x86-64 image").
pub fn recover_kernel_metadata(
    image: &[u8],
    image_size: usize,
    container: &str,
) -> Result<RecoveredKernelMetadata> {
    let btf_candidates = find_btf_candidates(image);
    let token_tables = find_kallsyms_token_tables(image);
    let token_table_count = token_tables.len();
    let mut candidates = Vec::new();
    let mut complete_candidates = 0usize;

    for (tokens, token_table_offset, token_index_offset) in token_tables {
        let name_tables = find_kallsyms_names(image, &tokens, token_table_offset);
        for (num_syms_offset, _, _, names) in name_tables {
            let count = names.len();
            let old_relative_base_offset = num_syms_offset.saturating_sub(8);
            let old_offsets_offset =
                old_relative_base_offset.saturating_sub(align_up(count * 4, 8));
            let new_offsets_offset = align_up(
                token_index_offset + KALLSYMS_TOKEN_INDEX_SIZE,
                KALLSYMS_ALIGNMENT,
            );
            let new_relative_base_offset =
                align_up(new_offsets_offset + count * 4, KALLSYMS_ALIGNMENT);
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
                complete_candidates += 1;
                let recovered = RecoveredKallsyms {
                    symbols: SymbolMap::new(entries)?,
                    layout,
                    count,
                };
                candidates.push(recovered);
            }
        }
    }

    let mut boundary_matches = Vec::new();
    for (kallsyms_index, kallsyms) in candidates.iter().enumerate() {
        for (btf_index, btf) in btf_candidates.iter().enumerate() {
            if validate_kallsyms_btf_boundaries(&kallsyms.symbols, btf.file_offset(), btf.size())
                .is_ok()
            {
                boundary_matches.push((kallsyms_index, btf_index));
            }
        }
    }

    if boundary_matches.len() > 1 {
        bail!(
            "cannot uniquely match vmlinux BTF to GKI kallsyms in {container} ({} token tables, {complete_candidates} complete kallsyms candidates, {} valid BTF blobs, {} boundary matches)",
            token_table_count,
            btf_candidates.len(),
            boundary_matches.len()
        );
    }

    if let Some((kallsyms_index, btf_index)) = boundary_matches.pop() {
        let kallsyms = candidates.swap_remove(kallsyms_index);
        let btf = btf_candidates[btf_index]
            .to_kernel_btf()
            .context("cannot parse vmlinux BTF selected by kallsyms boundaries")?;
        return Ok(RecoveredKernelMetadata {
            kallsyms,
            btf: Some(btf),
        });
    }

    if candidates.len() != 1 {
        bail!(
            "cannot uniquely recover GKI kallsyms from {container} ({} token tables, {complete_candidates} complete candidates, {} valid BTF blobs, no BTF boundary match); CONFIG_KALLSYMS_ALL is required",
            token_table_count,
            btf_candidates.len()
        );
    }
    Ok(RecoveredKernelMetadata {
        kallsyms: candidates.pop().expect("one kallsyms candidate"),
        btf: None,
    })
}

// GKI ABI and ARM64 patch-site analysis.
pub fn recover_gki_abi(
    image: &[u8],
    image_base: u64,
    linux_banner: &MapSymbol,
    btf: Option<&KernelBtf>,
) -> Result<(String, GkiAbi)> {
    let offset = IdentityLayout::new(image.len()).offset_of(
        linux_banner.address,
        image_base,
        4,
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
    let abi = match btf {
        Some(btf) => GKI_ABI.apply_btf(btf)?,
        None => GKI_ABI,
    };
    abi.validate()?;
    Ok((release.to_owned(), abi))
}

pub fn symbol_addresses(symbols: &SymbolMap, names: &[&str]) -> HashSet<u64> {
    names
        .iter()
        .flat_map(|name| symbols.variants(name))
        .map(|symbol| symbol.address)
        .collect()
}

pub fn is_text_boundary_symbol(name: &str) -> bool {
    name == "_etext"
        || name.starts_with("__stop_")
        || name.contains("___stop_")
        || name.ends_with("_text_end")
}
