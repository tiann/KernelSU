// SPDX-License-Identifier: GPL-2.0-only

#![allow(clippy::similar_names)]

use std::collections::BTreeSet;

use anyhow::{Result, ensure};

const BTF_MAGIC: u16 = 0xeb9f;
const BTF_VERSION: u8 = 1;
const BTF_HEADER_SIZE: usize = 24;
const BTF_MAGIC_BYTES: &[u8; 4] = b"\x9f\xeb\x01\x00";
const ARM64_POINTER_SIZE: u64 = 8;

const BTF_KIND_INT: u8 = 1;
const BTF_KIND_PTR: u8 = 2;
const BTF_KIND_ARRAY: u8 = 3;
const BTF_KIND_STRUCT: u8 = 4;
const BTF_KIND_UNION: u8 = 5;
const BTF_KIND_ENUM: u8 = 6;
const BTF_KIND_FWD: u8 = 7;
const BTF_KIND_TYPEDEF: u8 = 8;
const BTF_KIND_VOLATILE: u8 = 9;
const BTF_KIND_CONST: u8 = 10;
const BTF_KIND_RESTRICT: u8 = 11;
const BTF_KIND_FUNC: u8 = 12;
const BTF_KIND_FUNC_PROTO: u8 = 13;
const BTF_KIND_VAR: u8 = 14;
const BTF_KIND_DATASEC: u8 = 15;
const BTF_KIND_FLOAT: u8 = 16;
const BTF_KIND_DECL_TAG: u8 = 17;
const BTF_KIND_TYPE_TAG: u8 = 18;
const BTF_KIND_ENUM64: u8 = 19;

#[derive(Clone, Copy, Debug, Eq, Ord, PartialEq, PartialOrd)]
pub struct LoadInfoLayout {
    pub structure_size: u64,
    pub hdr_offset: u64,
    pub len_offset: u64,
}

#[derive(Clone, Debug)]
pub struct KernelBtf {
    pub file_offset: usize,
    pub size: usize,
    pub type_count: usize,
    pub load_info: Option<LoadInfoLayout>,
    pub loading_module_id: Option<u64>,
}

#[derive(Clone, Copy, Debug)]
struct BtfType {
    kind: u8,
    vlen: u16,
    kind_flag: bool,
    name_offset: u32,
    size_or_type: u32,
    payload_offset: usize,
}

#[derive(Debug)]
pub struct BtfCandidate<'a> {
    data: &'a [u8],
    file_offset: usize,
    blob_end: usize,
    strings_start: usize,
    strings_end: usize,
    types: Vec<BtfType>,
}

fn read_u16(data: &[u8], offset: usize) -> Option<u16> {
    Some(u16::from_le_bytes(
        data.get(offset..offset.checked_add(2)?)?.try_into().ok()?,
    ))
}

fn read_u32(data: &[u8], offset: usize) -> Option<u32> {
    Some(u32::from_le_bytes(
        data.get(offset..offset.checked_add(4)?)?.try_into().ok()?,
    ))
}

fn ranges_overlap(
    left_start: usize,
    left_end: usize,
    right_start: usize,
    right_end: usize,
) -> bool {
    left_start.max(right_start) < left_end.min(right_end)
}

fn read_string(data: &[u8], strings_start: usize, strings_end: usize, offset: u32) -> Option<&str> {
    let start = strings_start.checked_add(usize::try_from(offset).ok()?)?;
    if start >= strings_end {
        return None;
    }
    let tail = data.get(start..strings_end)?;
    let length = tail.iter().position(|byte| *byte == 0)?;
    std::str::from_utf8(tail.get(..length)?).ok()
}

fn payload_size(kind: u8, vlen: u16) -> Option<usize> {
    let vlen = usize::from(vlen);
    match kind {
        BTF_KIND_INT | BTF_KIND_VAR | BTF_KIND_DECL_TAG => Some(4),
        BTF_KIND_PTR | BTF_KIND_FWD | BTF_KIND_TYPEDEF | BTF_KIND_VOLATILE | BTF_KIND_CONST
        | BTF_KIND_RESTRICT | BTF_KIND_FUNC | BTF_KIND_FLOAT | BTF_KIND_TYPE_TAG => Some(0),
        BTF_KIND_ARRAY => Some(12),
        BTF_KIND_STRUCT | BTF_KIND_UNION | BTF_KIND_DATASEC | BTF_KIND_ENUM64 => {
            vlen.checked_mul(12)
        }
        BTF_KIND_ENUM | BTF_KIND_FUNC_PROTO => vlen.checked_mul(8),
        _ => None,
    }
}

impl<'a> BtfCandidate<'a> {
    fn parse(data: &'a [u8], file_offset: usize) -> Option<Self> {
        let magic = read_u16(data, file_offset)?;
        let version = *data.get(file_offset.checked_add(2)?)?;
        let flags = *data.get(file_offset.checked_add(3)?)?;
        let header_length = usize::try_from(read_u32(data, file_offset.checked_add(4)?)?).ok()?;
        if magic != BTF_MAGIC
            || version != BTF_VERSION
            || flags != 0
            || header_length < BTF_HEADER_SIZE
        {
            return None;
        }

        let header_end = file_offset.checked_add(header_length)?;
        let type_offset = usize::try_from(read_u32(data, file_offset.checked_add(8)?)?).ok()?;
        let type_length = usize::try_from(read_u32(data, file_offset.checked_add(12)?)?).ok()?;
        let strings_offset = usize::try_from(read_u32(data, file_offset.checked_add(16)?)?).ok()?;
        let strings_length = usize::try_from(read_u32(data, file_offset.checked_add(20)?)?).ok()?;
        let types_start = header_end.checked_add(type_offset)?;
        let types_end = types_start.checked_add(type_length)?;
        let strings_start = header_end.checked_add(strings_offset)?;
        let strings_end = strings_start.checked_add(strings_length)?;
        if header_end > data.len()
            || types_start < header_end
            || strings_start < header_end
            || types_end > data.len()
            || strings_end > data.len()
            || strings_length == 0
            || ranges_overlap(types_start, types_end, strings_start, strings_end)
            || data.get(strings_start).copied() != Some(0)
            || data.get(strings_end.checked_sub(1)?).copied() != Some(0)
        {
            return None;
        }

        let mut types = Vec::new();
        let mut position = types_start;
        while position < types_end {
            let header_end = position.checked_add(12)?;
            if header_end > types_end {
                return None;
            }
            let name_offset = read_u32(data, position)?;
            let info = read_u32(data, position.checked_add(4)?)?;
            let size_or_type = read_u32(data, position.checked_add(8)?)?;
            let kind = u8::try_from((info >> 24) & 0x1f).ok()?;
            let vlen = u16::try_from(info & 0xffff).ok()?;
            let payload_length = payload_size(kind, vlen)?;
            let next = header_end.checked_add(payload_length)?;
            if next > types_end {
                return None;
            }
            read_string(data, strings_start, strings_end, name_offset)?;
            types.push(BtfType {
                kind,
                vlen,
                kind_flag: info >> 31 != 0,
                name_offset,
                size_or_type,
                payload_offset: header_end,
            });
            position = next;
        }
        if position != types_end {
            return None;
        }

        let parsed = Self {
            data,
            file_offset,
            blob_end: strings_end.max(types_end),
            strings_start,
            strings_end,
            types,
        };
        parsed.validate_references().then_some(parsed)
    }

    fn string(&self, offset: u32) -> Option<&'a str> {
        read_string(self.data, self.strings_start, self.strings_end, offset)
    }

    fn type_by_id(&self, type_id: u32) -> Option<&BtfType> {
        if type_id == 0 {
            return None;
        }
        self.types
            .get(usize::try_from(type_id).ok()?.checked_sub(1)?)
    }

    fn valid_type_id(&self, type_id: u32) -> bool {
        type_id == 0 || usize::try_from(type_id).is_ok_and(|id| id <= self.types.len())
    }

    fn validate_references(&self) -> bool {
        self.types.iter().all(|btf_type| {
            let valid_name = self.string(btf_type.name_offset).is_some();
            if !valid_name {
                return false;
            }
            let valid_base_reference = match btf_type.kind {
                BTF_KIND_PTR | BTF_KIND_TYPEDEF | BTF_KIND_VOLATILE | BTF_KIND_CONST
                | BTF_KIND_RESTRICT | BTF_KIND_FUNC | BTF_KIND_FUNC_PROTO | BTF_KIND_VAR
                | BTF_KIND_DECL_TAG | BTF_KIND_TYPE_TAG => {
                    self.valid_type_id(btf_type.size_or_type)
                }
                _ => true,
            };
            if !valid_base_reference {
                return false;
            }
            match btf_type.kind {
                BTF_KIND_ARRAY => {
                    let Some(element_type) = read_u32(self.data, btf_type.payload_offset) else {
                        return false;
                    };
                    let Some(index_type) = read_u32(self.data, btf_type.payload_offset + 4) else {
                        return false;
                    };
                    self.valid_type_id(element_type) && self.valid_type_id(index_type)
                }
                BTF_KIND_STRUCT | BTF_KIND_UNION => (0..usize::from(btf_type.vlen)).all(|index| {
                    let offset = btf_type.payload_offset + index * 12;
                    read_u32(self.data, offset).is_some_and(|name| self.string(name).is_some())
                        && read_u32(self.data, offset + 4)
                            .is_some_and(|type_id| self.valid_type_id(type_id))
                }),
                BTF_KIND_ENUM => (0..usize::from(btf_type.vlen)).all(|index| {
                    read_u32(self.data, btf_type.payload_offset + index * 8)
                        .is_some_and(|name| self.string(name).is_some())
                }),
                BTF_KIND_FUNC => self
                    .type_by_id(btf_type.size_or_type)
                    .is_some_and(|prototype| prototype.kind == BTF_KIND_FUNC_PROTO),
                BTF_KIND_FUNC_PROTO => (0..usize::from(btf_type.vlen)).all(|index| {
                    let offset = btf_type.payload_offset + index * 8;
                    read_u32(self.data, offset).is_some_and(|name| self.string(name).is_some())
                        && read_u32(self.data, offset + 4)
                            .is_some_and(|type_id| self.valid_type_id(type_id))
                }),
                BTF_KIND_DATASEC => (0..usize::from(btf_type.vlen)).all(|index| {
                    read_u32(self.data, btf_type.payload_offset + index * 12)
                        .and_then(|type_id| self.type_by_id(type_id))
                        .is_some_and(|variable| variable.kind == BTF_KIND_VAR)
                }),
                BTF_KIND_ENUM64 => (0..usize::from(btf_type.vlen)).all(|index| {
                    read_u32(self.data, btf_type.payload_offset + index * 12)
                        .is_some_and(|name| self.string(name).is_some())
                }),
                _ => true,
            }
        })
    }

    fn strip_modifiers(&self, mut type_id: u32) -> Option<u32> {
        for _ in 0..self.types.len() {
            let Some(btf_type) = self.type_by_id(type_id) else {
                return (type_id == 0).then_some(0);
            };
            if matches!(
                btf_type.kind,
                BTF_KIND_TYPEDEF
                    | BTF_KIND_VOLATILE
                    | BTF_KIND_CONST
                    | BTF_KIND_RESTRICT
                    | BTF_KIND_TYPE_TAG
            ) {
                type_id = btf_type.size_or_type;
            } else {
                return Some(type_id);
            }
        }
        None
    }

    fn type_size(&self, type_id: u32) -> Option<u64> {
        self.type_size_inner(type_id, 0)
    }

    fn type_size_inner(&self, type_id: u32, depth: usize) -> Option<u64> {
        if depth > 64 {
            return None;
        }
        let type_id = self.strip_modifiers(type_id)?;
        let btf_type = self.type_by_id(type_id)?;
        match btf_type.kind {
            BTF_KIND_INT | BTF_KIND_STRUCT | BTF_KIND_UNION | BTF_KIND_ENUM | BTF_KIND_FLOAT
            | BTF_KIND_ENUM64 => Some(u64::from(btf_type.size_or_type)),
            BTF_KIND_PTR => Some(ARM64_POINTER_SIZE),
            BTF_KIND_ARRAY => {
                let element_type = read_u32(self.data, btf_type.payload_offset)?;
                let count = read_u32(self.data, btf_type.payload_offset + 8)?;
                self.type_size_inner(element_type, depth + 1)?
                    .checked_mul(u64::from(count))
            }
            _ => None,
        }
    }

    fn points_to_named_struct(&self, type_id: u32, expected_name: &str) -> bool {
        let Some(type_id) = self.strip_modifiers(type_id) else {
            return false;
        };
        let Some(pointer) = self.type_by_id(type_id) else {
            return false;
        };
        if pointer.kind != BTF_KIND_PTR {
            return false;
        }
        let Some(pointee_id) = self.strip_modifiers(pointer.size_or_type) else {
            return false;
        };
        self.type_by_id(pointee_id).is_some_and(|pointee| {
            pointee.kind == BTF_KIND_STRUCT
                && self.string(pointee.name_offset) == Some(expected_name)
        })
    }

    fn member_offset(&self, structure: &BtfType, member_index: usize) -> Option<u64> {
        let raw = read_u32(self.data, structure.payload_offset + member_index * 12 + 8)?;
        if structure.kind_flag && raw >> 24 != 0 {
            return None;
        }
        let bit_offset = if structure.kind_flag {
            raw & 0x00ff_ffff
        } else {
            raw
        };
        bit_offset
            .is_multiple_of(8)
            .then_some(u64::from(bit_offset / 8))
    }

    fn load_info_layout(&self) -> Result<Option<LoadInfoLayout>> {
        let mut named_structs = 0usize;
        let mut layouts = BTreeSet::new();
        for structure in self.types.iter().filter(|btf_type| {
            btf_type.kind == BTF_KIND_STRUCT
                && self.string(btf_type.name_offset) == Some("load_info")
        }) {
            named_structs += 1;
            let mut hdr = None;
            let mut len = None;
            for index in 0..usize::from(structure.vlen) {
                let member_offset = structure.payload_offset + index * 12;
                let Some(name_offset) = read_u32(self.data, member_offset) else {
                    continue;
                };
                let Some(name) = self.string(name_offset) else {
                    continue;
                };
                if !matches!(name, "hdr" | "len") {
                    continue;
                }
                let member_type = read_u32(self.data, member_offset + 4)
                    .ok_or_else(|| anyhow::anyhow!("BTF load_info member type is truncated"))?;
                ensure!(
                    self.type_size(member_type) == Some(ARM64_POINTER_SIZE),
                    "BTF struct load_info.{name} is not eight bytes"
                );
                let offset = self.member_offset(structure, index).ok_or_else(|| {
                    anyhow::anyhow!("BTF struct load_info.{name} is not byte-aligned")
                })?;
                if name == "hdr" {
                    hdr = Some(offset);
                } else {
                    len = Some(offset);
                }
            }
            let Some((hdr_offset, len_offset)) = hdr.zip(len) else {
                continue;
            };
            let structure_size = u64::from(structure.size_or_type);
            let hdr_end = hdr_offset.checked_add(ARM64_POINTER_SIZE);
            let len_end = len_offset.checked_add(ARM64_POINTER_SIZE);
            ensure!(
                hdr_end.is_some_and(|end| end <= structure_size)
                    && len_end.is_some_and(|end| end <= structure_size),
                "BTF struct load_info fields exceed the structure size"
            );
            layouts.insert(LoadInfoLayout {
                structure_size,
                hdr_offset,
                len_offset,
            });
        }
        if named_structs == 0 {
            return Ok(None);
        }
        ensure!(
            !layouts.is_empty(),
            "BTF contains struct load_info without usable hdr/len members"
        );
        ensure!(
            layouts.len() == 1,
            "BTF contains conflicting struct load_info layouts"
        );
        Ok(layouts.into_iter().next())
    }

    fn loading_module_id(&self) -> Result<Option<u64>> {
        let mut values = BTreeSet::new();
        for enumeration in self
            .types
            .iter()
            .filter(|btf_type| matches!(btf_type.kind, BTF_KIND_ENUM | BTF_KIND_ENUM64))
        {
            let stride = if enumeration.kind == BTF_KIND_ENUM {
                8
            } else {
                12
            };
            for index in 0..usize::from(enumeration.vlen) {
                let offset = enumeration.payload_offset + index * stride;
                let Some(name_offset) = read_u32(self.data, offset) else {
                    continue;
                };
                if self.string(name_offset) != Some("LOADING_MODULE") {
                    continue;
                }
                let value = if enumeration.kind == BTF_KIND_ENUM {
                    let raw = read_u32(self.data, offset + 4)
                        .ok_or_else(|| anyhow::anyhow!("BTF enum value is truncated"))?;
                    if enumeration.kind_flag {
                        let signed = i64::from(i32::from_le_bytes(raw.to_le_bytes()));
                        ensure!(signed >= 0, "BTF LOADING_MODULE is negative");
                        u64::try_from(signed)?
                    } else {
                        u64::from(raw)
                    }
                } else {
                    let low = u64::from(
                        read_u32(self.data, offset + 4)
                            .ok_or_else(|| anyhow::anyhow!("BTF enum64 value is truncated"))?,
                    );
                    let high = u64::from(
                        read_u32(self.data, offset + 8)
                            .ok_or_else(|| anyhow::anyhow!("BTF enum64 value is truncated"))?,
                    );
                    let raw = low | (high << 32);
                    if enumeration.kind_flag {
                        let signed = i64::from_le_bytes(raw.to_le_bytes());
                        ensure!(signed >= 0, "BTF LOADING_MODULE is negative");
                        u64::try_from(signed)?
                    } else {
                        raw
                    }
                };
                values.insert(value);
            }
        }
        ensure!(
            values.len() <= 1,
            "BTF contains conflicting LOADING_MODULE values"
        );
        Ok(values.into_iter().next())
    }

    fn validate_function_abis(&self) -> Result<()> {
        const EXPECTED_ARITIES: &[(&str, u16)] = &[
            ("load_module", 3),
            ("security_kernel_load_data", 2),
            ("security_kernel_post_load_data", 4),
            ("capable", 1),
            ("vmalloc", 1),
            ("vmalloc_noprof", 1),
            ("memcpy", 3),
            ("vfree", 1),
            ("kstrdup", 2),
            ("strndup_user", 2),
            ("memblock_reserve", 2),
        ];

        for &(name, expected_arity) in EXPECTED_ARITIES {
            for function in self.types.iter().filter(|btf_type| {
                btf_type.kind == BTF_KIND_FUNC && self.string(btf_type.name_offset) == Some(name)
            }) {
                let prototype = self
                    .type_by_id(function.size_or_type)
                    .ok_or_else(|| anyhow::anyhow!("BTF function {name} has no prototype"))?;
                ensure!(
                    prototype.kind == BTF_KIND_FUNC_PROTO && prototype.vlen == expected_arity,
                    "BTF function {name} has {} parameters; expected {expected_arity}",
                    prototype.vlen
                );
                if name == "load_module" {
                    let first_parameter = read_u32(self.data, prototype.payload_offset + 4)
                        .ok_or_else(|| anyhow::anyhow!("BTF load_module prototype is truncated"))?;
                    ensure!(
                        self.points_to_named_struct(first_parameter, "load_info"),
                        "BTF load_module first parameter is not struct load_info *"
                    );
                }
            }
        }
        Ok(())
    }

    pub const fn file_offset(&self) -> usize {
        self.file_offset
    }

    pub const fn size(&self) -> usize {
        self.blob_end - self.file_offset
    }

    pub fn to_kernel_btf(&self) -> Result<KernelBtf> {
        let load_info = self.load_info_layout()?;
        let loading_module_id = self.loading_module_id()?;
        self.validate_function_abis()?;
        Ok(KernelBtf {
            file_offset: self.file_offset,
            size: self.size(),
            type_count: self.types.len(),
            load_info,
            loading_module_id,
        })
    }
}

pub fn find_btf_candidates(image: &[u8]) -> Vec<BtfCandidate<'_>> {
    let mut candidates = Vec::new();
    let mut search_from = 0usize;
    while search_from <= image.len().saturating_sub(BTF_MAGIC_BYTES.len()) {
        let Some(relative) = image[search_from..]
            .windows(BTF_MAGIC_BYTES.len())
            .position(|window| window == BTF_MAGIC_BYTES)
        else {
            break;
        };
        let file_offset = search_from + relative;
        if let Some(candidate) = BtfCandidate::parse(image, file_offset) {
            candidates.push(candidate);
        }
        search_from = file_offset.saturating_add(1);
    }
    candidates
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Default)]
    struct Strings {
        data: Vec<u8>,
    }

    impl Strings {
        fn new() -> Self {
            Self { data: vec![0] }
        }

        fn add(&mut self, value: &str) -> u32 {
            let offset = u32::try_from(self.data.len()).unwrap();
            self.data.extend_from_slice(value.as_bytes());
            self.data.push(0);
            offset
        }
    }

    fn push_u32(output: &mut Vec<u8>, value: u32) {
        output.extend_from_slice(&value.to_le_bytes());
    }

    fn push_type(output: &mut Vec<u8>, name: u32, kind: u8, vlen: u16, size_or_type: u32) {
        push_u32(output, name);
        push_u32(output, (u32::from(kind) << 24) | u32::from(vlen));
        push_u32(output, size_or_type);
    }

    fn build_btf(
        load_info_layouts: &[(u32, u32, u32)],
        loading_modules: &[u32],
        arity: u16,
    ) -> Vec<u8> {
        let mut strings = Strings::new();
        let unsigned_long = strings.add("unsigned long");
        let hdr = strings.add("hdr");
        let len = strings.add("len");
        let load_info = strings.add("load_info");
        let enum_name = strings.add("kernel_load_data_id");
        let loading_module_name = strings.add("LOADING_MODULE");
        let load_module = strings.add("load_module");
        let parameter = strings.add("parameter");

        let mut types = Vec::new();
        push_type(&mut types, unsigned_long, BTF_KIND_INT, 0, 8);
        push_u32(&mut types, 64);
        push_type(&mut types, 0, BTF_KIND_PTR, 0, 1);
        for &(size, hdr_offset, len_offset) in load_info_layouts {
            push_type(&mut types, load_info, BTF_KIND_STRUCT, 2, size);
            push_u32(&mut types, hdr);
            push_u32(&mut types, 2);
            push_u32(&mut types, hdr_offset * 8);
            push_u32(&mut types, len);
            push_u32(&mut types, 1);
            push_u32(&mut types, len_offset * 8);
        }
        if !loading_modules.is_empty() {
            push_type(
                &mut types,
                enum_name,
                BTF_KIND_ENUM,
                u16::try_from(loading_modules.len()).unwrap(),
                4,
            );
            for &value in loading_modules {
                push_u32(&mut types, loading_module_name);
                push_u32(&mut types, value);
            }
        }
        if !load_info_layouts.is_empty() {
            let first_load_info_id = 3u32;
            let pointer_id = u32::try_from(3 + load_info_layouts.len()).unwrap()
                + u32::from(!loading_modules.is_empty());
            push_type(&mut types, 0, BTF_KIND_PTR, 0, first_load_info_id);
            let prototype_id = pointer_id + 1;
            push_type(&mut types, 0, BTF_KIND_FUNC_PROTO, arity, 1);
            for index in 0..arity {
                push_u32(&mut types, parameter);
                push_u32(&mut types, if index == 0 { pointer_id } else { 1 });
            }
            push_type(&mut types, load_module, BTF_KIND_FUNC, 0, prototype_id);
        }

        let mut output = Vec::new();
        output.extend_from_slice(&BTF_MAGIC.to_le_bytes());
        output.push(BTF_VERSION);
        output.push(0);
        push_u32(&mut output, BTF_HEADER_SIZE as u32);
        push_u32(&mut output, 0);
        push_u32(&mut output, u32::try_from(types.len()).unwrap());
        push_u32(&mut output, u32::try_from(types.len()).unwrap());
        push_u32(&mut output, u32::try_from(strings.data.len()).unwrap());
        output.extend_from_slice(&types);
        output.extend_from_slice(&strings.data);
        output
    }

    fn recover_single_btf(image: &[u8]) -> Result<KernelBtf> {
        let candidates = find_btf_candidates(image);
        assert_eq!(candidates.len(), 1);
        candidates[0].to_kernel_btf()
    }

    #[test]
    fn recovers_load_info_and_loading_module() {
        let blob = build_btf(&[(136, 16, 24)], &[2], 3);
        let mut image = vec![0xaa; 32];
        image.extend_from_slice(&blob);
        let btf = recover_single_btf(&image).unwrap();
        assert_eq!(btf.file_offset, 32);
        assert_eq!(btf.size, blob.len());
        assert_eq!(
            btf.load_info,
            Some(LoadInfoLayout {
                structure_size: 136,
                hdr_offset: 16,
                len_offset: 24,
            })
        );
        assert_eq!(btf.loading_module_id, Some(2));
    }

    #[test]
    fn ignores_false_magic_before_valid_btf() {
        let mut image = BTF_MAGIC_BYTES.to_vec();
        image.extend_from_slice(&[0; 20]);
        image.extend_from_slice(&build_btf(&[(136, 16, 24)], &[2], 3));
        assert_eq!(find_btf_candidates(&image).len(), 1);
    }

    #[test]
    fn returns_multiple_valid_btf_candidates() {
        let blob = build_btf(&[(136, 16, 24)], &[2], 3);
        let mut image = blob.clone();
        image.extend_from_slice(&blob);
        let candidates = find_btf_candidates(&image);
        assert_eq!(candidates.len(), 2);
        assert_eq!(candidates[0].file_offset(), 0);
        assert_eq!(candidates[1].file_offset(), blob.len());
    }

    #[test]
    fn malformed_btf_is_treated_as_unavailable() {
        assert!(find_btf_candidates(BTF_MAGIC_BYTES).is_empty());
    }

    #[test]
    fn rejects_conflicting_load_info_layouts() {
        let blob = build_btf(&[(136, 16, 24), (152, 24, 32)], &[2], 3);
        assert!(
            recover_single_btf(&blob)
                .unwrap_err()
                .to_string()
                .contains("conflicting struct load_info layouts")
        );
    }

    #[test]
    fn rejects_incompatible_load_module_arity() {
        let blob = build_btf(&[(136, 16, 24)], &[2], 2);
        assert!(
            recover_single_btf(&blob)
                .unwrap_err()
                .to_string()
                .contains("load_module has 2 parameters")
        );
    }

    #[test]
    fn missing_abi_records_use_callers_fallback() {
        let blob = build_btf(&[], &[], 0);
        let btf = recover_single_btf(&blob).unwrap();
        assert_eq!(btf.load_info, None);
        assert_eq!(btf.loading_module_id, None);
    }

    #[test]
    fn rejects_conflicting_loading_module_values() {
        let blob = build_btf(&[(136, 16, 24)], &[2, 3], 3);
        assert!(
            recover_single_btf(&blob)
                .unwrap_err()
                .to_string()
                .contains("conflicting LOADING_MODULE values")
        );
    }
}
