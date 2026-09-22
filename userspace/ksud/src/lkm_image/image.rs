// SPDX-License-Identifier: GPL-2.0-only
//!
//! The narrow interfaces shared by every architecture.
//!
//! The two kernels differ in almost everything that matters for the patch
//! itself, but two things are common: how a linked kernel address maps onto the
//! decompressed image ([`ImageLayout`]), and how a direct branch is encoded
//! ([`BranchCodec`]).  Everything that only needs those two -- finding the
//! unique `BL`/`call` that the bootstrap stub has to replace -- is implemented
//! once here.

#![allow(clippy::too_many_arguments)]

use std::collections::HashSet;

use anyhow::{Context, Result, anyhow, ensure};

use super::CallSite;
use super::bytes::get_slice_mut;
use super::symbols::{MapSymbol, SymbolMap};

/// Mapping between linked kernel addresses and content offsets in the
/// decompressed image.
///
/// The two are *not* interchangeable on x86-64: a segment may be loaded at a
/// different address than it is stored at, so an offset derived from
/// `image_base` only becomes a `content` index through this conversion.  Every
/// symbol in [`SymbolMap`] is an address, so every `content` index derived from
/// one goes through here.
pub trait ImageLayout {
    /// Content offset of a linked kernel address, when it is file backed.
    fn content_offset_of(&self, address: u64, image_base: u64) -> Option<usize>;

    /// Linked address of a decompressed content byte, when it is file backed.
    fn address_of(&self, content_offset: usize, image_base: u64) -> Option<u64>;

    /// Content offset just past the last file-backed byte of the image.
    fn content_end(&self) -> usize;

    /// [`Self::content_offset_of`] with a diagnostic and a `room` byte
    /// requirement, for the instructions that are about to be written there.
    fn offset_of(&self, address: u64, image_base: u64, room: usize, label: &str) -> Result<usize> {
        ensure!(
            address >= image_base,
            "{label} address 0x{address:x} is below the kernel image base"
        );
        self.content_offset_of(address, image_base)
            .filter(|offset| offset.saturating_add(room) <= self.content_end())
            .ok_or_else(|| {
                anyhow!("{label} address 0x{address:x} is not backed by the kernel image")
            })
    }

    /// [`Self::content_offset_of`], clamped to the end of the image.
    fn offset_or_end(&self, address: u64, image_base: u64, content_len: usize) -> usize {
        self.content_offset_of(address, image_base)
            .map_or(content_len, |offset| offset.min(content_len))
    }
}

/// The whole image is mapped one-to-one from `_text` at content offset 0,
/// which is what an uncompressed arm64 `Image` looks like.
#[derive(Clone, Copy, Debug)]
pub struct IdentityLayout {
    size: usize,
}

impl IdentityLayout {
    pub const fn new(size: usize) -> Self {
        Self { size }
    }
}

impl ImageLayout for IdentityLayout {
    fn content_offset_of(&self, address: u64, image_base: u64) -> Option<usize> {
        let offset = usize::try_from(address.checked_sub(image_base)?).ok()?;
        (offset <= self.size).then_some(offset)
    }

    fn address_of(&self, content_offset: usize, image_base: u64) -> Option<u64> {
        (content_offset <= self.size).then(|| image_base + content_offset as u64)
    }

    fn content_end(&self) -> usize {
        self.size
    }
}

/// A direct branch: the arm64 `BL` or the x86-64 `call rel32`.
pub trait BranchCodec {
    /// Instruction size in bytes.
    const LENGTH: usize;
    /// Offset multiple a branch may start at.
    const ALIGNMENT: usize;
    /// Instruction name used in diagnostics.
    const NAME: &'static str;

    /// Decode the branch at `offset`, if the bytes there are one.
    fn decode(content: &[u8], offset: usize, address: u64) -> Option<u64>;

    /// Encode a branch from `source_address` to `target_address`.
    fn encode(source_address: u64, target_address: u64) -> Result<Vec<u8>>;
}

/// Offsets that may contain a direct branch inside `function`.
///
/// The range stops at the next symbol, so a `bl`/`call` in a neighbouring
/// function is never attributed to this one.
pub fn patch_site_scan_range<L: ImageLayout, B: BranchCodec>(
    layout: &L,
    content: &[u8],
    symbols: &SymbolMap,
    image_base: u64,
    function: &MapSymbol,
    maximum_scan_size: usize,
) -> Result<(usize, usize)> {
    let start = layout.offset_of(function.address, image_base, B::LENGTH, &function.name)?;
    let end_address = function
        .address
        .checked_add(u64::try_from(maximum_scan_size)?)
        .context("function scan range overflow")?;
    let end_address = symbols
        .next_address(function.address)
        .map_or(end_address, |next| end_address.min(next));
    let end = layout.offset_or_end(end_address, image_base, content.len());
    ensure!(
        end > start,
        "cannot establish a scan range for {}",
        function.name
    );
    Ok((start, end))
}

/// The one direct branch in `function` that targets one of `accepted_targets`.
pub fn find_unique_direct_call<L: ImageLayout, B: BranchCodec>(
    layout: &L,
    content: &[u8],
    symbols: &SymbolMap,
    image_base: u64,
    function: &MapSymbol,
    accepted_targets: &HashSet<u64>,
    label: &str,
    maximum_scan_size: usize,
) -> Result<CallSite> {
    let (start, end) = patch_site_scan_range::<L, B>(
        layout,
        content,
        symbols,
        image_base,
        function,
        maximum_scan_size,
    )?;
    let mut matches = Vec::new();
    let mut offset = start;
    while offset + B::LENGTH <= end {
        let address = layout.address_of(offset, image_base);
        if let Some(address) = address
            && let Some(target) = B::decode(content, offset, address)
            && accepted_targets.contains(&target)
        {
            matches.push(CallSite {
                file_offset: offset,
                address,
                target,
            });
        }
        offset += B::ALIGNMENT;
    }
    ensure!(
        matches.len() == 1,
        "{label}: expected exactly one direct {} in {}, found {}{}",
        B::NAME,
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

/// A base address that cannot underflow, for callers that only want to know
/// whether the bytes at an offset decode as a branch.
pub const BRANCH_PROBE_ADDRESS: u64 = 0xffff_ffff_0000_0000;

/// Whether `content[offset..]` starts with a branch of this architecture.
pub fn is_branch<B: BranchCodec>(content: &[u8], offset: usize) -> bool {
    B::decode(content, offset, BRANCH_PROBE_ADDRESS).is_some()
}

/// Write the branch from `source_address` to `target_address` at `offset`.
pub fn write_branch<B: BranchCodec>(
    content: &mut [u8],
    offset: usize,
    source_address: u64,
    target_address: u64,
) -> Result<()> {
    let encoded = B::encode(source_address, target_address)?;
    get_slice_mut(content, offset, B::LENGTH)?.copy_from_slice(&encoded);
    Ok(())
}
