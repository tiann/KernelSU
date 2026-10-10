// SPDX-License-Identifier: GPL-2.0-only
//!
//! x86-64 (`bzImage`) LKM injection for `ksud boot-patch-v2`.
//!
//! The arm64 implementation appends the `KSULKM1` capsule past the end of an
//! uncompressed kernel `Image` and grows the header's `image_size` field.  An
//! x86-64 kernel is a `bzImage`: a small position-dependent stub immediately
//! followed by an LZ4 compressed payload, with the rest of the stub's code and
//! data stored *after* the payload.  Stub and payload reference each other with
//! link-time offsets, so the payload cannot grow in place.
//!
//! The injection therefore works like this:
//!
//! 1. decode the payload, recover kallsyms/BTF from the decompressed image and
//!    insert the capsule inside that image, right after `_end` and before the
//!    kernel's relocation table (which must stay at the very end);
//! 2. patch the three call sites (kernel-image reservation, `kernel_init`,
//!    `load_module`) and link the x86-64 bootstrap into the `int3` padding at
//!    the tail of `.text`;
//! 3. recompress the image (the capsule makes it bigger) and rebuild the
//!    `bzImage`, shifting the stub data that follows the payload and adjusting
//!    every cross-boundary reference, plus the stub's copy length, `input_len`
//!    and `output_len` and the setup header fields.

#![allow(clippy::too_many_arguments)]

use std::collections::BTreeMap;

use anyhow::{Context, Result, bail, ensure};

use super::arm64::check_non_overlapping;
use super::bootstrap::{BOOTSTRAP_X86_64_OBJECT, BootstrapObject};
use super::bytes::{
    align_up, find_subslice, get_slice, get_slice_mut, read_i32, read_u16, read_u32, write_u32,
};
use super::capsule::{build_capsule_at, capsule_wire_definitions};
use super::elf::{EM_X86_64, collect_module_fixups, pack_fixups};
use super::image::{BranchCodec, ImageLayout, find_unique_direct_call, is_branch, write_branch};
use super::modinfo::override_module_vermagic;
use super::restore::{
    ContainerFields, PatchedImage, RestoreCave, RestoreDetails, RestoreRecord, RestoreReport,
    RestoreSite, RestoredImage, Undone, open_injection, open_patched_image,
};
use super::{ImageInjectionReport, InjectionDetails};

use super::symbols::{
    GKI_ABI, GkiAbi, MapSymbol, RequiredSymbolSpec, RequiredSymbols, SymbolMap,
    recover_kernel_metadata, symbol_addresses,
};
use super::x86_64_layout::{KernelImageLayout, X86_PAGE_SIZE};
use super::{Arch, CallSite};
use crate::lkm_image::btf::KernelBtf;

/// Everything the x86-64 bootstrap resolves from the kernel symbol map.
pub const REQUIRED_SYMBOLS: &[RequiredSymbolSpec] = &[
    RequiredSymbolSpec::new("_text"),
    RequiredSymbolSpec::new("_end").allow_end(),
    RequiredSymbolSpec::new("__end_of_kernel_reserve").allow_end(),
    RequiredSymbolSpec::new("_stext"),
    RequiredSymbolSpec::new("_etext"),
    RequiredSymbolSpec::new("linux_banner"),
    RequiredSymbolSpec::new("kernel_init"),
    RequiredSymbolSpec::new("async_synchronize_full"),
    RequiredSymbolSpec::new("load_module"),
    RequiredSymbolSpec::new("strndup_user"),
    RequiredSymbolSpec::new("memblock_reserve"),
    RequiredSymbolSpec::new("vmalloc").or("vmalloc_noprof"),
    RequiredSymbolSpec::new("memcpy"),
    RequiredSymbolSpec::new("kstrdup"),
    RequiredSymbolSpec::new("page_offset_base")
        .hint("the kernel has no page_offset_base (CONFIG_RANDOMIZE_MEMORY is required)"),
    RequiredSymbolSpec::new("phys_base"),
    // The bootstrap logs through the kernel's printk; `_printk` is the symbol
    // since 5.15 (older kernels keep a real `printk`).  Missing means the
    // injection still succeeds, only without its boot messages.
    RequiredSymbolSpec::new("_printk").or("printk").optional(),
];

/// The x86-64 specific invariants on top of
/// [`RequiredSymbols::validate_image_bounds`]: the reservation has to stay
/// inside `[_text, _end]` and `.text` has to be a real range.
pub fn validate_symbols(required: &RequiredSymbols, layout: &KernelImageLayout) -> Result<()> {
    required.validate_image_bounds(
        usize::try_from(layout.kernel_total_size)
            .context("kernel image size does not fit this host")?,
    )?;
    let reserve_end = required.get("__end_of_kernel_reserve");
    ensure!(
        reserve_end.address >= required.image_base() && reserve_end.address <= required.image_end(),
        "__end_of_kernel_reserve is outside the kernel image"
    );
    ensure!(
        required.get("_etext").address > required.get("_stext").address,
        "_etext is not above _stext"
    );
    Ok(())
}

// --- bzImage (x86 boot protocol) constants ---------------------------------
pub const SETUP_SECTS_OFFSET: usize = 0x1f1;
pub const SYSSIZE_OFFSET: usize = 0x1f4;
pub const BOOT_FLAG_OFFSET: usize = 0x1fe;
pub const HEADER_MAGIC_OFFSET: usize = 0x202;
pub const HEADER_MAGIC: &[u8; 4] = b"HdrS";
pub const HEADER_VERSION_OFFSET: usize = 0x206;
pub const PAYLOAD_OFFSET_OFFSET: usize = 0x248;
pub const PAYLOAD_LENGTH_OFFSET: usize = 0x24c;
pub const INIT_SIZE_OFFSET: usize = 0x260;
pub const BOOT_FLAG: u16 = 0xaa55;
pub const MINIMUM_HEADER_VERSION: u16 = 0x0208;
pub const SETUP_SECTOR_SIZE: usize = 512;

// --- LZ4 legacy (the format `arch/x86/boot/compressed` produces) -----------
pub const LZ4_LEGACY_MAGIC: u32 = 0x184c_2102;
pub const LZ4_BLOCK_SIZE: usize = 8 << 20;
pub const GZIP_MAGIC: &[u8; 2] = b"\x1f\x8b";

/// Margin the kernel reserves for in-place decompression; see the long comment
/// in `arch/x86/boot/header.S` (`extra_bytes = (uncompressed >> 8) + 131072`).
pub const INIT_SIZE_MARGIN_SHIFT: usize = 8;
pub const INIT_SIZE_MARGIN_FIXED: usize = 128 * 1024;

/// `int3` padding the compiler emits between (and after) functions.
pub const TEXT_PADDING_BYTE: u8 = 0xcc;

/// `early_reserve_memory()` materialises the kernel reservation size as one
/// 32 bit immediate, which the injection rewrites.
pub const RESERVE_IMMEDIATE_LENGTH: usize = 4;

/// `-__START_KERNEL_map` modulo 2^64, i.e. `__pa_symbol(x) = x + this` for a
/// kernel image address.  `__START_KERNEL_map` is `0xffffffff80000000` on
/// x86-64, so the delta is `0x80000000`; the bootstrap loads it with `movl` so
/// that the assembler zero-extends it instead of sign-extending an `imm32`.
pub const X86_KERNEL_MAP_DELTA: u64 = 0x8000_0000;

/// A `bzImage` is a `bzImage` when the boot flag and the `HdrS` signature are
/// present; the compressed payload is stored at `setup_size + payload_offset`.
pub fn is_x86_bz_image(data: &[u8]) -> bool {
    data.len() >= 0x208 + 4
        && read_u16(data, BOOT_FLAG_OFFSET).ok() == Some(BOOT_FLAG)
        && data.get(HEADER_MAGIC_OFFSET..HEADER_MAGIC_OFFSET + 4) == Some(HEADER_MAGIC.as_slice())
}

#[derive(Clone, Copy, Debug)]
pub struct BzImageHeader {
    setup_size: usize,
    payload_offset: usize,
    payload_length: usize,
    init_size: usize,
    syssize: usize,
    version: u16,
}

impl BzImageHeader {
    fn parse(data: &[u8]) -> Result<Self> {
        ensure!(is_x86_bz_image(data), "kernel input is not an x86 bzImage");
        let version = read_u16(data, HEADER_VERSION_OFFSET)?;
        ensure!(
            version >= MINIMUM_HEADER_VERSION,
            "unsupported x86 boot protocol 0x{version:04x}; boot protocol 2.08 or newer is required"
        );
        let setup_size = (usize::from(data[SETUP_SECTS_OFFSET]) + 1) * SETUP_SECTOR_SIZE;
        let payload_offset = usize::try_from(read_u32(data, PAYLOAD_OFFSET_OFFSET)?)?;
        let payload_length = usize::try_from(read_u32(data, PAYLOAD_LENGTH_OFFSET)?)?;
        let init_size = usize::try_from(read_u32(data, INIT_SIZE_OFFSET)?)?;
        let syssize = usize::try_from(read_u32(data, SYSSIZE_OFFSET)?)?;
        let payload_start = setup_size
            .checked_add(payload_offset)
            .context("bzImage payload offset overflow")?;
        let payload_end = payload_start
            .checked_add(payload_length)
            .context("bzImage payload length overflow")?;
        ensure!(
            setup_size > 0 && payload_length > 0,
            "bzImage has no setup area or no payload"
        );
        ensure!(
            payload_end <= data.len(),
            "bzImage payload 0x{payload_start:x}..0x{payload_end:x} is outside the file ({} bytes)",
            data.len()
        );
        ensure!(
            // `syssize` counts 16-byte paragraphs and may round the protected
            // mode blob up.
            syssize.saturating_mul(16) <= (data.len() - setup_size) + 16,
            "bzImage syssize is larger than the protected-mode blob"
        );
        Ok(Self {
            setup_size,
            payload_offset,
            payload_length,
            init_size,
            syssize,
            version,
        })
    }

    const fn payload_start(&self) -> usize {
        self.setup_size + self.payload_offset
    }

    fn blob<'a>(&self, data: &'a [u8]) -> &'a [u8] {
        &data[self.setup_size..]
    }
}

// --- payload codec ---------------------------------------------------------

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PayloadFormat {
    Lz4Legacy,
    Gzip,
}

pub fn detect_payload_format(payload: &[u8]) -> Result<PayloadFormat> {
    if payload.len() >= 4 && read_u32(payload, 0)? == LZ4_LEGACY_MAGIC {
        Ok(PayloadFormat::Lz4Legacy)
    } else if payload.starts_with(GZIP_MAGIC) {
        Ok(PayloadFormat::Gzip)
    } else {
        bail!(
            "unsupported x86-64 kernel payload compression (magic {:02x?}); only LZ4 and gzip are supported",
            get_slice(payload, 0, 4.min(payload.len()))?
        )
    }
}

pub fn decompress_lz4_legacy(payload: &[u8]) -> Result<Vec<u8>> {
    ensure!(
        payload.len() > 4 && read_u32(payload, 0)? == LZ4_LEGACY_MAGIC,
        "invalid LZ4 legacy payload"
    );
    let trailer = payload.len() - 4;
    let total = usize::try_from(read_u32(payload, trailer)?).context("LZ4 output size overflow")?;
    let mut output = Vec::with_capacity(total);
    let mut buffer = vec![0u8; LZ4_BLOCK_SIZE];
    let mut position = 4;
    while position + 4 <= trailer {
        let compressed_size = usize::try_from(read_u32(payload, position)?)?;
        if compressed_size == 0 {
            break;
        }
        position += 4;
        let block = get_slice(payload, position, compressed_size)?;
        position = position
            .checked_add(compressed_size)
            .context("LZ4 block offset overflow")?;
        ensure!(position <= trailer, "LZ4 block crosses the payload trailer");
        let want = (total - output.len()).min(LZ4_BLOCK_SIZE);
        ensure!(want > 0, "LZ4 payload has more data than its trailer");
        let produced = lz4::block::decompress_to_buffer(block, Some(want as i32), &mut buffer)
            .context("cannot decode LZ4 block")?;
        ensure!(
            produced <= want,
            "LZ4 block expands past the declared output size"
        );
        output.extend_from_slice(&buffer[..produced]);
    }
    ensure!(
        output.len() == total,
        "LZ4 payload decoded to 0x{:x} bytes, expected 0x{total:x}",
        output.len()
    );
    Ok(output)
}

pub fn compress_lz4_legacy(content: &[u8]) -> Result<Vec<u8>> {
    let bound = lz4::block::compress_bound(LZ4_BLOCK_SIZE).context("LZ4 block bound overflow")?;
    let mut scratch = vec![0u8; bound];
    let mut output = Vec::with_capacity(content.len() / 2);
    output.extend_from_slice(&LZ4_LEGACY_MAGIC.to_le_bytes());
    for chunk in content.chunks(LZ4_BLOCK_SIZE) {
        let compressed = lz4::block::compress_to_buffer(
            chunk,
            Some(lz4::block::CompressionMode::HIGHCOMPRESSION(12)),
            false,
            &mut scratch,
        )
        .context("cannot encode LZ4 block")?;
        output.extend_from_slice(&u32::try_from(compressed)?.to_le_bytes());
        output.extend_from_slice(&scratch[..compressed]);
    }
    let total =
        u32::try_from(content.len()).context("kernel image does not fit the LZ4 trailer")?;
    output.extend_from_slice(&total.to_le_bytes());
    Ok(output)
}

pub fn decompress_gzip(payload: &[u8]) -> Result<Vec<u8>> {
    use std::io::Read;

    let mut decoder = flate2::read::MultiGzDecoder::new(payload);
    let mut output = Vec::new();
    decoder
        .read_to_end(&mut output)
        .context("cannot decompress the gzip kernel payload")?;
    Ok(output)
}

pub fn compress_gzip(content: &[u8]) -> Result<Vec<u8>> {
    use std::io::Write;

    let mut encoder = flate2::write::GzEncoder::new(Vec::new(), flate2::Compression::best());
    encoder
        .write_all(content)
        .context("cannot compress the gzip kernel payload")?;
    encoder.finish().context("cannot finish the gzip payload")
}

pub fn decompress_payload(format: PayloadFormat, payload: &[u8]) -> Result<Vec<u8>> {
    match format {
        PayloadFormat::Lz4Legacy => decompress_lz4_legacy(payload),
        PayloadFormat::Gzip => decompress_gzip(payload),
    }
}

pub fn compress_payload(format: PayloadFormat, content: &[u8]) -> Result<Vec<u8>> {
    match format {
        PayloadFormat::Lz4Legacy => compress_lz4_legacy(content),
        PayloadFormat::Gzip => compress_gzip(content),
    }
}

/// The kernel relocation table lives at the very end of the decompressed image:
/// three zero-terminated lists of 32-bit sign-extended addresses, which
/// `handle_relocations()` walks backwards from the last byte.
pub fn relocation_table_range(content: &[u8]) -> Result<(usize, usize)> {
    let length = content.len();
    ensure!(length >= 16, "decompressed image is too small");
    let mut position = length;
    for list in 0..3 {
        ensure!(position >= 4, "truncated kernel relocation table");
        let mut count = 0usize;
        while position >= 4 && read_i32(content, position - 4)? != 0 {
            position -= 4;
            count += 1;
        }
        ensure!(
            count > 0,
            "kernel relocation list {list} is empty; the image is not a relocatable kernel"
        );
        ensure!(
            position >= 4,
            "kernel relocation list {list} has no terminator"
        );
        position -= 4;
    }
    Ok((position, length))
}

// --- required symbols and patch sites -------------------------------------

#[derive(Clone, Copy, Debug)]
pub struct X86PatchSites {
    /// `kernel_init()` call to `async_synchronize_full()`.
    bootstrap_call: CallSite,
    /// `load_module()` call to `strndup_user()`.
    strndup_call: CallSite,
    /// Immediate operand of the `sub rsi, imm32` that computes the size of the
    /// kernel-image `memblock_reserve()` call.
    reserve_size_immediate: usize,
}

pub fn decode_call_rel32(content: &[u8], offset: usize, address: u64) -> Option<u64> {
    if *content.get(offset)? != 0xe8 {
        return None;
    }
    let displacement = i64::from(read_i32(content, offset + 1).ok()?);
    address.checked_add_signed(5 + displacement)
}

/// `call rel32` (`e8`) is a signed 32 bit displacement after a 5 byte
/// instruction, and unlike `BL` it needs no alignment.
pub struct CallRel32;

impl BranchCodec for CallRel32 {
    const LENGTH: usize = 5;
    const ALIGNMENT: usize = 1;
    const NAME: &'static str = "call";

    fn decode(content: &[u8], offset: usize, address: u64) -> Option<u64> {
        decode_call_rel32(content, offset, address)
    }

    fn encode(source_address: u64, target_address: u64) -> Result<Vec<u8>> {
        let displacement = i128::from(target_address) - i128::from(source_address) - 5;
        let encoded = i32::try_from(displacement).with_context(|| {
            format!("relative branch 0x{source_address:x} -> 0x{target_address:x} is out of range")
        })?;
        let mut output = vec![0xe8, 0, 0, 0, 0];
        output[1..].copy_from_slice(&encoded.to_le_bytes());
        Ok(output)
    }
}

/// `early_reserve_memory()` reserves `[_text, __end_of_kernel_reserve)` with
/// `memblock_reserve(__pa_symbol(_text), __end_of_kernel_reserve - _text)`.
///
/// The size is a compile-time constant materialised either as
/// `sub rsi, imm32(_text)` (GCC) or as `mov rax, imm32(_text); sub rax, rsi`
/// (Clang), so growing the reservation is a single immediate rewrite.
pub fn find_reserve_size_immediate(
    layout: &KernelImageLayout,
    content: &[u8],
    required: &RequiredSymbols,
) -> Result<usize> {
    let image_base = required.image_base();
    let text_low = (image_base as u32).to_le_bytes();
    let reserve_end_low = (required.get("__end_of_kernel_reserve").address as u32).to_le_bytes();
    let mut calls = Vec::new();
    for offset in 0..content.len().saturating_sub(5) {
        let Some(address) = layout.address_of(offset, image_base) else {
            continue;
        };
        if decode_call_rel32(content, offset, address)
            == Some(required.get("memblock_reserve").address)
        {
            calls.push(offset);
        }
    }
    ensure!(
        !calls.is_empty(),
        "cannot find the kernel-image memblock_reserve() call"
    );

    let mut candidates = Vec::new();
    for &call_offset in &calls {
        let window_start = call_offset.saturating_sub(32);
        let window = &content[window_start..call_offset];
        let Some(reserve_end_position) = window
            .windows(4)
            .position(|candidate| candidate == reserve_end_low.as_slice())
        else {
            continue;
        };
        let mut immediates = Vec::new();
        for position in reserve_end_position + 4..window.len().saturating_sub(4) {
            if window[position..position + 4] != text_low {
                continue;
            }
            let prefix = &window[..position];
            if prefix.ends_with(&[0x48, 0x81, 0xee])
                || prefix.ends_with(&[0x48, 0xc7, 0xc0])
                || prefix.ends_with(&[0x48, 0x81, 0xc0])
            {
                immediates.push(window_start + position);
            }
        }
        ensure!(
            immediates.len() <= 1,
            "ambiguous kernel reservation size ({} candidates near 0x{call_offset:x})",
            immediates.len()
        );
        let Some(immediate) = immediates.pop() else {
            continue;
        };
        // The argument setup must belong to *this* call.
        let previous_call = calls
            .iter()
            .copied()
            .filter(|&call| call < call_offset)
            .max();
        if previous_call.is_none_or(|previous| previous < immediate) {
            candidates.push(immediate);
        }
    }

    ensure!(
        candidates.len() == 1,
        "cannot uniquely identify the kernel-image memblock_reserve() size ({} matches)",
        candidates.len()
    );
    Ok(candidates[0])
}

pub fn analyze_patch_sites(
    layout: &KernelImageLayout,
    content: &[u8],
    symbols: &SymbolMap,
    required: &RequiredSymbols,
) -> Result<X86PatchSites> {
    let image_base = required.image_base();
    let mut async_targets = symbol_addresses(
        symbols,
        &[
            "async_synchronize_full",
            "async_synchronize_cookie_domain",
            "async_synchronize_full_domain",
        ],
    );
    async_targets.insert(required.get("async_synchronize_full").address);
    let mut strndup_targets = symbol_addresses(symbols, &["strndup_user"]);
    strndup_targets.insert(required.get("strndup_user").address);

    let bootstrap_call = find_unique_direct_call::<KernelImageLayout, CallRel32>(
        layout,
        content,
        symbols,
        image_base,
        required.get("kernel_init"),
        &async_targets,
        "kernel_init async call",
        0x4000,
    )?;
    let strndup_call = find_unique_direct_call::<KernelImageLayout, CallRel32>(
        layout,
        content,
        symbols,
        image_base,
        required.get("load_module"),
        &strndup_targets,
        "load_module strndup_user call",
        0x10000,
    )?;
    let reserve_size_immediate = find_reserve_size_immediate(layout, content, required)?;

    Ok(X86PatchSites {
        bootstrap_call,
        strndup_call,
        reserve_size_immediate,
    })
}

// --- kallsyms / BTF recovery ----------------------------------------------
pub fn recover_kernel_release(
    layout: &KernelImageLayout,
    content: &[u8],
    image_base: u64,
    linux_banner: &MapSymbol,
    btf: Option<&KernelBtf>,
) -> Result<(String, GkiAbi)> {
    let offset = layout.offset_of(linux_banner.address, image_base, 0, "linux_banner")?;
    let bounded = content
        .get(offset..offset.saturating_add(1024).min(content.len()))
        .context("linux_banner is outside the decompressed image")?;
    let end = bounded
        .iter()
        .position(|byte| *byte == 0)
        .context("linux_banner is not a bounded C string")?;
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

// --- stub data relocation --------------------------------------------------
pub const RIP_RELATIVE_MODRM: fn(u8) -> bool = |byte| byte < 0x40 && byte & 7 == 5;

/// `mod=10`, `rm=011`/`rm=101`: `[rbx+disp32]` / `[rbp+disp32]`, the base
/// registers the compressed stub uses for its link-time offsets.
pub const BASE_RELATIVE_MODRM: fn(u8) -> bool =
    |byte| (0x80..0xc0).contains(&byte) && matches!(byte & 7, 3 | 5);

/// A displacement only counts as a link-time offset when it really is the
/// operand of a memory reference: the byte before the ModRM must be a plausible
/// opcode (optionally preceded by a REX prefix).  Without this check the opcode
/// byte of an unrelated instruction can be mistaken for a ModRM byte.
pub fn is_memory_operand(blob: &[u8], modrm_position: usize) -> bool {
    if modrm_position == 0 {
        return false;
    }
    let mut index = modrm_position - 1;
    if (0x40..=0x4f).contains(&blob[index]) {
        if index == 0 {
            return false;
        }
        index -= 1;
    }
    matches!(
        blob[index],
        0x01 | 0x03
            | 0x09
            | 0x0b
            | 0x21
            | 0x23
            | 0x29
            | 0x2b
            | 0x31
            | 0x33
            | 0x39
            | 0x3b
            | 0x81
            | 0x83
            | 0x85
            | 0x87
            | 0x88
            | 0x89
            | 0x8a
            | 0x8b
            | 0x8d
            | 0xc7
            | 0xff
    )
}

/// Legacy prefixes that may sit in front of the opcode of a stub instruction.
pub const LEGACY_PREFIX: fn(u8) -> bool = |byte| {
    matches!(
        byte,
        0x26 | 0x2e | 0x36 | 0x3e | 0x64 | 0x65 | 0x66 | 0x67 | 0xf0 | 0xf2 | 0xf3
    )
};

/// Position of the opcode byte of the instruction whose ModRM byte lives at
/// `modrm_position`, skipping any legacy prefixes and a REX prefix.
pub fn stub_opcode_position(blob: &[u8], modrm_position: usize) -> usize {
    let mut index = modrm_position;
    while index > 0 {
        index -= 1;
        if LEGACY_PREFIX(blob[index]) || (0x40..=0x4f).contains(&blob[index]) {
            continue;
        }
        return index;
    }
    0
}

/// Size of the immediate operand that follows the ModRM/SIB/displacement bytes
/// of the instruction starting at `opcode_position`.
///
/// This matters for the RIP-relative target: the compressed stub contains
/// `cmp $imm32, disp32(%rip)` (`81 /7`) and `mov $imm32, disp32(%rip)`
/// (`c7 /0`), and for those the displacement is resolved against the end of the
/// whole instruction, not against the end of the displacement field.
pub fn trailing_immediate_size(blob: &[u8], opcode_position: usize) -> usize {
    let opcode = blob[opcode_position];
    if opcode == 0x0f {
        let Some(&second) = blob.get(opcode_position + 1) else {
            return 0;
        };
        return usize::from(matches!(
            second,
            0x70 | 0x71 | 0x72 | 0x73 | 0xa4 | 0xac | 0xba | 0xc2 | 0xc4 | 0xc5 | 0xc6
        ));
    }
    match opcode {
        0x80 | 0x83 | 0xc0 | 0xc1 | 0xc6 | 0x6b | 0xf6 => 1,
        0x69 | 0x81 | 0xc7 | 0xf7 => 4,
        _ => 0,
    }
}

/// Target of the RIP-relative operand whose `disp32` starts at `position`.
pub fn rip_relative_target(blob: &[u8], position: usize) -> Result<u64> {
    let displacement = i64::from(read_i32(blob, position)?);
    let opcode_position = stub_opcode_position(blob, position - 1);
    let immediate = trailing_immediate_size(blob, opcode_position);
    let instruction_end = position
        .checked_add(4)
        .and_then(|end| end.checked_add(immediate))
        .context("RIP-relative instruction length overflow")?;
    Ok((instruction_end as i64 + displacement) as u64)
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ReferenceKind {
    /// `disp32` of a RIP-relative operand, applied at `position`.
    RipRelative,
    /// `disp32` of a `[rbx+disp32]`/`[rbp+disp32]` operand.
    BaseRelative,
    /// `rel32` of a direct `call`/`jmp`/`jcc`, applied at `position`.
    RelativeBranch,
}

/// Every reference that crosses the payload boundary has to be adjusted when
/// the stub data after the payload moves.  References inside the head, inside
/// the payload and inside the tail keep their displacement.
pub fn collect_stub_references(
    blob: &[u8],
    payload_offset: usize,
    payload_length: usize,
    stub_end: usize,
) -> Result<Vec<(usize, ReferenceKind)>> {
    let tail_start = payload_offset + payload_length;
    ensure!(
        stub_end > tail_start,
        "compressed stub end 0x{stub_end:x} is not past the payload; unsupported layout"
    );
    let mut references = Vec::new();

    let mut push = |position: usize, kind: ReferenceKind| {
        references.push((position, kind));
    };

    // Head: references into the relocated tail/bss.
    for position in 1..payload_offset.saturating_sub(4) {
        if RIP_RELATIVE_MODRM(blob[position - 1]) && is_memory_operand(blob, position - 1) {
            let target = rip_relative_target(blob, position)?;
            if target >= tail_start as u64 && target < stub_end as u64 {
                push(position, ReferenceKind::RipRelative);
                continue;
            }
        }
        if BASE_RELATIVE_MODRM(blob[position - 1]) && is_memory_operand(blob, position - 1) {
            let value = u64::from(read_u32(blob, position)?);
            if value >= tail_start as u64 && value < stub_end as u64 {
                push(position, ReferenceKind::BaseRelative);
                continue;
            }
        }
        if blob[position] == 0xe8 || blob[position] == 0xe9 {
            let displacement = i64::from(read_i32(blob, position + 1)?);
            let target = (position as i64 + 5 + displacement) as u64;
            if target >= tail_start as u64 && target < stub_end as u64 {
                push(position + 1, ReferenceKind::RelativeBranch);
                continue;
            }
        }
        if blob[position] == 0x0f && blob[position + 1] & 0xf0 == 0x80 {
            let displacement = i64::from(read_i32(blob, position + 2)?);
            let target = (position as i64 + 6 + displacement) as u64;
            if target >= tail_start as u64 && target < stub_end as u64 {
                push(position + 2, ReferenceKind::RelativeBranch);
            }
        }
    }

    // Tail: references back into the head or the payload, which do not move.
    for position in tail_start + 1..blob.len().saturating_sub(4) {
        if RIP_RELATIVE_MODRM(blob[position - 1]) && is_memory_operand(blob, position - 1) {
            let target = rip_relative_target(blob, position)?;
            // Position independent stub code only refers back to the start of
            // the blob (`startup_32`) or to the compressed payload
            // (`input_data`, `input_data + 4`); arbitrary low addresses are
            // byte-pattern false positives.
            if target == 0 || (target >= payload_offset as u64 && target <= tail_start as u64) {
                push(position, ReferenceKind::RipRelative);
            }
        }
        // The tail is position independent: `%rbx`-based link-time offsets only
        // appear in the stub head, so base-relative references are not scanned
        // here (byte patterns mislead far too often in 32 KiB of code).
        // Relative branches are not scanned here: the tail is compiled
        // position independently, so it only reaches the stub head and the
        // payload through RIP-relative operands, and a stray 0xe8/0x0f byte
        // inside an unrelated instruction would be indistinguishable from a
        // real branch.
    }

    Ok(references)
}

/// Location of the immediate operand of the stub's `movl $(len), %ecx` that
/// feeds `rep movsq`.
pub fn find_stub_copy_count(blob: &[u8]) -> Result<usize> {
    let marker = find_subslice(blob, &[0xfd, 0xf3, 0x48, 0xa5], 0)
        .context("cannot find the compressed stub's relocation copy loop")?;
    let mut shift = None;
    for position in marker.saturating_sub(12)..marker {
        if blob[position..position + 3] == [0xc1, 0xe9, 0x03] {
            shift = Some(position);
            break;
        }
    }
    let shift = shift.context("cannot find the compressed stub's copy length shift")?;
    let opcode = shift
        .checked_sub(5)
        .context("compressed stub copy length is too close to the blob start")?;
    ensure!(
        blob[opcode] == 0xb9,
        "unexpected compressed stub copy length instruction"
    );
    // `movl $imm32, %ecx` is one opcode byte followed by the immediate.
    Ok(opcode + 1)
}

/// Positions of the immediates of the `subl $_end, %ebx` instructions in the
/// stub head.  The stub subtracts the compressed kernel's `_end` from
/// `init_size + load address` to find its relocation base, so the value has to
/// move together with the stub data that `_end` bounds.
pub fn find_stub_end_immediates(blob: &[u8], payload_offset: usize) -> Result<Vec<usize>> {
    let mut positions = Vec::new();
    for position in 0..payload_offset.saturating_sub(6) {
        if blob[position] == 0x81 && blob[position + 1] == 0xeb {
            let value = usize::try_from(read_u32(blob, position + 2)?)?;
            if value > payload_offset {
                positions.push(position + 2);
            }
        }
    }
    ensure!(
        !positions.is_empty(),
        "cannot recover the compressed stub's _end"
    );
    Ok(positions)
}

/// The stub stores `_end - startup_32` in `%ebx`; `_end` is the end of the
/// stub's own image and bounds every link-time offset.
pub fn find_stub_end(blob: &[u8], payload_offset: usize) -> Result<usize> {
    let mut matches = Vec::new();
    for position in find_stub_end_immediates(blob, payload_offset)? {
        matches.push(usize::try_from(read_u32(blob, position)?)?);
    }
    matches.sort_unstable();
    matches.dedup();
    let candidate = matches
        .first()
        .copied()
        .context("cannot recover the compressed stub's _end")?;
    ensure!(
        matches.len() == 1,
        "ambiguous compressed stub _end ({} candidates)",
        matches.len()
    );
    Ok(candidate)
}

/// `input_len`/`output_len` are two adjacent 32-bit values in the stub's
/// rodata: the compressed payload size and the decompressed image size.
pub fn find_stub_lengths(
    blob: &[u8],
    payload_offset: usize,
    payload_length: usize,
    old_content_length: usize,
) -> Result<usize> {
    let mut needle = Vec::with_capacity(8);
    needle.extend_from_slice(&u32::try_from(payload_length)?.to_le_bytes());
    needle.extend_from_slice(&u32::try_from(old_content_length)?.to_le_bytes());
    let mut matches = Vec::new();
    let mut search_from = payload_offset + payload_length;
    while let Some(position) = find_subslice(blob, &needle, search_from) {
        matches.push(position);
        search_from = position + 1;
    }
    ensure!(
        matches.len() == 1,
        "cannot uniquely locate the compressed stub's input_len/output_len ({} matches)",
        matches.len()
    );
    Ok(matches[0])
}

/// Round `value` up to the next multiple of `alignment`.
///
/// The stub data that follows the payload carries linker guarantees that a byte
/// exact shift would destroy: `pgtable` has to stay 4 KiB aligned because the
/// boot stub loads it into `CR3`, and `_bss` has to stay 8 byte aligned because
/// the relocation copy moves it with `rep movsq`.  Rounding the move to a whole
/// page keeps every one of them intact.
///
/// Rounding is towards positive infinity, so a shrinking payload shifts the
/// tail by no more than it shrank and `shift - value` stays non-negative.
pub const fn align_up_signed(value: i64, alignment: i64) -> i64 {
    let remainder = value.rem_euclid(alignment);
    if remainder == 0 {
        value
    } else {
        value + (alignment - remainder)
    }
}

/// Rebuild the `bzImage` with a longer or shorter payload, moving the stub data
/// that follows it and fixing every reference that crosses the boundary.
pub fn build_bz_image(
    original: &[u8],
    header: &BzImageHeader,
    new_payload: &[u8],
    new_content_length: usize,
) -> Result<Vec<u8>> {
    let blob = header.blob(original);
    let old_payload_length = header.payload_length;
    let delta = new_payload.len() as i64 - old_payload_length as i64;
    // Shift the stub data by a whole number of pages and turn the remainder
    // into padding between the payload and the tail.  Whole pages, because the
    // stub's `pgtable` is loaded into `CR3` and x86 page tables are 4 KiB
    // based, whatever page size userspace is told it has.
    let shift = align_up_signed(delta, X86_PAGE_SIZE as i64);
    let padding = usize::try_from(shift - delta).context("stub padding overflow")?;
    let payload_offset = header.payload_offset;
    let tail_start = payload_offset + old_payload_length;
    ensure!(tail_start <= blob.len(), "bzImage tail is outside the blob");

    let stub_end = find_stub_end(blob, payload_offset)?;
    let old_content_length =
        usize::try_from(read_u32(blob, payload_offset + old_payload_length - 4)?)?;
    let lengths_position =
        find_stub_lengths(blob, payload_offset, old_payload_length, old_content_length)?;
    let copy_count_position = find_stub_copy_count(blob)?;
    ensure!(
        copy_count_position < payload_offset,
        "compressed stub copy loop is not in the stub head"
    );
    let copy_count = usize::try_from(read_u32(blob, copy_count_position)?)?;

    let references = collect_stub_references(blob, payload_offset, old_payload_length, stub_end)?;

    // Assemble the new blob first: head, new payload, alignment padding, then
    // the relocated tail.
    let mut new_blob = Vec::with_capacity(blob.len() + padding);
    new_blob.extend_from_slice(&blob[..payload_offset]);
    new_blob.extend_from_slice(new_payload);
    new_blob.resize(new_blob.len() + padding, 0);
    new_blob.extend_from_slice(&blob[tail_start..]);

    // Map an old blob offset to its offset in the new blob.
    let map_offset = |position: usize| -> Result<usize> {
        let mapped = if position < payload_offset {
            position as i64
        } else if position >= tail_start {
            position as i64 + shift
        } else {
            bail!("reference inside the compressed payload");
        };
        usize::try_from(mapped).context("relocated stub offset does not fit this host")
    };

    for (position, kind) in references {
        let new_position = map_offset(position)?;
        match kind {
            ReferenceKind::RipRelative => {
                let displacement = i64::from(read_i32(blob, position)?);
                let target = rip_relative_target(blob, position)?;
                let adjustment = if target >= tail_start as u64 {
                    shift
                } else {
                    -shift
                };
                let updated = i32::try_from(displacement + adjustment)
                    .context("relocated displacement is out of range")?;
                write_u32(&mut new_blob, new_position, updated as u32)?;
            }
            ReferenceKind::BaseRelative => {
                let value = u64::from(read_u32(blob, position)?);
                let adjusted = if value >= tail_start as u64 {
                    i128::from(value) + i128::from(shift)
                } else {
                    i128::from(value) - i128::from(shift)
                };
                write_u32(
                    &mut new_blob,
                    new_position,
                    u32::try_from(adjusted).context("stub offset overflow")?,
                )?;
            }
            ReferenceKind::RelativeBranch => {
                let displacement = i64::from(read_i32(blob, position)?);
                let opcode_position = position - 1;
                let length = if blob[opcode_position] == 0x0f { 6 } else { 5 };
                let target = (opcode_position as i64 + length + displacement) as u64;
                let adjustment = if target >= tail_start as u64 {
                    shift
                } else {
                    -shift
                };
                let updated = i32::try_from(displacement + adjustment)
                    .context("relocated branch displacement is out of range")?;
                write_u32(&mut new_blob, new_position, updated as u32)?;
            }
        }
    }

    // The copy loop must move the whole relocated blob, including the stub data
    // that now extends past the original `_bss`.
    let new_copy_count = i64::from(copy_count as i32) + shift;
    ensure!(
        new_copy_count >= 0,
        "relocated stub copy length is negative"
    );
    let new_copy_count = ((new_copy_count + 7) / 8) * 8;
    write_u32(
        &mut new_blob,
        copy_count_position,
        u32::try_from(new_copy_count)?,
    )?;

    // `_end` bounds the stub image, so it grows with the relocated stub data.
    // Leaving it behind would push the relocation base past the buffer the boot
    // loader sized with `init_size`.
    for position in find_stub_end_immediates(blob, payload_offset)? {
        let value = i64::from(read_i32(blob, position)?);
        let updated = i32::try_from(value + shift).context("stub _end is out of range")?;
        write_u32(&mut new_blob, position, updated as u32)?;
    }

    // Patch the stub's own view of the payload and image sizes.
    let new_lengths_position = map_offset(lengths_position)?;
    write_u32(
        &mut new_blob,
        new_lengths_position,
        u32::try_from(new_payload.len())?,
    )?;
    write_u32(
        &mut new_blob,
        new_lengths_position + 4,
        u32::try_from(new_content_length)?,
    )?;

    // Rebuild the file and update the setup header.
    let mut image = original.to_vec();
    image.truncate(header.setup_size);
    image.extend_from_slice(&new_blob);

    let new_payload_length = new_payload.len();
    write_u32(
        &mut image,
        PAYLOAD_LENGTH_OFFSET,
        u32::try_from(new_payload_length)?,
    )?;
    let new_syssize = new_blob.len().div_ceil(16);
    write_u32(&mut image, SYSSIZE_OFFSET, u32::try_from(new_syssize)?)?;
    let margin = (new_content_length >> INIT_SIZE_MARGIN_SHIFT) + INIT_SIZE_MARGIN_FIXED;
    // Same page size as `arch/x86/boot/header.S` uses for `extra_bytes`, so
    // the boot loader reserves what the decompressor asks for.
    let minimum_init_size = align_up(
        new_content_length
            .checked_add(margin)
            .context("init_size overflow")?,
        X86_PAGE_SIZE as usize,
    );
    let new_init_size = header.init_size.max(minimum_init_size);
    write_u32(&mut image, INIT_SIZE_OFFSET, u32::try_from(new_init_size)?)?;
    let _ = header.version;
    Ok(image)
}

// --- injection -------------------------------------------------------------

/// The capsule goes right after `_end` and before the kernel relocation table.
pub fn capsule_offset(
    layout: &KernelImageLayout,
    required: &RequiredSymbols,
    relocation_start: usize,
) -> Result<usize> {
    let image_base = required.image_base();
    let image_end = usize::try_from(
        required
            .image_end()
            .checked_sub(image_base)
            .context("_end is below _text")?,
    )
    .context("kernel image size does not fit this host")?;
    ensure!(
        image_end == usize::try_from(layout.kernel_total_size)?,
        "kernel image size does not match the ELF layout"
    );
    // `_end` is a memory offset while `relocation_start` indexes `content`, so
    // the placement is expressed in the content's own coordinates.
    Ok(align_up(relocation_start.max(layout.content_end()), 16))
}

/// Extra bytes the kernel image reservation has to cover so that the capsule
/// survives until the bootstrap runs.
pub fn reservation_extension(required: &RequiredSymbols, capsule_end: usize) -> Result<usize> {
    let reserve_end = usize::try_from(
        required
            .get("__end_of_kernel_reserve")
            .address
            .checked_sub(required.image_base())
            .context("__end_of_kernel_reserve is below _text")?,
    )
    .context("kernel reservation size does not fit this host")?;
    let extension = capsule_end
        .checked_sub(reserve_end)
        .context("capsule is below the reserved kernel region")?;
    ensure!(
        i32::try_from(extension).is_ok(),
        "kernel reservation extension does not fit an immediate"
    );
    Ok(extension)
}

/// Names of the symbols whose bodies the kernel rewrites during early boot.
///
/// The SRSO/retbleed/retpoline mitigations *generate* thunks in those areas and
/// fill the remainder with `int3`, so a cave placed in their padding is gone by
/// the time the kernel calls it.  Every other `cc` run in `.text` is plain
/// alignment padding that nothing but the bootstrap executes.
pub fn is_runtime_patched_symbol(name: &str) -> bool {
    let name = name.trim_start_matches('_');
    ["srso", "retbleed", "retpoline", "thunk", "altinstr", "ibt"]
        .iter()
        .any(|marker| name.contains(marker))
}

/// Code the kernel rewrites during early boot.
///
/// `int3` padding looks free in the image, but the kernel copies real code into
/// parts of it: `apply_alternatives` writes replacement bodies over the recorded
/// sites, the retpoline/return/SRSO mitigations patch the addresses listed in
/// their site arrays, and kCFI builds indirect-call thunks.  A cave inside any
/// of those ranges is overwritten before the bootstrap is ever called, which is
/// exactly how `distribute_cfs_runtime` ended up executing `int3`.
pub struct BootPatchedRanges {
    ranges: Vec<(usize, usize)>,
}

impl BootPatchedRanges {
    /// Every recorded patch site, as content offsets.
    fn collect(
        layout: &KernelImageLayout,
        content: &[u8],
        symbols: &SymbolMap,
        image_base: u64,
    ) -> Self {
        let mut ranges = Vec::new();
        let offset = |name: &str| -> Option<usize> {
            let symbol = symbols.resolve(name).ok()?;
            layout.content_offset_of(symbol.address, image_base)
        };
        // The tables store the patched instruction as a displacement from the
        // entry's own address, so the site has to be resolved in address space
        // and then converted back to a content offset.
        let mut record = |entry: usize, displacement: i32, length: usize| {
            let Some(entry_address) = layout.address_of(entry, image_base) else {
                return;
            };
            let Some(site_address) = entry_address.checked_add_signed(i64::from(displacement))
            else {
                return;
            };
            let Some(site) = layout.content_offset_of(site_address, image_base) else {
                return;
            };
            ranges.push((site.saturating_sub(1), site + length + 1));
        };

        // `struct alt_instr` is 13 bytes and stores the patched instruction as a
        // 32-bit offset relative to the entry itself.
        if let (Some(start), Some(end)) = (
            offset("__alt_instructions"),
            offset("__alt_instructions_end"),
        ) {
            let mut entry = start;
            while entry + 13 <= end && entry + 13 <= content.len() {
                if let Ok(site) = read_i32(content, entry) {
                    let length = usize::from(content[entry + 10]);
                    record(entry, site, length);
                }
                entry += 13;
            }
        }

        // `struct jump_entry` (16 bytes) and `struct static_call_site` (8 bytes)
        // both start with the patched address as a relative 32-bit offset.
        for (first, last, size) in [
            ("__start___jump_table", "__stop___jump_table", 16usize),
            ("__start_static_call_sites", "__stop_static_call_sites", 8),
        ] {
            let (Some(start), Some(end)) = (offset(first), offset(last)) else {
                continue;
            };
            let mut entry = start;
            while entry + size <= end && entry + size <= content.len() {
                if let Ok(site) = read_i32(content, entry) {
                    record(entry, site, 8);
                }
                entry += size;
            }
        }

        // The mitigation site arrays are plain `s32` arrays of relative offsets.
        for prefix in ["__retpoline_sites", "__return_sites", "__srso_sites"] {
            let (Some(start), Some(end)) = (offset(prefix), offset(&format!("{prefix}_end")))
            else {
                continue;
            };
            let mut entry = start;
            while entry + 4 <= end && entry + 4 <= content.len() {
                if let Ok(site) = read_i32(content, entry) {
                    record(entry, site, 8);
                }
                entry += 4;
            }
        }

        ranges.sort_unstable();
        Self { ranges }
    }

    fn overlaps(&self, start: usize, end: usize) -> bool {
        self.ranges
            .iter()
            .any(|(low, high)| *low < end && start < *high)
    }
}

/// Find executable padding inside `.text` for the bootstrap.
///
/// `content` is the decompressed image, so the returned offset is a *content*
/// offset; `build_bootstrap` turns it back into a linked address.  Everything
/// in here therefore has to be converted with `KernelImageLayout`, which is the
/// only thing that knows how the image's segments are stored.
pub fn find_x86_text_cave(
    layout: &KernelImageLayout,
    content: &[u8],
    symbols: &SymbolMap,
    image_base: u64,
    text_start: &MapSymbol,
    text_end: &MapSymbol,
    required_size: usize,
) -> Result<(usize, usize)> {
    const GUARD: usize = 64;
    let start = layout.offset_of(text_start.address, image_base, 0, "_stext")?;
    let end = layout.offset_of(text_end.address, image_base, 0, "_etext")?;
    ensure!(
        start < end && end <= content.len() && required_size > 0,
        "cannot establish a permanent text range"
    );

    // Symbol boundaries.  The map is sorted, so a symbol ends where the next
    // one starts.  A symbol that merely *starts* before a padding run still
    // ends inside it, and a runtime patched one (`srso_alias_return_thunk`
    // spans most of a megabyte) covers the run completely.
    let mut starts: Vec<usize> = Vec::new();
    let mut patched: Vec<(usize, usize)> = Vec::new();
    for (index, entry) in symbols.entries.iter().enumerate() {
        let Some(offset) = layout.content_offset_of(entry.address, image_base) else {
            continue;
        };
        if offset < start || offset >= end {
            continue;
        }
        starts.push(offset);
        if !is_runtime_patched_symbol(&entry.name) {
            continue;
        }
        let next = symbols
            .entries
            .get(index + 1)
            .and_then(|entry| layout.content_offset_of(entry.address, image_base))
            .unwrap_or(end);
        if next > offset {
            patched.push((offset, next));
        }
    }
    starts.sort_unstable();
    patched.sort_unstable();
    let boot_patched = BootPatchedRanges::collect(layout, content, symbols, image_base);

    // Take the first gap between two symbols that is big enough.
    let mut index = start;
    while index < end {
        if content[index] != TEXT_PADDING_BYTE {
            index += 1;
            continue;
        }
        let run_start = index;
        while index < end && content[index] == TEXT_PADDING_BYTE {
            index += 1;
        }
        let run_end = index;

        // A run is split by the symbols that begin inside it; the usable
        // padding is one of those gaps, and it may sit *in front of* the first
        // symbol of the run (`__pick_eevdf` follows 0x225 bytes of padding).
        let mut bounds = vec![run_start];
        bounds.extend(
            starts
                .iter()
                .copied()
                .filter(|offset| *offset > run_start && *offset < run_end),
        );
        bounds.push(run_end);

        for pair in bounds.windows(2) {
            let (gap_start, gap_end) = (pair[0], pair[1]);
            let mut floor = gap_start;
            for &(symbol_start, symbol_end) in &patched {
                if symbol_end <= gap_start {
                    continue;
                }
                if symbol_start >= gap_end {
                    break;
                }
                floor = floor.max(symbol_end + GUARD);
            }
            let mut candidate = align_up(floor, 16);
            if candidate + required_size + GUARD <= gap_end {
                let page_aligned = align_up(candidate, 4096);
                if page_aligned + required_size + GUARD <= gap_end {
                    candidate = page_aligned;
                }
            }
            if candidate + required_size + GUARD <= gap_end
                && !boot_patched.overlaps(candidate, candidate + required_size)
            {
                return Ok((candidate, gap_end - candidate));
            }
        }
    }

    bail!("cannot find 0x{required_size:x} bytes of .text padding that no symbol occupies")
}

pub fn build_bootstrap(
    layout: &KernelImageLayout,
    content: &[u8],
    symbols: &SymbolMap,
    required: &RequiredSymbols,
    capsule_offset: usize,
    capsule_size: usize,
    capsule_module_offset: usize,
    capsule_fixup_offset: usize,
    capsule_load_args_offset: usize,
    module_size: usize,
    fixup_count: usize,
    reserve_extension: usize,
    abi: &GkiAbi,
) -> Result<(usize, Vec<u8>, u64, u64)> {
    let image_base = required.image_base();
    let bootstrap_object = BootstrapObject::parse(BOOTSTRAP_X86_64_OBJECT, EM_X86_64)
        .context("cannot parse the embedded x86-64 LKM bootstrap")?;
    let (code_offset, code_cave_size) = find_x86_text_cave(
        layout,
        content,
        symbols,
        image_base,
        required.get("_stext"),
        required.get("_etext"),
        bootstrap_object.image_size(),
    )
    .context("cannot find an executable cave for the x86-64 bootstrap")?;
    let code_address = layout
        .address_of(code_offset, image_base)
        .context("bootstrap cave is not backed by the decompressed image")?;
    let definitions = bootstrap_definitions(&BootstrapDefinitions {
        required,
        bootstrap_image_offset: code_address
            .checked_sub(image_base)
            .context("bootstrap cave is below the kernel base")?,
        capsule_offset,
        capsule_size,
        capsule_module_offset,
        capsule_fixup_offset,
        capsule_load_args_offset,
        module_size,
        fixup_count,
        reserve_extension,
        abi,
        printk_address: bootstrap_printk_address(required, &bootstrap_object, code_address)?,
    });
    let bootstrap = bootstrap_object
        .link(code_address, &definitions, Arch::X86_64)
        .context("cannot relocate the embedded x86-64 LKM bootstrap")?;
    ensure!(
        bootstrap.data.len() <= code_cave_size,
        "code cave needs 0x{:x} bytes, cave has 0x{code_cave_size:x}",
        bootstrap.data.len()
    );
    ensure!(
        bootstrap.entry_address == code_address,
        "linked bootstrap address does not match the selected text cave"
    );
    Ok((
        code_offset,
        bootstrap.data,
        bootstrap.entry_address,
        bootstrap.strndup_adapter_address,
    ))
}

/// Everything the x86-64 link definitions are derived from.
pub struct BootstrapDefinitions<'a> {
    pub required: &'a RequiredSymbols,
    /// The stub's own offset from the kernel base, i.e. where it is linked.
    pub bootstrap_image_offset: u64,
    pub capsule_offset: usize,
    pub capsule_size: usize,
    pub capsule_module_offset: usize,
    pub capsule_fixup_offset: usize,
    /// Capsule relative offset of the NUL terminated `load_module()` args.
    pub capsule_load_args_offset: usize,
    pub module_size: usize,
    pub fixup_count: usize,
    pub reserve_extension: usize,
    pub abi: &'a GkiAbi,
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

/// The definitions the embedded x86-64 bootstrap object is linked against.
pub fn bootstrap_definitions(context: &BootstrapDefinitions<'_>) -> BTreeMap<&'static str, u64> {
    let BootstrapDefinitions {
        required,
        bootstrap_image_offset,
        capsule_offset,
        capsule_size,
        capsule_module_offset,
        capsule_fixup_offset,
        capsule_load_args_offset,
        module_size,
        fixup_count,
        reserve_extension,
        abi,
        printk_address,
    } = *context;
    let mut definitions = BTreeMap::from(capsule_wire_definitions());
    definitions.insert(
        "ksu_ext_async_synchronize_full",
        required.get("async_synchronize_full").address,
    );
    definitions.insert("ksu_ext_vmalloc", required.get("vmalloc").address);
    definitions.insert("ksu_ext_memcpy", required.get("memcpy").address);
    definitions.insert("ksu_ext_load_module", required.get("load_module").address);
    definitions.insert("ksu_ext_kstrdup", required.get("kstrdup").address);
    definitions.insert("ksu_ext_strndup_user", required.get("strndup_user").address);
    // Boot-time logging: the caller picked printk or the cave-local no-op.
    definitions.insert("ksu_ext_printk", printk_address);
    definitions.insert(
        "ksu_ext_memblock_reserve",
        required.get("memblock_reserve").address,
    );
    definitions.insert("ksu_image_base", required.image_base());
    definitions.insert("ksu_bootstrap_image_offset", bootstrap_image_offset);
    definitions.insert("ksu_capsule_size", capsule_size as u64);
    definitions.insert("ksu_module_capsule_offset", capsule_module_offset as u64);
    definitions.insert("ksu_module_size", module_size as u64);
    definitions.insert("ksu_fixup_capsule_offset", capsule_fixup_offset as u64);
    definitions.insert(
        "ksu_load_args_capsule_offset",
        capsule_load_args_offset as u64,
    );
    definitions.insert("ksu_fixup_count", fixup_count as u64);
    definitions.insert("ksu_reserve_extension", reserve_extension as u64);
    definitions.insert("ksu_load_info_size", abi.load_info_storage_size);
    definitions.insert("ksu_load_info_hdr_offset", abi.load_info_hdr_offset);
    definitions.insert("ksu_load_info_len_offset", abi.load_info_len_offset);
    definitions.insert("ksu_gfp_kernel", abi.gfp_kernel);
    // The capsule is past `_end`, outside the kernel image mapping, so the
    // bootstrap reaches it through the direct map.  `ksu_capsule_image_offset`
    // is the capsule's offset from the kernel's physical base, and
    // `ksu_page_offset_base` is the (randomized) base of the direct map.
    definitions.insert("ksu_capsule_image_offset", capsule_offset as u64);
    definitions.insert("ksu_kernel_map_delta", X86_KERNEL_MAP_DELTA);
    definitions.insert(
        "ksu_page_offset_base",
        required.get("page_offset_base").address,
    );
    definitions.insert("ksu_phys_base", required.get("phys_base").address);
    definitions
}

/// Inject `module` into a boot image kernel (`bzImage`) and return the rebuilt
/// kernel plus a human readable report.
pub fn inject_image(
    original: &[u8],
    module: &[u8],
    load_args: &str,
    override_vermagic: bool,
) -> Result<(Vec<u8>, ImageInjectionReport)> {
    let header = BzImageHeader::parse(original)?;
    let payload = get_slice(original, header.payload_start(), header.payload_length)?;
    let format = detect_payload_format(payload)?;
    let content =
        decompress_payload(format, payload).context("cannot decompress the kernel payload")?;
    // Patching an already patched image starts from the original payload: the
    // record inside the old capsule says what it was, and it is carried into
    // the new one so repeated updates stay stable.
    let (content, replaced, original_container) = match undo_injection(&content)? {
        Some(undone) => (
            undone.content,
            Some(undone.replaced),
            Some(undone.original_container),
        ),
        None => (content, None, None),
    };

    let layout = KernelImageLayout::parse(&content)?;
    let recovered = recover_kernel_metadata(
        &content,
        usize::try_from(layout.kernel_total_size)
            .context("kernel image size does not fit this host")?,
        "x86-64 image",
    )?;
    let symbols = &recovered.kallsyms.symbols;
    let required = RequiredSymbols::resolve(REQUIRED_SYMBOLS, symbols)?;
    validate_symbols(&required, &layout)?;
    let (kernel_release, abi) = recover_kernel_release(
        &layout,
        &content,
        required.image_base(),
        required.get("linux_banner"),
        recovered.btf.as_ref(),
    )?;

    let (relocation_start, _) = relocation_table_range(&content)?;
    let image_base = required.image_base();
    let (module, vermagic) =
        override_module_vermagic(module, EM_X86_64, &kernel_release, override_vermagic)?;
    let (fixups, unresolved) =
        collect_module_fixups(&module, symbols, image_base, content.len(), EM_X86_64)?;
    let fixup_bytes = pack_fixups(&fixups)?;

    // The capsule lives just past `_end` and before the relocation table; the
    // reservation extension has to be derived from the final capsule size
    // because the capsule is aligned relative to its own offset.
    let capsule_offset = capsule_offset(&layout, &required, relocation_start)?;
    let sites = analyze_patch_sites(&layout, &content, symbols, &required)?;

    // What a restore has to put back, captured before anything is written.
    let mut original_sites = Vec::with_capacity(3);
    for (offset, length) in [
        (sites.bootstrap_call.file_offset, CallRel32::LENGTH),
        (sites.strndup_call.file_offset, CallRel32::LENGTH),
        (sites.reserve_size_immediate, RESERVE_IMMEDIATE_LENGTH),
    ] {
        original_sites.push(RestoreSite::new(
            offset,
            get_slice(&content, offset, length)?,
        )?);
    }
    let (cave_offset, cave_size) = find_x86_text_cave(
        &layout,
        &content,
        symbols,
        image_base,
        required.get("_stext"),
        required.get("_etext"),
        BootstrapObject::parse(BOOTSTRAP_X86_64_OBJECT, EM_X86_64)
            .context("cannot parse the embedded x86-64 LKM bootstrap")?
            .image_size(),
    )?;
    let restore_record = RestoreRecord::new(
        &content,
        original_container.unwrap_or(ContainerFields::X86_64 {
            syssize: u32::try_from(header.syssize).context("syssize does not fit")?,
            init_size: u32::try_from(header.init_size).context("init_size does not fit")?,
        }),
        original_sites,
        RestoreCave::new(cave_offset, cave_size, TEXT_PADDING_BYTE)?,
        capsule_offset,
    )?;
    let restore_bytes = restore_record.encode()?;

    let capsule = build_capsule_at(
        capsule_offset,
        content.len(),
        &module,
        &fixup_bytes,
        fixups.len(),
        load_args,
        Some(&restore_bytes),
    )?;
    let reserve_extension = reservation_extension(&required, capsule_offset + capsule.data.len())?;

    let (code_offset, bootstrap, bootstrap_address, strndup_adapter_address) = build_bootstrap(
        &layout,
        &content,
        symbols,
        &required,
        capsule_offset,
        capsule.data.len(),
        capsule.module_offset - capsule.file_offset,
        capsule.fixup_offset - capsule.file_offset,
        capsule.load_args_offset - capsule.file_offset,
        module.len(),
        fixups.len(),
        reserve_extension,
        &abi,
    )?;

    ensure!(
        code_offset == cave_offset,
        "internal error: the bootstrap was linked into a different cave"
    );

    // Patch the decompressed image.
    let mut patched = content.clone();
    get_slice_mut(&mut patched, code_offset, bootstrap.len())?.copy_from_slice(&bootstrap);
    check_non_overlapping(&[
        (code_offset, code_offset + bootstrap.len(), "bootstrap cave"),
        (
            sites.bootstrap_call.file_offset,
            sites.bootstrap_call.file_offset + CallRel32::LENGTH,
            "kernel_init patch",
        ),
        (
            sites.strndup_call.file_offset,
            sites.strndup_call.file_offset + CallRel32::LENGTH,
            "load_module patch",
        ),
        (
            sites.reserve_size_immediate,
            sites.reserve_size_immediate + RESERVE_IMMEDIATE_LENGTH,
            "reservation patch",
        ),
    ])?;

    write_branch::<CallRel32>(
        &mut patched,
        sites.bootstrap_call.file_offset,
        sites.bootstrap_call.address,
        bootstrap_address,
    )?;
    write_branch::<CallRel32>(
        &mut patched,
        sites.strndup_call.file_offset,
        sites.strndup_call.address,
        strndup_adapter_address,
    )?;
    let reserve_immediate = (image_base.wrapping_sub(reserve_extension as u64)) as u32;
    write_u32(
        &mut patched,
        sites.reserve_size_immediate,
        reserve_immediate,
    )?;

    // Insert the capsule before the kernel relocation table.
    let mut extended = Vec::with_capacity(patched.len() + capsule.data.len());
    extended.extend_from_slice(&patched[..relocation_start]);
    extended.resize(capsule_offset, 0);
    extended.extend_from_slice(&capsule.data);
    extended.extend_from_slice(&patched[relocation_start..]);

    let new_payload = compress_payload(format, &extended)?;
    let rebuilt = build_bz_image(original, &header, &new_payload, extended.len())?;

    let report = ImageInjectionReport {
        arch: Arch::X86_64,
        kernel_release,
        btf: None,
        replaced,
        kallsyms_layout: recovered.kallsyms.layout,
        kallsyms_count: recovered.kallsyms.count,
        bootstrap_offset: code_offset,
        bootstrap_size: bootstrap.len(),
        fixup_count: fixups.len(),
        unresolved,
        vermagic,
        details: InjectionDetails::X86_64 {
            capsule_offset,
            capsule_size: capsule.data.len(),
            reservation_extension: reserve_extension,
            payload_before: header.payload_length,
            payload_after: new_payload.len(),
            payload_format: format!("{format:?}"),
            content_before: content.len(),
            content_after: extended.len(),
            syssize: header.syssize,
        },
    };
    Ok((rebuilt, report))
}

/// Undo a `boot-patch-v2` injection on a `bzImage`.
///
/// The decompressed payload comes back byte for byte (the record's SHA-256 is
/// checked before anything is rebuilt), then the capsule is cut out from
/// between `_end` and the kernel relocation table and the `bzImage` is rebuilt.
/// The container itself is not byte-identical: the payload is compressed
/// again.
pub fn restore_image(original: &[u8]) -> Result<RestoredImage> {
    let header = BzImageHeader::parse(original)?;
    let payload = get_slice(original, header.payload_start(), header.payload_length)?;
    let format = detect_payload_format(payload)?;
    let content =
        decompress_payload(format, payload).context("cannot decompress the kernel payload")?;

    let patched = open_patched_image(&content, EM_X86_64, "the kernel payload")?;
    let image = undo_opened(&content, &patched)?;

    let new_payload = compress_payload(format, &image)?;
    let rebuilt = build_bz_image(original, &header, &new_payload, image.len())?;
    let new_header = BzImageHeader::parse(&rebuilt)?;

    let ContainerFields::X86_64 { syssize, init_size } = patched.record.container else {
        bail!("restore record is not an x86-64 record");
    };
    let new_init_size = u32::try_from(new_header.init_size).context("init_size does not fit")?;
    ensure!(
        new_init_size >= init_size,
        "the rebuilt bzImage reserves less memory (0x{new_init_size:x}) than the original (0x{init_size:x})"
    );

    Ok(RestoredImage {
        image: rebuilt,
        report: RestoreReport {
            arch: Arch::X86_64,
            capsule_offset: patched.capsule_offset,
            capsule_size: patched.capsule_size,
            patched_sites: patched.record.sites.len(),
            original_content_len: patched.record.original_content_len,
            details: RestoreDetails::X86_64 {
                payload_before: header.payload_length,
                payload_after: new_payload.len(),
                syssize_before: usize::try_from(syssize)?,
                syssize_after: new_header.syssize,
                init_size_before: init_size,
                init_size_after: new_init_size,
            },
        },
    })
}

/// Undo the injection the payload carries, so that patching always starts from
/// the original bytes.  `None` means the image was never patched.
fn undo_injection(content: &[u8]) -> Result<Option<Undone>> {
    match open_injection(content, EM_X86_64)? {
        Some(patched) => {
            let image = undo_opened(content, &patched)?;
            Ok(Some(Undone::new(image, &patched)))
        }
        None => Ok(None),
    }
}

/// The original payload behind an already located capsule, verified against the
/// record before it is returned.
fn undo_opened(content: &[u8], patched: &PatchedImage) -> Result<Vec<u8>> {
    let record = &patched.record;
    for site in record.sites.iter().take(2) {
        let range = site.range()?;
        ensure!(
            is_branch::<CallRel32>(content, range.start),
            "site 0x{:x} does not hold a call; the image is not patched with v2",
            site.offset
        );
    }

    let mut restored = content.to_vec();
    record.apply(&mut restored)?;

    // Removing the capsule has to leave exactly the recorded payload behind:
    // the kernel relocation table stays at the very end of the image, so the
    // tail behind the capsule is its original self.
    let capsule_end = patched
        .capsule_offset
        .checked_add(patched.capsule_size)
        .context("capsule end overflow")?;
    let original_len = usize::try_from(record.original_content_len)?;
    let table_length = restored
        .len()
        .checked_sub(capsule_end)
        .context("capsule ends past the decompressed image")?;
    let relocation_start = original_len
        .checked_sub(table_length)
        .context("the recorded image is shorter than its relocation table")?;
    ensure!(
        relocation_start <= patched.capsule_offset,
        "the capsule overlaps the kernel relocation table"
    );
    let mut image = Vec::with_capacity(original_len);
    image.extend_from_slice(&restored[..relocation_start]);
    image.extend_from_slice(&restored[capsule_end..]);
    ensure!(
        image.len() == original_len,
        "restored image is 0x{:x} bytes, the record says 0x{original_len:x}",
        image.len()
    );
    let (recovered_relocation_start, _) = relocation_table_range(&image)?;
    ensure!(
        recovered_relocation_start == relocation_start,
        "the restored image does not end with the kernel relocation table"
    );
    record.verify(&image)?;
    Ok(image)
}

#[cfg(test)]
pub mod tests {
    use super::*;

    use crate::lkm_image::bootstrap::assert_definition_contract;
    use crate::lkm_image::capsule::CAPSULE_MAGIC;
    use crate::lkm_image::x86_64_layout::ProgramSegment;

    /// `KSU_LKM_TEST_IMAGE` is a boot image or a raw kernel; the test only runs
    /// when the kernel inside it is a `bzImage`.
    fn test_fixtures() -> Option<(Vec<u8>, Vec<u8>)> {
        let kernel =
            crate::lkm_image::test_kernel_from(&std::env::var("KSU_LKM_TEST_IMAGE").ok()?)?;
        if !is_x86_bz_image(&kernel) {
            return None;
        }
        let module = std::fs::read(std::env::var("KSU_LKM_TEST_MODULE").ok()?).ok()?;
        Some((kernel, module))
    }

    fn decompressed(image: &[u8]) -> Vec<u8> {
        let header = BzImageHeader::parse(image).unwrap();
        let payload = get_slice(image, header.payload_start(), header.payload_length).unwrap();
        let format = detect_payload_format(payload).unwrap();
        decompress_payload(format, payload).unwrap()
    }

    /// See the arm64 counterpart: the definitions and the object have to
    /// agree, because a definition that never reaches the linker is exactly
    /// how the capsule magic used to end up wrong.
    #[test]
    fn bootstrap_definitions_match_the_object() {
        let object = BootstrapObject::parse(BOOTSTRAP_X86_64_OBJECT, EM_X86_64).unwrap();
        let required = RequiredSymbols::synthetic(REQUIRED_SYMBOLS);
        let definitions = bootstrap_definitions(&BootstrapDefinitions {
            required: &required,
            bootstrap_image_offset: 0x2000,
            capsule_offset: 0x31_0000,
            capsule_size: 0x4000,
            capsule_module_offset: 0x40,
            capsule_fixup_offset: 0x60,
            capsule_load_args_offset: 0x80,
            module_size: 0x100,
            fixup_count: 1,
            reserve_extension: 0x1000,
            abi: &GKI_ABI,
            printk_address: required.optional("_printk").expect("synthetic").address,
        });
        assert_definition_contract("x86-64", &object, &definitions);
        assert_ne!(definitions["ksu_ext_printk"], 0);

        // See the arm64 counterpart: a kernel without `printk` still has to
        // link, with the logger pointing at a `ret` inside the cave.
        let required = RequiredSymbols::synthetic_without_optionals(REQUIRED_SYMBOLS);
        let code_address = 0xffff_ff80_0000_0000;
        let printk_address = bootstrap_printk_address(&required, &object, code_address).unwrap();
        let definitions = bootstrap_definitions(&BootstrapDefinitions {
            required: &required,
            bootstrap_image_offset: 0x2000,
            capsule_offset: 0x31_0000,
            capsule_size: 0x4000,
            capsule_module_offset: 0x40,
            capsule_fixup_offset: 0x60,
            capsule_load_args_offset: 0x80,
            module_size: 0x100,
            fixup_count: 1,
            reserve_extension: 0x1000,
            abi: &GKI_ABI,
            printk_address,
        });
        assert!(required.optional("_printk").is_none());
        assert_definition_contract("x86-64 without printk", &object, &definitions);
        // The synthetic definitions in this test all live just above
        // 0xffff_ff80_0000_0000, so the link has to happen there for the
        // 32-bit PC-relative branch ranges to be meaningful.
        object
            .link(code_address, &definitions, Arch::X86_64)
            .expect("a kernel without printk has to stay injectable");
    }

    /// A RIP-relative operand has to be resolved against the end of the whole
    /// instruction.  The compressed stub contains
    /// `cmpl $0x184c2102, disp32(%rip)` (`81 /7`), whose displacement is
    /// followed by a four byte immediate; ignoring that immediate moved the
    /// target by four bytes, which dropped the `input_data` reference from the
    /// relocation list and left the stub reading the wrong address after the
    /// payload grew.
    #[test]
    fn rip_relative_target_accounts_for_trailing_immediate() {
        let mut blob = vec![0_u8; 0x40];

        // 81 3d <disp32> <imm32> = cmpl $0x184c2102, disp32(%rip)
        blob[0x10] = 0x81;
        blob[0x11] = 0x3d;
        let displacement: i32 = 0x20 - (0x11 + 1 + 4 + 4);
        blob[0x12..0x16].copy_from_slice(&displacement.to_le_bytes());
        blob[0x16..0x1a].copy_from_slice(&0x184c_2102_u32.to_le_bytes());
        assert_eq!(rip_relative_target(&blob, 0x12).unwrap(), 0x20);

        // 48 8d 3d <disp32> = lea disp32(%rip), %rdi: no trailing immediate.
        blob[0x20] = 0x48;
        blob[0x21] = 0x8d;
        blob[0x22] = 0x3d;
        let displacement: i32 = 0x30 - (0x22 + 1 + 4);
        blob[0x23..0x27].copy_from_slice(&displacement.to_le_bytes());
        assert_eq!(rip_relative_target(&blob, 0x23).unwrap(), 0x30);
    }

    /// A layout whose content offsets and memory offsets coincide, which is what
    /// the hand-built `content` buffers in these tests assume.
    fn identity_layout(size: usize) -> KernelImageLayout {
        KernelImageLayout {
            segments: vec![ProgramSegment {
                file_offset: 0,
                memory_offset: 0,
                file_size: size as u64,
                memory_size: size as u64,
            }],
            kernel_total_size: size as u64,
        }
    }

    /// A cave must not land inside a symbol that merely *starts* before the
    /// padding run.  `srso_alias_return_thunk` covers a megabyte of `cc`
    /// padding that the SRSO mitigation rewrites at boot, so a cave placed
    /// there is filled with `int3` before the kernel ever calls it.
    #[test]
    fn text_cave_avoids_runtime_patched_thunk_padding() {
        const BASE: u64 = 0xffff_ffff_8100_0000;
        let mut content = vec![0x90_u8; 0x5000];
        content[0x1000..0x2000].fill(TEXT_PADDING_BYTE);
        let symbols = SymbolMap::new(vec![
            MapSymbol {
                address: BASE + 0x800,
                name: "wide_thunk".into(),
            },
            MapSymbol {
                address: BASE + 0x1800,
                name: "after".into(),
            },
            MapSymbol {
                address: BASE + 0x3000,
                name: "later".into(),
            },
        ])
        .unwrap();
        let text_start = MapSymbol {
            address: BASE,
            name: "_stext".into(),
        };
        let text_end = MapSymbol {
            address: BASE + 0x4800,
            name: "_etext".into(),
        };
        let layout = identity_layout(content.len());
        let (cave, available) = find_x86_text_cave(
            &layout,
            &content,
            &symbols,
            BASE,
            &text_start,
            &text_end,
            0x100,
        )
        .unwrap();
        assert!(cave >= 0x800 + 64, "cave 0x{cave:x} overlaps wide_thunk");
        assert_eq!(cave + available, 0x2000);
    }

    /// The cave search works on the decompressed image, whose segments are not
    /// necessarily stored at their load offsets.  Treating a content offset as
    /// an address put the bootstrap two megabytes away from where the patched
    /// call jumped, which is exactly how `distribute_cfs_runtime` ended up
    /// executing the middle of an instruction.
    #[test]
    fn text_cave_uses_content_offsets_not_address_offsets() {
        const BASE: u64 = 0xffff_ffff_8100_0000;
        const SHIFT: usize = 0x2000;
        let mut content = vec![0x90_u8; SHIFT + 0x4000];
        content[SHIFT + 0x1000..SHIFT + 0x2000].fill(TEXT_PADDING_BYTE);
        let layout = KernelImageLayout {
            segments: vec![ProgramSegment {
                file_offset: SHIFT as u64,
                memory_offset: 0,
                file_size: 0x4000,
                memory_size: 0x4000,
            }],
            kernel_total_size: 0x4000,
        };
        let symbols = SymbolMap::new(vec![MapSymbol {
            address: BASE,
            name: "_stext".into(),
        }])
        .unwrap();
        let text_start = MapSymbol {
            address: BASE,
            name: "_stext".into(),
        };
        let text_end = MapSymbol {
            address: BASE + 0x3f00,
            name: "_etext".into(),
        };
        let (cave, _) = find_x86_text_cave(
            &layout,
            &content,
            &symbols,
            BASE,
            &text_start,
            &text_end,
            0x100,
        )
        .unwrap();
        assert_eq!(cave, SHIFT + 0x1000);
        assert_eq!(layout.address_of(cave, BASE), Some(BASE + 0x1000));
    }

    /// The module fixup path, without needing a kernel image: the embedded
    /// bootstrap object is a real `ET_REL` full of relocations against symbols
    /// a module would import.
    #[test]
    fn collects_fixups_from_an_et_rel_object() {
        let base = 0xffff_ffff_8100_0000;
        let symbols = SymbolMap::new(vec![MapSymbol {
            address: base + 0x1000,
            name: "ksu_image_base".into(),
        }])
        .unwrap();
        let (fixups, unresolved) = collect_module_fixups(
            BOOTSTRAP_X86_64_OBJECT,
            &symbols,
            base,
            BOOTSTRAP_X86_64_OBJECT.len(),
            EM_X86_64,
        )
        .unwrap();
        assert!(
            unresolved.iter().any(|name| name == "ksu_capsule_magic"),
            "symbols the kernel cannot provide are reported: {unresolved:?}"
        );
        assert!(
            fixups
                .iter()
                .all(|fixup| fixup.symbol_file_offset < BOOTSTRAP_X86_64_OBJECT.len()),
            "every fixup points at a symbol inside the object"
        );
    }

    /// Full round trip against a real `bzImage` (set both environment variables
    /// to enable it, e.g.
    /// `KSU_LKM_TEST_IMAGE=... KSU_LKM_TEST_MODULE=... cargo test`).
    /// `KSU_LKM_TEST_OUT` additionally writes the patched kernel, which is what
    /// a boot test consumes.
    #[test]
    fn patch_and_restore_a_real_bzimage() {
        let Some((original, module)) = test_fixtures() else {
            return;
        };
        let header = BzImageHeader::parse(&original).unwrap();
        let payload = get_slice(&original, header.payload_start(), header.payload_length).unwrap();
        let format = detect_payload_format(payload).unwrap();
        let original_content = decompress_payload(format, payload).unwrap();
        let (relocation_start, _) = relocation_table_range(&original_content).unwrap();

        // Analyse the original image so the patched offsets are known.
        let layout = KernelImageLayout::parse(&original_content).unwrap();
        let symbols = recover_kernel_metadata(
            &original_content,
            usize::try_from(layout.kernel_total_size).unwrap(),
            "x86-64 image",
        )
        .unwrap()
        .kallsyms
        .symbols;
        let required = RequiredSymbols::resolve(REQUIRED_SYMBOLS, &symbols).unwrap();
        let sites = analyze_patch_sites(&layout, &original_content, &symbols, &required).unwrap();

        let (patched, report) = inject_image(&original, &module, "", false).unwrap();
        if let Ok(output) = std::env::var("KSU_LKM_TEST_OUT") {
            std::fs::write(output, &patched).unwrap();
        }
        assert!(report.replaced.is_none());
        assert!(report.to_string().contains("Capsule"));

        // The rebuilt bzImage must still parse and decode.
        let new_header = BzImageHeader::parse(&patched).unwrap();
        assert_eq!(new_header.payload_offset, header.payload_offset);
        let blob_length = patched.len() - new_header.setup_size;
        assert!(
            new_header.syssize * 16 >= blob_length && new_header.syssize * 16 <= blob_length + 16,
            "syssize must cover the protected-mode blob"
        );
        let new_payload = get_slice(
            &patched,
            new_header.payload_start(),
            new_header.payload_length,
        )
        .unwrap();
        let new_content = decompress_payload(format, new_payload).unwrap();
        assert_eq!(
            read_u32(new_payload, new_payload.len() - 4).unwrap() as usize,
            new_content.len(),
            "the payload trailer carries the uncompressed length"
        );
        let (new_relocation_start, _) = relocation_table_range(&new_content).unwrap();

        // Only the three patched sites and the bootstrap cave may differ; the
        // bootstrap itself is only ever written over `int3` padding.
        let patched_ranges = [
            (
                sites.bootstrap_call.file_offset,
                CallRel32::LENGTH,
                "kernel_init patch",
            ),
            (
                sites.strndup_call.file_offset,
                CallRel32::LENGTH,
                "load_module patch",
            ),
            (
                sites.reserve_size_immediate,
                RESERVE_IMMEDIATE_LENGTH,
                "reservation patch",
            ),
        ];
        let mut changed = 0usize;
        for offset in 0..relocation_start {
            if new_content[offset] == original_content[offset] {
                continue;
            }
            changed += 1;
            assert!(
                patched_ranges
                    .iter()
                    .any(|(start, length, _)| offset >= *start && offset < start + length)
                    || original_content[offset] == TEXT_PADDING_BYTE,
                "unexpected change at 0x{offset:x}"
            );
        }
        assert!(changed >= 3, "expected at least three patched sites");

        // The capsule lives between the kernel image and the relocation table,
        // and the relocation table moved with it without changing.
        let capsule_offset = align_up(relocation_start.max(layout.content_end()), 16);
        assert_eq!(
            &new_content[capsule_offset..capsule_offset + 8],
            CAPSULE_MAGIC
        );
        assert!(
            capsule_offset >= (required.get("_end").address - required.image_base()) as usize,
            "capsule must live past _end"
        );
        assert!(new_relocation_start > relocation_start);
        assert_eq!(
            &new_content[new_relocation_start..],
            &original_content[relocation_start..],
            "the kernel relocation table has to survive the rebuild"
        );

        // The patched instruction has to grow the kernel reservation so that it
        // covers the capsule: the bootstrap only runs once the kernel image has
        // been freed otherwise.
        let patched_immediate = i64::from(i32::from_le_bytes(
            new_content[sites.reserve_size_immediate..sites.reserve_size_immediate + 4]
                .try_into()
                .unwrap(),
        ));
        let reserved_end = required.get("__end_of_kernel_reserve").address as i64
            + (required.image_base() as i64 - patched_immediate);
        let capsule_end = required.image_base() as i64 + new_relocation_start as i64;
        assert!(
            reserved_end >= capsule_end,
            "reservation end 0x{reserved_end:x} must cover the capsule end 0x{capsule_end:x}"
        );

        // Restoring has to bring the decompressed image back byte for byte.
        let restored = restore_image(&patched).unwrap();
        assert_eq!(restored.report.patched_sites, 3);
        assert_eq!(decompressed(&restored.image), original_content);
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

        // Updating to a different module has to land on the same kernel as
        // patching the stock image with it.
        let updated_module = [module.as_slice(), b"KSU"].concat();
        let (from_stock, _) = inject_image(&original, &updated_module, "", false).unwrap();
        let (from_patched, _) = inject_image(&patched, &updated_module, "", false).unwrap();
        assert_eq!(decompressed(&from_stock), decompressed(&from_patched));
        assert_eq!(
            decompressed(&restore_image(&from_patched).unwrap().image),
            original_content
        );
    }
}
