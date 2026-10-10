// SPDX-License-Identifier: GPL-2.0-only
//!
//! The self-contained restore record.
//!
//! `boot-patch-v2` writes one of these into the capsule before it links the
//! bootstrap, so `boot-restore-v2` can undo an injection without any external
//! backup file: it records the original bytes of everything the injection
//! overwrote, plus enough geometry to cut the capsule back out and rebuild the
//! container.
//!
//! The record is deliberately dumb -- it stores bytes, not instructions -- so
//! restoring never has to re-derive an address that KASLR may have moved.

use anyhow::{Context, Result, bail, ensure};

use super::bytes::{read_u16, read_u32, read_u64, write_u32, write_u64};
use super::capsule::find_capsules;

pub const RESTORE_MAGIC: &[u8; 4] = b"KSUR";

/// Bumped whenever the record layout changes; an unknown version is an error,
/// never a guess.
pub const RESTORE_VERSION: u16 = 1;

/// The longest instruction the injection patches (the x86-64 `call rel32` is
/// five bytes, the arm64 `BL` is four).
pub const MAXIMUM_SITE_LENGTH: usize = 8;

const HEADER_SIZE: usize = 64;
const SITE_SIZE: usize = 16;
const CAVE_SIZE: usize = 16;
const CAPSULE_SIZE: usize = 8;

const SHA_OFFSET: usize = 8;
const CONTENT_LEN_OFFSET: usize = 40;
const CONTAINER_OFFSET: usize = 48;
const SITE_COUNT_OFFSET: usize = 56;

/// One instruction the injection replaced, and the bytes it replaced.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct RestoreSite {
    pub offset: u32,
    pub bytes: [u8; MAXIMUM_SITE_LENGTH],
    pub length: u8,
}

impl RestoreSite {
    pub fn new(offset: usize, bytes: &[u8]) -> Result<Self> {
        ensure!(
            !bytes.is_empty() && bytes.len() <= MAXIMUM_SITE_LENGTH,
            "a restore site holds at most {MAXIMUM_SITE_LENGTH} bytes"
        );
        let mut recorded = [0u8; MAXIMUM_SITE_LENGTH];
        recorded[..bytes.len()].copy_from_slice(bytes);
        Ok(Self {
            offset: u32::try_from(offset).context("restore site offset does not fit")?,
            bytes: recorded,
            length: u8::try_from(bytes.len())?,
        })
    }

    pub fn range(&self) -> Result<std::ops::Range<usize>> {
        let start = usize::try_from(self.offset)?;
        Ok(start..start + usize::from(self.length))
    }

    fn write(&self, bytes: &mut [u8]) -> Result<()> {
        let range = self.range()?;
        let destination = bytes
            .get_mut(range)
            .with_context(|| format!("restore site 0x{:x} is outside the image", self.offset))?;
        destination.copy_from_slice(&self.bytes[..usize::from(self.length)]);
        Ok(())
    }

    fn is_intact(&self, bytes: &[u8]) -> Result<bool> {
        let range = self.range()?;
        let current = bytes
            .get(range)
            .with_context(|| format!("restore site 0x{:x} is outside the image", self.offset))?;
        Ok(current == &self.bytes[..usize::from(self.length)])
    }
}

/// The `int3`/zero padding the bootstrap was linked into.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct RestoreCave {
    pub offset: u32,
    pub size: u32,
    pub padding: u8,
}

impl RestoreCave {
    pub fn new(offset: usize, size: usize, padding: u8) -> Result<Self> {
        Ok(Self {
            offset: u32::try_from(offset).context("restore cave offset does not fit")?,
            size: u32::try_from(size).context("restore cave size does not fit")?,
            padding,
        })
    }
}

/// The container fields the injection rewrote.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ContainerFields {
    /// The arm64 `Image` header's `image_size` at offset `0x10`.
    Arm64 { image_size: u64 },
    /// The x86-64 setup header's `syssize` and `init_size`.
    X86_64 { syssize: u32, init_size: u32 },
}

impl ContainerFields {
    const fn arch(self) -> u16 {
        match self {
            Self::Arm64 { .. } => super::elf::EM_AARCH64,
            Self::X86_64 { .. } => super::elf::EM_X86_64,
        }
    }

    fn encode(self) -> u64 {
        match self {
            Self::Arm64 { image_size } => image_size,
            Self::X86_64 { syssize, init_size } => {
                u64::from(syssize) | (u64::from(init_size) << 32)
            }
        }
    }

    fn decode(arch: u16, value: u64) -> Result<Self> {
        match arch {
            super::elf::EM_AARCH64 => Ok(Self::Arm64 { image_size: value }),
            super::elf::EM_X86_64 => Ok(Self::X86_64 {
                syssize: value as u32,
                init_size: (value >> 32) as u32,
            }),
            other => bail!("unsupported restore record architecture {other}"),
        }
    }
}

/// Everything `boot-restore-v2` needs to undo one injection.
#[derive(Debug)]
pub struct RestoreRecord {
    pub arch: u16,
    pub content_sha256: [u8; 32],
    /// Length of the decompressed image the injection started from.
    pub original_content_len: u64,
    pub container: ContainerFields,
    pub sites: Vec<RestoreSite>,
    pub cave: RestoreCave,
    /// Offset of the capsule inside the decompressed image.
    pub capsule_offset: u64,
}

impl RestoreRecord {
    pub fn new(
        content: &[u8],
        container: ContainerFields,
        sites: Vec<RestoreSite>,
        cave: RestoreCave,
        capsule_offset: usize,
    ) -> Result<Self> {
        ensure!(
            !sites.is_empty(),
            "an injection always patches at least one instruction"
        );
        Ok(Self {
            arch: container.arch(),
            content_sha256: sha256_bytes(content)?,
            original_content_len: u64::try_from(content.len())
                .context("content length does not fit")?,
            container,
            sites,
            cave,
            capsule_offset: u64::try_from(capsule_offset).context("capsule offset does not fit")?,
        })
    }

    fn encoded_len(&self) -> Result<usize> {
        HEADER_SIZE
            .checked_add(
                self.sites
                    .len()
                    .checked_mul(SITE_SIZE)
                    .context("site overflow")?,
            )
            .and_then(|size| size.checked_add(CAVE_SIZE + CAPSULE_SIZE))
            .context("restore record size overflow")
    }

    pub fn encode(&self) -> Result<Vec<u8>> {
        let mut data = vec![0u8; self.encoded_len()?];
        data[..4].copy_from_slice(RESTORE_MAGIC);
        data[4..6].copy_from_slice(&RESTORE_VERSION.to_le_bytes());
        data[6..8].copy_from_slice(&self.arch.to_le_bytes());
        data[SHA_OFFSET..SHA_OFFSET + 32].copy_from_slice(&self.content_sha256);
        write_u64(&mut data, CONTENT_LEN_OFFSET, self.original_content_len)?;
        write_u64(&mut data, CONTAINER_OFFSET, self.container.encode())?;
        write_u32(
            &mut data,
            SITE_COUNT_OFFSET,
            u32::try_from(self.sites.len())?,
        )?;
        for (index, site) in self.sites.iter().enumerate() {
            let base = HEADER_SIZE + index * SITE_SIZE;
            write_u32(&mut data, base, site.offset)?;
            write_u32(&mut data, base + 4, u32::from(site.length))?;
            data[base + 8..base + SITE_SIZE].copy_from_slice(&site.bytes);
        }
        let cave_base = HEADER_SIZE + self.sites.len() * SITE_SIZE;
        write_u32(&mut data, cave_base, self.cave.offset)?;
        write_u32(&mut data, cave_base + 4, self.cave.size)?;
        data[cave_base + 8] = self.cave.padding;
        write_u64(&mut data, cave_base + CAVE_SIZE, self.capsule_offset)?;
        Ok(data)
    }

    /// Parse a record the capsule points at, checking it describes `arch`.
    pub fn parse(data: &[u8], arch: u16) -> Result<Self> {
        ensure!(
            data.len() >= HEADER_SIZE,
            "restore record is truncated ({} bytes)",
            data.len()
        );
        ensure!(
            &data[..4] == RESTORE_MAGIC,
            "restore record magic is missing"
        );
        let version = read_u16(data, 4)?;
        ensure!(
            version == RESTORE_VERSION,
            "restore record version {version} is not supported (this build understands {RESTORE_VERSION})"
        );
        let record_arch = read_u16(data, 6)?;
        ensure!(
            record_arch == arch,
            "restore record is for a different architecture (record {record_arch}, image {arch})"
        );
        let mut content_sha256 = [0u8; 32];
        content_sha256.copy_from_slice(&data[SHA_OFFSET..SHA_OFFSET + 32]);
        let site_count = usize::try_from(read_u32(data, SITE_COUNT_OFFSET)?)?;
        let expected = HEADER_SIZE
            + site_count.checked_mul(SITE_SIZE).context("site overflow")?
            + CAVE_SIZE
            + CAPSULE_SIZE;
        ensure!(
            data.len() >= expected,
            "restore record is truncated: {site_count} sites need {expected} bytes, found {}",
            data.len()
        );
        ensure!(site_count > 0, "restore record has no patched sites");

        let mut sites = Vec::with_capacity(site_count);
        for index in 0..site_count {
            let base = HEADER_SIZE + index * SITE_SIZE;
            let length = u8::try_from(read_u32(data, base + 4)?)
                .context("restore site length is out of range")?;
            ensure!(
                length > 0 && usize::from(length) <= MAXIMUM_SITE_LENGTH,
                "restore site {index} has an invalid length"
            );
            let mut bytes = [0u8; MAXIMUM_SITE_LENGTH];
            bytes.copy_from_slice(&data[base + 8..base + SITE_SIZE]);
            sites.push(RestoreSite {
                offset: read_u32(data, base)?,
                bytes,
                length,
            });
        }
        let cave_base = HEADER_SIZE + site_count * SITE_SIZE;
        let cave = RestoreCave {
            offset: read_u32(data, cave_base)?,
            size: read_u32(data, cave_base + 4)?,
            padding: data[cave_base + 8],
        };
        let capsule_offset = read_u64(data, cave_base + CAVE_SIZE)?;
        let record = Self {
            arch: record_arch,
            content_sha256,
            original_content_len: read_u64(data, CONTENT_LEN_OFFSET)?,
            container: ContainerFields::decode(record_arch, read_u64(data, CONTAINER_OFFSET)?)?,
            sites,
            cave,
            capsule_offset,
        };
        ensure!(
            record.original_content_len > 0,
            "restore record has no original content length"
        );
        Ok(record)
    }

    /// Undo the injection on the *decompressed* image, without cutting the
    /// capsule out: the caller owns the container layout.
    ///
    /// Every patched instruction has to still differ from the recorded bytes,
    /// which is what makes restoring an unpatched or half-patched image fail
    /// instead of corrupting it.
    pub fn apply(&self, content: &mut [u8]) -> Result<()> {
        for site in &self.sites {
            ensure!(
                !site.is_intact(content)?,
                "the image is already restored: site 0x{:x} still holds its original bytes",
                site.offset
            );
            site.write(content)?;
        }
        let start = usize::try_from(self.cave.offset)?;
        let end = start
            .checked_add(usize::try_from(self.cave.size)?)
            .context("restore cave range overflow")?;
        let cave = content.get_mut(start..end).with_context(|| {
            format!(
                "bootstrap cave 0x{:x}..0x{end:x} is outside the decompressed image",
                self.cave.offset
            )
        })?;
        cave.fill(self.cave.padding);
        Ok(())
    }

    /// The checks that do not depend on the container: the restored image has
    /// the recorded length and digests to the recorded SHA-256.
    pub fn verify(&self, content: &[u8]) -> Result<()> {
        let expected = usize::try_from(self.original_content_len)?;
        ensure!(
            content.len() == expected,
            "restored image is 0x{:x} bytes, the record says 0x{expected:x}",
            content.len()
        );
        let digest = sha256_bytes(content)?;
        ensure!(
            digest == self.content_sha256,
            "restored image does not match the recorded SHA-256"
        );
        Ok(())
    }
}

pub fn sha256_bytes(data: &[u8]) -> Result<[u8; 32]> {
    let digest = sha256::digest(data);
    let mut output = [0u8; 32];
    base16ct::lower::decode(digest.as_bytes(), &mut output)
        .map_err(|error| anyhow::anyhow!("cannot decode SHA-256: {error}"))?;
    Ok(output)
}

/// What a restore did, in the order the CLI prints it.
#[derive(Debug)]
pub struct RestoreReport {
    pub arch: super::Arch,
    pub capsule_offset: usize,
    pub capsule_size: usize,
    pub patched_sites: usize,
    pub original_content_len: u64,
    pub details: RestoreDetails,
}

/// The container specific part of a restore.
#[derive(Debug)]
pub enum RestoreDetails {
    /// The whole `Image` is byte-identical to the one the injection started
    /// from; only the header's `image_size` had to be written back.
    Arm64 { image_size: u64 },
    /// Only the decompressed payload is byte-identical: the container is
    /// rebuilt, and the compressed payload is unlikely to compress back to its
    /// original bytes.
    X86_64 {
        payload_before: usize,
        payload_after: usize,
        syssize_before: usize,
        syssize_after: usize,
        init_size_before: u32,
        init_size_after: u32,
    },
}

/// The restored kernel block, ready to be put back into the boot image.
#[derive(Debug)]
pub struct RestoredImage {
    pub image: Vec<u8>,
    pub report: RestoreReport,
}

impl RestoreReport {
    /// Print the report the way `boot-restore-v2` does.
    pub fn print(&self) {
        print!("{self}");
    }
}

impl std::fmt::Display for RestoreReport {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        writeln!(
            f,
            "- Architecture: {} ({})",
            self.arch.label(),
            self.arch.container_label()
        )?;
        writeln!(
            f,
            "- Capsule: offset 0x{:x}, {} bytes removed",
            self.capsule_offset, self.capsule_size
        )?;
        writeln!(
            f,
            "- Undone: {} patched instructions, 1 bootstrap cave",
            self.patched_sites
        )?;
        writeln!(
            f,
            "- Kernel image: 0x{:x} bytes, SHA-256 matches the original",
            self.original_content_len
        )?;
        match &self.details {
            RestoreDetails::Arm64 { image_size } => {
                writeln!(f, "- Image size field restored to 0x{image_size:x}")?;
            }
            RestoreDetails::X86_64 {
                payload_before,
                payload_after,
                syssize_before,
                syssize_after,
                init_size_before,
                init_size_after,
            } => {
                writeln!(
                    f,
                    "- Payload: 0x{payload_before:x} -> 0x{payload_after:x} bytes (re-encoded)"
                )?;
                writeln!(
                    f,
                    "- Container: syssize 0x{syssize_before:x} -> 0x{syssize_after:x}, \
init_size 0x{init_size_before:x} -> 0x{init_size_after:x}"
                )?;
                if syssize_before == syssize_after && init_size_before == init_size_after {
                    writeln!(f, "- Container fields came back identical")?;
                } else {
                    writeln!(
                        f,
                        "- Note: the compressed payload is re-encoded, so the boot image is not \
byte-identical to the original; AVB/vbmeta has to be re-signed"
                    )?;
                }
            }
        }
        Ok(())
    }
}

/// A previous injection that was undone, ready to be patched again.
#[derive(Debug)]
pub struct Undone {
    /// The image as it was before that injection.
    pub content: Vec<u8>,
    pub replaced: ReplacedInjection,
    /// The container fields the previous record carried.  They describe the
    /// *original* container, so an update passes them on instead of recording
    /// whatever container it happens to run in.
    pub original_container: ContainerFields,
}

/// What `boot-patch-v2` had to undo before it could patch again.
#[derive(Clone, Copy, Debug)]
pub struct ReplacedInjection {
    pub capsule_offset: usize,
    pub capsule_size: usize,
    pub patched_sites: usize,
}

impl Undone {
    pub const fn new(content: Vec<u8>, patched: &PatchedImage) -> Self {
        Self {
            content,
            replaced: ReplacedInjection {
                capsule_offset: patched.capsule_offset,
                capsule_size: patched.capsule_size,
                patched_sites: patched.record.sites.len(),
            },
            original_container: patched.record.container,
        }
    }
}

/// The capsule and the record inside it, located in a decompressed image.
#[derive(Debug)]
pub struct PatchedImage {
    pub capsule_offset: usize,
    pub capsule_size: usize,
    pub record: RestoreRecord,
}

/// Find the capsule in `content` and parse the restore record it carries, if
/// the image carries one at all.
pub fn open_injection(content: &[u8], arch: u16) -> Result<Option<PatchedImage>> {
    let mut found: Option<PatchedImage> = None;
    let mut without_record = false;
    for capsule in find_capsules(content) {
        // The bootstrap object embeds the header constants as literals, which
        // look exactly like a capsule header; the record is what tells the two
        // apart.
        let Some(restore) = capsule.restore else {
            without_record = true;
            continue;
        };
        let Ok(record) = RestoreRecord::parse(&content[restore], arch) else {
            continue;
        };
        ensure!(
            record.capsule_offset == u64::try_from(capsule.offset)?,
            "restore record points at capsule 0x{:x}, the capsule is at 0x{:x}",
            record.capsule_offset,
            capsule.offset
        );
        ensure!(
            found.is_none(),
            "the image contains more than one capsule (0x{:x} and 0x{:x})",
            found.as_ref().map_or(0, |patched| patched.capsule_offset),
            capsule.offset
        );
        found = Some(PatchedImage {
            capsule_offset: capsule.offset,
            capsule_size: capsule.size,
            record,
        });
    }
    if found.is_none() && without_record {
        bail!(
            "the kernel image is patched by an older boot-patch-v2 without a restore record; \
re-patch the stock image instead of updating this one"
        );
    }
    Ok(found)
}

/// [`open_injection`], but an image without a capsule is an error.
pub fn open_patched_image(content: &[u8], arch: u16, container: &str) -> Result<PatchedImage> {
    open_injection(content, arch)?.with_context(|| {
        format!("{container} is not patched by KernelSU boot-patch-v2 (no capsule found)")
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sample_record() -> RestoreRecord {
        RestoreRecord::new(
            b"the original decompressed image",
            ContainerFields::X86_64 {
                syssize: 0x1234,
                init_size: 0x5678_9abc,
            },
            vec![
                RestoreSite::new(0x10, &[0xe8, 1, 2, 3, 4]).unwrap(),
                RestoreSite::new(0x1000, &[0x48, 0x81, 0xee, 0, 0, 0, 0]).unwrap(),
            ],
            RestoreCave::new(0x2000, 0x40, 0xcc).unwrap(),
            0x3000,
        )
        .unwrap()
    }

    #[test]
    fn records_round_trip() {
        for record in [
            sample_record(),
            RestoreRecord::new(
                b"image",
                ContainerFields::Arm64 {
                    image_size: 0x30_0000,
                },
                vec![RestoreSite::new(0x40, &[0x94, 0, 0, 1]).unwrap()],
                RestoreCave::new(0x1000, 0x80, 0).unwrap(),
                0x30_0040,
            )
            .unwrap(),
        ] {
            let encoded = record.encode().unwrap();
            assert_eq!(encoded.len(), record.encoded_len().unwrap());
            let parsed = RestoreRecord::parse(&encoded, record.arch).unwrap();
            assert_eq!(parsed.content_sha256, record.content_sha256);
            assert_eq!(parsed.original_content_len, record.original_content_len);
            assert_eq!(parsed.container, record.container);
            assert_eq!(parsed.sites, record.sites);
            assert_eq!(parsed.cave, record.cave);
            assert_eq!(parsed.capsule_offset, record.capsule_offset);
        }
    }

    #[test]
    fn rejects_unusable_records() {
        let encoded = sample_record().encode().unwrap();
        let x86 = crate::lkm_image::elf::EM_X86_64;
        let mut unknown_version = encoded.clone();
        unknown_version[4..6].copy_from_slice(&(RESTORE_VERSION + 1).to_le_bytes());
        let mut no_magic = encoded.clone();
        no_magic[0] = b'X';

        for (label, bytes, arch, expected) in [
            ("unknown version", unknown_version, x86, "is not supported"),
            (
                "another architecture",
                encoded.clone(),
                crate::lkm_image::elf::EM_AARCH64,
                "different architecture",
            ),
            (
                "truncated",
                encoded[..HEADER_SIZE - 1].to_vec(),
                x86,
                "truncated",
            ),
            ("no magic", no_magic, x86, "magic"),
        ] {
            let error = RestoreRecord::parse(&bytes, arch).unwrap_err();
            assert!(error.to_string().contains(expected), "{label}: {error}");
        }
    }

    /// The two halves of a restore: put the recorded bytes back, then prove the
    /// result is the image the record describes.
    #[test]
    fn apply_restores_the_recorded_bytes() {
        let mut original = (0..0x4000u32)
            .map(|index| (index % 251) as u8)
            .collect::<Vec<u8>>();
        original[0x2000..0x2040].fill(0xcc);
        let record = RestoreRecord::new(
            &original,
            ContainerFields::X86_64 {
                syssize: 1,
                init_size: 2,
            },
            vec![
                RestoreSite::new(0x10, &original[0x10..0x15]).unwrap(),
                RestoreSite::new(0x1000, &original[0x1000..0x1007]).unwrap(),
            ],
            RestoreCave::new(0x2000, 0x40, 0xcc).unwrap(),
            0x3000,
        )
        .unwrap();

        // What the injection would have left behind: two replaced
        // instructions, the stub in the cave, and the capsule past the end.
        let mut patched = original.clone();
        patched[0x10..0x15].copy_from_slice(&[0xe8, 9, 9, 9, 9]);
        patched[0x1000..0x1007].copy_from_slice(&[0x48, 0x81, 0xee, 7, 7, 7, 7]);
        patched[0x2000..0x2040].fill(0x90);
        patched.resize(original.len() + 0x10, 0);

        record.apply(&mut patched).unwrap();
        assert_eq!(patched[..original.len()], original);
        record.verify(&patched[..original.len()]).unwrap();

        // Applying it again would write the same bytes back, which is reported
        // rather than silently redone.
        let error = record.apply(&mut patched).unwrap_err();
        assert!(error.to_string().contains("already restored"), "{error}");
    }

    #[test]
    fn verify_rejects_a_modified_image() {
        let record = sample_record();
        let mut content = vec![0u8; record.original_content_len as usize];
        let error = record.verify(&content).unwrap_err();
        assert!(error.to_string().contains("SHA-256"), "{error}");
        content.push(0);
        let error = record.verify(&content).unwrap_err();
        assert!(error.to_string().contains("bytes"), "{error}");
    }
}
