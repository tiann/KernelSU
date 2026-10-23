// SPDX-License-Identifier: GPL-2.0-only
//!
//! Little-endian buffer helpers shared by every architecture.

use anyhow::{Context, Result, anyhow, ensure};

pub const fn align_up(value: usize, alignment: usize) -> usize {
    (value + alignment - 1) & !(alignment - 1)
}

pub const fn align_up_u64(value: u64, alignment: u64) -> Option<u64> {
    match value.checked_add(alignment - 1) {
        Some(value) => Some(value & !(alignment - 1)),
        None => None,
    }
}

pub fn get_slice(data: &[u8], offset: usize, size: usize) -> Result<&[u8]> {
    data.get(offset..offset.saturating_add(size))
        .ok_or_else(|| anyhow!("offset 0x{offset:x} size 0x{size:x} is outside input"))
}

pub fn read_u16(data: &[u8], offset: usize) -> Result<u16> {
    Ok(u16::from_le_bytes(get_slice(data, offset, 2)?.try_into()?))
}

pub fn read_u32(data: &[u8], offset: usize) -> Result<u32> {
    Ok(u32::from_le_bytes(get_slice(data, offset, 4)?.try_into()?))
}

pub fn read_i32(data: &[u8], offset: usize) -> Result<i32> {
    Ok(i32::from_le_bytes(get_slice(data, offset, 4)?.try_into()?))
}

pub fn read_u64(data: &[u8], offset: usize) -> Result<u64> {
    Ok(u64::from_le_bytes(get_slice(data, offset, 8)?.try_into()?))
}

pub fn read_i64(data: &[u8], offset: usize) -> Result<i64> {
    Ok(i64::from_le_bytes(get_slice(data, offset, 8)?.try_into()?))
}

pub fn write_u32(data: &mut [u8], offset: usize, value: u32) -> Result<()> {
    get_slice_mut(data, offset, 4)?.copy_from_slice(&value.to_le_bytes());
    Ok(())
}

pub fn write_u64(data: &mut [u8], offset: usize, value: u64) -> Result<()> {
    get_slice_mut(data, offset, 8)?.copy_from_slice(&value.to_le_bytes());
    Ok(())
}

pub fn get_slice_mut(data: &mut [u8], offset: usize, size: usize) -> Result<&mut [u8]> {
    let end = offset
        .checked_add(size)
        .ok_or_else(|| anyhow!("offset overflow"))?;
    data.get_mut(offset..end)
        .ok_or_else(|| anyhow!("offset 0x{offset:x} size 0x{size:x} is outside output"))
}

pub fn find_subslice(data: &[u8], needle: &[u8], start: usize) -> Option<usize> {
    if needle.is_empty() || start > data.len() {
        return None;
    }
    data[start..]
        .windows(needle.len())
        .position(|window| window == needle)
        .map(|position| position + start)
}

pub fn read_c_string(data: &[u8], offset: usize) -> Option<String> {
    let tail = data.get(offset..)?;
    let end = tail.iter().position(|byte| *byte == 0)?;
    Some(String::from_utf8_lossy(&tail[..end]).into_owned())
}

pub fn checked_align_up(value: usize, alignment: usize) -> Result<usize> {
    ensure!(
        alignment.is_power_of_two(),
        "ELF section alignment is invalid"
    );
    value
        .checked_add(alignment - 1)
        .map(|aligned| aligned & !(alignment - 1))
        .context("ELF section layout overflow")
}
