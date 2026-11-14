// SPDX-License-Identifier: GPL-2.0-only
//!
//! The `KSULKM1` capsule: module payload, fixup table, `load_module()` params
//! and the self-contained restore record, appended to the kernel image.

use anyhow::{Context, Result, anyhow, ensure};

use super::bytes::{align_up, find_subslice, get_slice, read_u32, read_u64, write_u32, write_u64};

pub const CAPSULE_MAGIC: &[u8; 8] = b"KSULKM1\0";

/// Version 3 merges the two independent version 2 layouts that never shipped:
/// upstream grew the header to 112 bytes for the `load_module()` params at
/// offsets 96/104, while the x86-64 work used the very same slots for the
/// restore record.  The merged header is 128 bytes: upstream's params keep
/// 96/104, the restore record moves to 112/120.  Bumping the version means an
/// image patched by either unreleased v2 is rejected with a clear message
/// instead of being misread.
///
/// The bootstrap object only ever reads offsets
/// 0/8/12/16/24/32/40/48/96/104, so it does not care about the record.
pub const CAPSULE_VERSION: u32 = 3;
pub const CAPSULE_ALIGNMENT: usize = 4096;
pub const CAPSULE_HEADER_SIZE: usize = 128;
pub const CAPSULE_FLAG_SHN_ABS_FIXUPS: u64 = 1;
/// The capsule carries a [`crate::lkm_image::restore::RestoreRecord`].
pub const CAPSULE_FLAG_RESTORE_RECORD: u64 = 2;

/// Upper bound of the `load_module()` params string, excluding the NUL
/// terminator.
pub const MAX_MODULE_PARAMS_SIZE: usize = 4096;

const MODULE_SHA_OFFSET: usize = 64;
const MODULE_SHA_SIZE: usize = 32;
const ARGS_OFFSET_OFFSET: usize = 96;
const ARGS_SIZE_OFFSET: usize = 104;
const RESTORE_OFFSET_OFFSET: usize = 112;
const RESTORE_SIZE_OFFSET: usize = 120;

#[derive(Debug)]
pub struct Capsule {
    pub data: Vec<u8>,
    pub file_offset: usize,
    pub image_size: usize,
    pub module_offset: usize,
    pub fixup_offset: usize,
    /// Where the NUL terminated `load_module()` params string lives.
    pub args_offset: usize,
    /// Length of that string including its NUL terminator.
    pub args_size: usize,
}

pub fn module_sha256(module: &[u8]) -> Result<[u8; 32]> {
    let digest = sha256::digest(module);
    let mut output = [0u8; 32];
    base16ct::lower::decode(digest.as_bytes(), &mut output)
        .map_err(|error| anyhow!("cannot decode module SHA-256: {error}"))?;
    Ok(output)
}

/// Build the `KSULKM1` capsule at an explicit image offset.
///
/// `image_size` is the size the caller's kernel image (arm64 `Image`, or the
/// decompressed x86-64 payload) will have once the capsule is installed; it is
/// only used to compute the reported capsule end.
pub fn build_capsule_at(
    capsule_offset: usize,
    image_size: usize,
    module: &[u8],
    fixup_bytes: &[u8],
    fixup_count: usize,
    params: &str,
    restore_record: Option<&[u8]>,
) -> Result<Capsule> {
    ensure!(
        fixup_bytes.len() == fixup_count.saturating_mul(16),
        "fixup table length does not match its entry count"
    );
    ensure!(
        !params.as_bytes().contains(&0),
        "module params must not contain a NUL byte"
    );
    ensure!(
        params.len() <= MAX_MODULE_PARAMS_SIZE,
        "module params are {} bytes, the limit is {MAX_MODULE_PARAMS_SIZE}",
        params.len()
    );
    let _ = image_size;
    let module_relative_offset = CAPSULE_HEADER_SIZE;
    let fixup_relative_offset = module_relative_offset
        .checked_add(align_up(module.len(), 16))
        .context("capsule module offset overflow")?;
    // The params sit behind the fixup table; the bootstrap reads their offset
    // and size out of header fields 96/104 and compares both.
    let args_relative_offset = align_up(
        fixup_relative_offset
            .checked_add(fixup_bytes.len())
            .context("capsule fixup offset overflow")?,
        16,
    );
    // NUL terminated, passed to load_module() as uargs
    let args_size = params.len() + 1;
    let args_end = args_relative_offset
        .checked_add(args_size)
        .context("capsule params length overflow")?;
    let restore_relative_offset = align_up(args_end, 8);
    let content_end = restore_relative_offset
        .checked_add(restore_record.map_or(0, <[u8]>::len))
        .context("capsule restore record offset overflow")?;
    let new_image_size = align_up(
        capsule_offset
            .checked_add(content_end)
            .context("capsule image size overflow")?,
        CAPSULE_ALIGNMENT,
    );
    let capsule_size = new_image_size - capsule_offset;
    let mut flags = if fixup_count == 0 {
        0
    } else {
        CAPSULE_FLAG_SHN_ABS_FIXUPS
    };
    if restore_record.is_some() {
        flags |= CAPSULE_FLAG_RESTORE_RECORD;
    }

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
    data[MODULE_SHA_OFFSET..MODULE_SHA_OFFSET + MODULE_SHA_SIZE]
        .copy_from_slice(&module_sha256(module)?);
    write_u64(
        &mut data,
        ARGS_OFFSET_OFFSET,
        u64::try_from(args_relative_offset)?,
    )?;
    write_u64(&mut data, ARGS_SIZE_OFFSET, u64::try_from(args_size)?)?;
    data[module_relative_offset..module_relative_offset + module.len()].copy_from_slice(module);
    data[fixup_relative_offset..fixup_relative_offset + fixup_bytes.len()]
        .copy_from_slice(fixup_bytes);
    data[args_relative_offset..args_relative_offset + params.len()]
        .copy_from_slice(params.as_bytes());
    // `data` is zeroed, so the NUL terminator after the params is already there.
    if let Some(record) = restore_record {
        ensure!(
            !record.is_empty(),
            "an empty restore record cannot be restored from"
        );
        write_u64(
            &mut data,
            RESTORE_OFFSET_OFFSET,
            u64::try_from(restore_relative_offset)?,
        )?;
        write_u64(&mut data, RESTORE_SIZE_OFFSET, u64::try_from(record.len())?)?;
        data[restore_relative_offset..restore_relative_offset + record.len()]
            .copy_from_slice(record);
    }

    Ok(Capsule {
        data,
        file_offset: capsule_offset,
        image_size: new_image_size,
        module_offset: capsule_offset + module_relative_offset,
        fixup_offset: capsule_offset + fixup_relative_offset,
        args_offset: capsule_offset + args_relative_offset,
        args_size,
    })
}

/// A parsed capsule header, plus where its restore record lives.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct CapsuleView {
    /// Offset of the capsule inside the decompressed kernel image.
    pub offset: usize,
    /// Length of the capsule, i.e. how many bytes have to be cut back out.
    pub size: usize,
    pub version: u32,
    pub header_size: usize,
    pub flags: u64,
    pub module_offset: usize,
    pub module_size: usize,
    pub fixup_offset: usize,
    pub fixup_count: usize,
    /// Absolute range of the restore record inside the image, when present.
    pub restore: Option<std::ops::Range<usize>>,
}

/// Read the capsule header at `offset`, checking that every field it claims
/// stays inside the image.
pub fn open_capsule(content: &[u8], offset: usize) -> Result<CapsuleView> {
    let header = get_slice(content, offset, CAPSULE_HEADER_SIZE)
        .with_context(|| format!("capsule header at 0x{offset:x} is outside the image"))?;
    ensure!(
        &header[..8] == CAPSULE_MAGIC,
        "capsule magic at 0x{offset:x} is missing"
    );
    let version = read_u32(header, 8)?;
    ensure!(
        version == CAPSULE_VERSION,
        "capsule version {version} is not supported (this build understands {CAPSULE_VERSION})"
    );
    let header_size = usize::try_from(read_u32(header, 12)?)?;
    ensure!(
        header_size == CAPSULE_HEADER_SIZE,
        "capsule header is {header_size} bytes, this build understands {CAPSULE_HEADER_SIZE}"
    );
    let size = usize::try_from(read_u64(header, 16)?)?;
    ensure!(size >= CAPSULE_HEADER_SIZE, "capsule size is too small");
    let end = offset.checked_add(size).context("capsule end overflow")?;
    ensure!(
        end <= content.len(),
        "capsule at 0x{offset:x} ends at 0x{end:x}, past the 0x{:x} byte image",
        content.len()
    );
    let module_offset = usize::try_from(read_u64(header, 24)?)?;
    let module_size = usize::try_from(read_u64(header, 32)?)?;
    let fixup_offset = usize::try_from(read_u64(header, 40)?)?;
    let fixup_count = usize::try_from(read_u64(header, 48)?)?;
    ensure!(
        module_offset
            .checked_add(module_size)
            .is_some_and(|module_end| module_end <= size),
        "capsule module is outside the capsule"
    );
    ensure!(
        fixup_offset
            .checked_add(fixup_count.saturating_mul(16))
            .is_some_and(|fixup_end| fixup_end <= size),
        "capsule fixup table is outside the capsule"
    );
    // The bootstrap reads both fields and hands the address to load_module(),
    // so a capsule whose params do not fit has to be rejected here.
    let args_offset = usize::try_from(read_u64(header, ARGS_OFFSET_OFFSET)?)?;
    let args_size = usize::try_from(read_u64(header, ARGS_SIZE_OFFSET)?)?;
    ensure!(
        args_size > 0
            && args_offset
                .checked_add(args_size)
                .is_some_and(|args_end| args_end <= size),
        "capsule module params are outside the capsule"
    );
    let flags = read_u64(header, 56)?;
    let restore = if flags & CAPSULE_FLAG_RESTORE_RECORD == 0 {
        None
    } else {
        let restore_offset = usize::try_from(read_u64(header, RESTORE_OFFSET_OFFSET)?)?;
        let restore_size = usize::try_from(read_u64(header, RESTORE_SIZE_OFFSET)?)?;
        ensure!(
            restore_size > 0
                && restore_offset
                    .checked_add(restore_size)
                    .is_some_and(|end| end <= size),
            "capsule restore record is outside the capsule"
        );
        Some(offset + restore_offset..offset + restore_offset + restore_size)
    };
    Ok(CapsuleView {
        offset,
        size,
        version,
        header_size,
        flags,
        module_offset: offset + module_offset,
        module_size,
        fixup_offset: offset + fixup_offset,
        fixup_count,
        restore,
    })
}

/// Every valid capsule header in a decompressed image.
///
/// Scanning for the magic is not enough on its own: the bootstrap object embeds
/// the capsule's own header constants as literals (that is how the stub compares
/// against them), and they happen to spell out a valid header prefix.  Callers
/// that need a capsule they can *use* have to look at the restore record too.
pub fn find_capsules(content: &[u8]) -> Vec<CapsuleView> {
    let mut found = Vec::new();
    let mut start = 0;
    while let Some(position) = find_subslice(content, CAPSULE_MAGIC, start) {
        start = position + 1;
        if let Ok(view) = open_capsule(content, position) {
            found.push(view);
        }
    }
    found
}

/// The link definitions that describe the capsule wire format./// The link definitions that describe the capsule wire format.
///
/// The bootstrap object compares these against the bytes [`build_capsule`]
/// wrote, so the two sides have to be derived from the same constants: when
/// they drifted apart the stub silently skipped the injection.
pub fn capsule_wire_definitions() -> [(&'static str, u64); 3] {
    [
        ("ksu_capsule_magic", u64::from_le_bytes(*CAPSULE_MAGIC)),
        ("ksu_capsule_version", u64::from(CAPSULE_VERSION)),
        ("ksu_capsule_header_size", CAPSULE_HEADER_SIZE as u64),
    ]
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::lkm_image::bytes::{read_u32, read_u64};
    use crate::lkm_image::restore::{
        ContainerFields, RestoreCave, RestoreRecord, RestoreSite, sha256_bytes,
    };

    fn lookup(definitions: &[(&str, u64)], key: &str) -> u64 {
        definitions
            .iter()
            .find(|(name, _)| *name == key)
            .unwrap_or_else(|| panic!("no definition for {key}"))
            .1
    }

    /// The definitions the stub is linked against have to describe the capsule
    /// that is actually written.
    #[test]
    fn capsule_wire_definitions_match_the_written_capsule() {
        let capsule =
            build_capsule_at(0x20_0000, 0x20_0000, b"module", &[0; 16], 1, "", None).unwrap();
        let definitions = capsule_wire_definitions();
        assert_eq!(
            read_u64(&capsule.data, 0).unwrap(),
            lookup(&definitions, "ksu_capsule_magic")
        );
        assert_eq!(
            u64::from(read_u32(&capsule.data, 8).unwrap()),
            lookup(&definitions, "ksu_capsule_version")
        );
        assert_eq!(
            u64::from(read_u32(&capsule.data, 12).unwrap()),
            lookup(&definitions, "ksu_capsule_header_size")
        );
    }

    /// Every field of the header, including the restore record, has to describe
    /// the capsule that was written -- and the capsule has to be findable again
    /// in the image it was built for.
    #[test]
    fn capsule_layout_matches_wire_format() {
        let module = b"module-bytes";
        let fixups = (0u8..16).collect::<Vec<_>>();
        let image_size = 0x20_0003;
        let capsule_offset = align_up(image_size, 16);
        let record = RestoreRecord::new(
            b"the original image",
            ContainerFields::Arm64 {
                image_size: image_size as u64,
            },
            vec![RestoreSite::new(0x40, &[0x94, 0, 0, 1]).unwrap()],
            RestoreCave::new(0x1000, 0x80, 0).unwrap(),
            capsule_offset,
        )
        .unwrap();
        let record_bytes = record.encode().unwrap();
        let params = "bundled=1 norc=1";
        let capsule = build_capsule_at(
            capsule_offset,
            image_size,
            module,
            &fixups,
            1,
            params,
            Some(&record_bytes),
        )
        .unwrap();

        assert_eq!(capsule.file_offset, capsule_offset);
        assert!(capsule.image_size.is_multiple_of(CAPSULE_ALIGNMENT));
        assert_eq!(&capsule.data[..8], CAPSULE_MAGIC);
        assert_eq!(read_u32(&capsule.data, 8).unwrap(), CAPSULE_VERSION);
        assert_eq!(
            read_u32(&capsule.data, 12).unwrap(),
            CAPSULE_HEADER_SIZE as u32
        );
        assert_eq!(
            read_u64(&capsule.data, 16).unwrap(),
            capsule.data.len() as u64
        );
        assert_eq!(read_u64(&capsule.data, 32).unwrap(), module.len() as u64);
        assert_eq!(read_u64(&capsule.data, 48).unwrap(), 1);
        assert_eq!(
            read_u64(&capsule.data, 56).unwrap(),
            CAPSULE_FLAG_SHN_ABS_FIXUPS | CAPSULE_FLAG_RESTORE_RECORD
        );
        let module_offset = usize::try_from(read_u64(&capsule.data, 24).unwrap()).unwrap();
        assert_eq!(
            &capsule.data[module_offset..module_offset + module.len()],
            module
        );
        let fixup_offset = usize::try_from(read_u64(&capsule.data, 40).unwrap()).unwrap();
        assert_eq!(
            &capsule.data[fixup_offset..fixup_offset + fixups.len()],
            fixups.as_slice()
        );
        assert_eq!(
            &capsule.data[64..96],
            sha256_bytes(module).unwrap().as_slice(),
            "the capsule has to carry the module digest"
        );

        // The header points at the params and the record; both come back
        // verbatim from the slots the bootstrap and `boot-restore-v2` read.
        let args_offset = usize::try_from(read_u64(&capsule.data, 96).unwrap()).unwrap();
        let args_size = usize::try_from(read_u64(&capsule.data, 104).unwrap()).unwrap();
        assert_eq!(args_offset, capsule.args_offset - capsule_offset);
        assert_eq!(args_size, capsule.args_size);
        assert!(args_offset >= 16 + usize::try_from(read_u64(&capsule.data, 40).unwrap()).unwrap());
        assert_eq!(
            &capsule.data[args_offset..args_offset + args_size],
            b"bundled=1 norc=1\0"
        );
        assert!(args_offset + args_size <= capsule.data.len());

        let restore_offset = usize::try_from(read_u64(&capsule.data, 112).unwrap()).unwrap();
        let restore_size = usize::try_from(read_u64(&capsule.data, 120).unwrap()).unwrap();
        assert_eq!(
            &capsule.data[restore_offset..restore_offset + restore_size],
            record_bytes.as_slice()
        );
        assert_eq!(
            RestoreRecord::parse(&record_bytes, crate::lkm_image::elf::EM_AARCH64)
                .unwrap()
                .capsule_offset,
            capsule_offset as u64
        );

        assert_eq!(args_offset, fixup_offset + fixups.len());
        assert_eq!(capsule.data[args_offset + params.len()], 0);
        assert!(
            restore_offset >= args_offset + args_size,
            "the restore record must not overlap the params (or their NUL)"
        );

        // And the capsule is findable in the image it was built for.
        let mut image = vec![0u8; capsule_offset];
        image.extend_from_slice(&capsule.data);
        let view = find_capsules(&image).into_iter().next().unwrap();
        assert_eq!(view.offset, capsule_offset);
        assert_eq!(view.size, capsule.data.len());
        assert_eq!(
            view.restore,
            Some(capsule_offset + restore_offset..capsule_offset + restore_offset + restore_size)
        );
        assert_eq!(open_capsule(&image, capsule_offset).unwrap(), view);
    }

    /// The params string goes straight into the capsule and is handed to
    /// `load_module()`, so a NUL inside it or an overlong string is refused.
    #[test]
    fn capsule_rejects_invalid_params() {
        assert!(build_capsule_at(0x20_0000, 0x20_0000, b"m", &[], 0, "a\0b", None).is_err());
        let long = "a".repeat(MAX_MODULE_PARAMS_SIZE + 1);
        assert!(build_capsule_at(0x20_0000, 0x20_0000, b"m", &[], 0, &long, None).is_err());
        assert!(
            build_capsule_at(
                0x20_0000,
                0x20_0000,
                b"m",
                &[],
                0,
                &"a".repeat(MAX_MODULE_PARAMS_SIZE),
                None
            )
            .is_ok()
        );
    }
}
