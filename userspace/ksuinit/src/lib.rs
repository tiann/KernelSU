use anyhow::{Context, Result, bail, ensure};
use goblin::elf::{Elf, header, section_header, sym::Sym};
use rustix::system::init_module;
use scroll::{Pwrite, ctx::SizeWith};
use std::collections::HashMap;
use std::ffi::CStr;
use std::fs::{self, File, OpenOptions};
use std::io::{BufRead, BufReader, ErrorKind, Read, Seek, SeekFrom};
use std::os::unix::fs::OpenOptionsExt;

struct Kptr {
    value: String,
}

impl Kptr {
    pub fn new() -> Result<Self> {
        let value = fs::read_to_string("/proc/sys/kernel/kptr_restrict")?;
        fs::write("/proc/sys/kernel/kptr_restrict", "1")?;
        Ok(Kptr { value })
    }
}

impl Drop for Kptr {
    fn drop(&mut self) {
        let _ = fs::write("/proc/sys/kernel/kptr_restrict", self.value.as_bytes());
    }
}

pub struct KptrOwnedIter<I> {
    _kptr: Kptr,
    iter: I,
}

impl<I: Iterator> Iterator for KptrOwnedIter<I> {
    type Item = I::Item;

    fn next(&mut self) -> Option<Self::Item> {
        self.iter.next()
    }
}

pub fn kernel_symbols_iter() -> Result<impl Iterator<Item = (String, u64)>> {
    let kptr = Kptr::new()?;

    let iter = BufReader::new(File::open("/proc/kallsyms")?)
        .lines()
        // https://github.com/torvalds/linux/blob/7f87a5ea75f011d2c9bc8ac0167e5e2d1adb1594/kernel/kallsyms.c#L727
        // We can stop read as soon as we read all kernel symbols
        .map_while(|line| {
            line.ok().and_then(|line| {
                let mut splits = line.split_whitespace();
                splits
                    .next()
                    .and_then(|addr| u64::from_str_radix(addr, 16).ok())
                    .and_then(|addr| {
                        splits
                            .nth(1)
                            .take_if(|_| splits.next().is_none()) // stop at module symbols
                            .map(|symbol| {
                                (
                                    symbol
                                        .find("$")
                                        .or_else(|| symbol.find(".llvm."))
                                        .map(|pos| &symbol[0..pos])
                                        .unwrap_or(symbol)
                                        .to_owned(),
                                    addr,
                                )
                            })
                    })
            })
        });

    Ok(KptrOwnedIter { _kptr: kptr, iter })
}

pub fn for_each_kernel_symbols<F: FnMut(&(String, u64)) -> Result<bool>>(mut f: F) -> Result<()> {
    for item in kernel_symbols_iter()? {
        if !f(&item)? {
            break;
        }
    }
    Ok(())
}

const O_NONBLOCK: i32 = 0x800;

fn open_kmsg_at_end() -> Result<File> {
    let mut last_error = None;

    for path in ["/dev/kmsg", "/kmsg"] {
        match OpenOptions::new()
            .read(true)
            .custom_flags(O_NONBLOCK)
            .open(path)
        {
            Ok(mut file) => {
                file.seek(SeekFrom::End(0))
                    .with_context(|| format!("Cannot seek {path} to end"))?;

                log::info!("Reading kernel log from {path}");
                return Ok(file);
            }
            Err(error) => {
                last_error = Some((path, error));
            }
        }
    }

    match last_error {
        Some((path, error)) => {
            Err(error).with_context(|| format!("Cannot open kernel log device, last tried {path}"))
        }
        None => bail!("No kernel log device candidate"),
    }
}

fn read_new_kmsg(file: &mut File) -> Result<String> {
    let mut output = Vec::new();
    let mut record = [0u8; 8192];

    loop {
        match file.read(&mut record) {
            Ok(0) => break,
            Ok(length) => {
                output.extend_from_slice(&record[..length]);
                output.push(b'\n');
            }
            Err(error) if error.kind() == ErrorKind::WouldBlock => break,
            Err(error) => return Err(error).context("Cannot read /dev/kmsg"),
        }
    }

    Ok(String::from_utf8_lossy(&output).into_owned())
}

fn extract_required_vermagic(kmsg: &str) -> Option<String> {
    const PREFIX: &str = "version magic '";
    const SEPARATOR: &str = "' should be '";

    for record in kmsg.lines().rev() {
        let message = record
            .split_once(';')
            .map(|(_, message)| message)
            .unwrap_or(record);

        let Some(prefix_position) = message.find(PREFIX) else {
            continue;
        };
        let after_prefix = &message[prefix_position + PREFIX.len()..];

        let Some(separator_position) = after_prefix.find(SEPARATOR) else {
            continue;
        };
        let required = &after_prefix[separator_position + SEPARATOR.len()..];

        let Some(end_quote) = required.find('\'') else {
            continue;
        };
        let required = &required[..end_quote];

        if !required.is_empty() {
            return Some(required.to_owned());
        }
    }

    None
}

fn align_up(value: usize, alignment: usize) -> Result<usize> {
    let alignment = alignment.max(1);

    if !alignment.is_power_of_two() {
        bail!("Invalid ELF alignment: {alignment}");
    }

    value
        .checked_add(alignment - 1)
        .map(|value| value & !(alignment - 1))
        .context("ELF alignment overflow")
}

fn write_elf64_word(
    buffer: &mut [u8],
    offset: usize,
    value: u64,
    little_endian: bool,
) -> Result<()> {
    let end = offset.checked_add(8).context("ELF write overflow")?;
    let destination = buffer
        .get_mut(offset..end)
        .context("ELF write outside module buffer")?;

    let bytes = if little_endian {
        value.to_le_bytes()
    } else {
        value.to_be_bytes()
    };
    destination.copy_from_slice(&bytes);
    Ok(())
}

fn replace_module_vermagic(buffer: &mut Vec<u8>, required_vermagic: &str) -> Result<()> {
    struct ModinfoLocation {
        offset: usize,
        size: usize,
        section_header_offset: usize,
        alignment: usize,
        little_endian: bool,
    }

    let location = {
        let elf = Elf::parse(buffer)?;

        if !elf.is_64 {
            bail!("Only ELF64 modules are supported");
        }

        let section_table_offset =
            usize::try_from(elf.header.e_shoff).context("Section table offset overflow")?;
        let section_entry_size = usize::from(elf.header.e_shentsize);
        let mut location = None;

        for (index, section) in elf.section_headers.iter().enumerate() {
            let Some(name) = elf.shdr_strtab.get_at(section.sh_name) else {
                continue;
            };
            if name != ".modinfo" {
                continue;
            }

            let offset = usize::try_from(section.sh_offset).context(".modinfo offset overflow")?;
            let size = usize::try_from(section.sh_size).context(".modinfo size overflow")?;
            let end = offset
                .checked_add(size)
                .context(".modinfo range overflow")?;

            if end > buffer.len() {
                bail!(".modinfo is outside module buffer");
            }

            let section_header_offset = section_table_offset
                .checked_add(
                    index
                        .checked_mul(section_entry_size)
                        .context("Section index overflow")?,
                )
                .context("Section header offset overflow")?;

            location = Some(ModinfoLocation {
                offset,
                size,
                section_header_offset,
                alignment: usize::try_from(section.sh_addralign).unwrap_or(1).max(1),
                little_endian: elf.little_endian,
            });
            break;
        }

        location.context("Module has no .modinfo section")?
    };

    let old_modinfo = &buffer[location.offset..location.offset + location.size];
    let replacement = format!("vermagic={required_vermagic}");
    let mut new_modinfo = Vec::with_capacity(old_modinfo.len().max(replacement.len() + 1));
    let mut replaced = false;

    for entry in old_modinfo.split(|byte| *byte == 0) {
        if entry.is_empty() {
            continue;
        }

        if entry.starts_with(b"vermagic=") {
            if !replaced {
                new_modinfo.extend_from_slice(replacement.as_bytes());
                new_modinfo.push(0);
                replaced = true;
            }
        } else {
            new_modinfo.extend_from_slice(entry);
            new_modinfo.push(0);
        }
    }

    if !replaced {
        new_modinfo.extend_from_slice(replacement.as_bytes());
        new_modinfo.push(0);
    }

    let new_offset = align_up(buffer.len(), location.alignment)?;
    buffer.resize(new_offset, 0);
    buffer.extend_from_slice(&new_modinfo);

    // Elf64_Shdr: sh_offset at +0x18, sh_size at +0x20.
    write_elf64_word(
        buffer,
        location.section_header_offset + 0x18,
        new_offset as u64,
        location.little_endian,
    )?;
    write_elf64_word(
        buffer,
        location.section_header_offset + 0x20,
        new_modinfo.len() as u64,
        location.little_endian,
    )?;

    log::warn!(
        "Replaced module vermagic with kernel-required value: {:?}",
        required_vermagic
    );
    Ok(())
}

fn validate_module_elf(elf: &Elf<'_>, machine: u16) -> Result<()> {
    ensure!(elf.is_64 && elf.little_endian, "Expected a little-endian ELF64 kernel module");
    ensure!(elf.header.e_type == header::ET_REL, "Expected an ELF relocatable kernel module");
    ensure!(elf.header.e_machine == machine, "Kernel module architecture does not match the loader");
    Ok(())
}

fn native_elf_machine() -> Result<u16> {
    match std::env::consts::ARCH {
        "aarch64" => Ok(header::EM_AARCH64),
        "x86_64" => Ok(header::EM_X86_64),
        "riscv64" => Ok(header::EM_RISCV),
        arch => bail!("Unsupported kernel module architecture: {arch}"),
    }
}

/// Resolve undefined symbols in an ELF kernel module buffer using /proc/kallsyms,
/// then load it via init_module syscall.
/// Architecture relocations (including RISC-V HI20/LO12 pairs and PLT entries)
/// are left intact for the kernel's module loader.
pub fn load_module(data: &[u8], params: &CStr) -> Result<()> {
    let mut buffer = data.to_vec();
    let elf = Elf::parse(&buffer)?;
    // Reject wrong-architecture input before changing kptr_restrict or asking
    // the kernel to load anything.
    validate_module_elf(&elf, native_elf_machine()?)?;
    let ctx = *elf.syms.ctx();

    let mut unresolved_symbols: HashMap<String, (Sym, usize)> = HashMap::new();
    for (index, sym) in elf.syms.iter().enumerate() {
        if index == 0 {
            continue;
        }

        if sym.st_shndx != section_header::SHN_UNDEF as usize {
            continue;
        }

        let Some(name) = elf.strtab.get_at(sym.st_name) else {
            continue;
        };

        let offset = elf.syms.offset() + index * Sym::size_with(elf.syms.ctx());
        unresolved_symbols.insert(name.to_owned(), (sym, offset));
    }

    if !unresolved_symbols.is_empty() {
        for_each_kernel_symbols(|(symbol, addr)| {
            if *addr == 0 {
                return Ok(true);
            }
            if let Some((mut sym, offset)) = unresolved_symbols.remove(symbol) {
                sym.st_shndx = section_header::SHN_ABS as usize;
                sym.st_value = *addr;
                buffer.pwrite_with(sym, offset, ctx)?;
            }

            Ok(!unresolved_symbols.is_empty())
        })
        .context("Cannot parse kallsyms")?;
    }

    for name in unresolved_symbols.keys() {
        log::warn!("Cannot find symbol: {}", name);
    }

    let mut kmsg = match open_kmsg_at_end() {
        Ok(file) => Some(file),
        Err(error) => {
            log::warn!("Cannot prepare kmsg fallback: {error:#}");
            None
        }
    };

    match init_module(&buffer, params) {
        Ok(()) => Ok(()),
        Err(first_error) => {
            let logs = match kmsg.as_mut() {
                Some(file) => read_new_kmsg(file).unwrap_or_default(),
                None => String::new(),
            };

            let Some(required_vermagic) = extract_required_vermagic(&logs) else {
                log::error!("Kernel module loading log:\n{}", logs);
                return Err(first_error).context("init_module failed without vermagic mismatch");
            };

            log::warn!(
                "Kernel requires vermagic {:?}; replacing and retrying",
                required_vermagic
            );

            replace_module_vermagic(&mut buffer, &required_vermagic)
                .context("Cannot replace module vermagic")?;

            init_module(&buffer, params).context("init_module failed after replacing vermagic")?;
            Ok(())
        }
    }
}

fn has_kernelsu_legacy() -> bool {
    use syscalls::{Sysno, syscall};
    let mut version = 0;
    const CMD_GET_VERSION: i32 = 2;
    unsafe {
        let _ = syscall!(
            Sysno::prctl,
            0xDEADBEEF,
            CMD_GET_VERSION,
            std::ptr::addr_of_mut!(version)
        );
    }

    log::info!("KernelSU version: {}", version);

    version != 0
}

fn has_kernelsu_v2() -> bool {
    use syscalls::{Sysno, syscall};
    const KSU_INSTALL_MAGIC1: u32 = 0xDEADBEEF;
    const KSU_INSTALL_MAGIC2: u32 = 0xCAFEBABE;
    const KSU_IOCTL_GET_INFO: u32 = 0x80104b02; // _IOR('K', 2, struct ksu_get_info_cmd)
    const KSU_IOCTL_GET_INFO_LEGACY: u32 = 0x80004b02; // _IOC(_IOC_READ, 'K', 2, 0)

    #[repr(C)]
    #[derive(Default)]
    struct GetInfoCmd {
        version: u32,
        flags: u32,
        features: u32,
        uapi_version: u32,
    }

    #[repr(C)]
    #[derive(Default)]
    struct GetInfoLegacyCmd {
        version: u32,
        flags: u32,
        features: u32,
    }

    // Try new method: get driver fd using reboot syscall with magic numbers
    let mut fd: i32 = -1;
    unsafe {
        let _ = syscall!(
            Sysno::reboot,
            KSU_INSTALL_MAGIC1,
            KSU_INSTALL_MAGIC2,
            0,
            std::ptr::addr_of_mut!(fd)
        );
    }

    let version = if fd >= 0 {
        // New method: try to get version info via ioctl
        let mut cmd = GetInfoCmd::default();
        let version = unsafe {
            let ret = syscall!(Sysno::ioctl, fd, KSU_IOCTL_GET_INFO, &mut cmd as *mut _);

            match ret {
                Ok(_) => cmd.version,
                Err(_) => {
                    let mut cmd = GetInfoLegacyCmd::default();
                    match syscall!(
                        Sysno::ioctl,
                        fd,
                        KSU_IOCTL_GET_INFO_LEGACY,
                        &mut cmd as *mut _
                    ) {
                        Ok(_) => cmd.version,
                        Err(_) => 0,
                    }
                }
            }
        };

        unsafe {
            let _ = syscall!(Sysno::close, fd);
        }

        version
    } else {
        0
    };

    log::info!("KernelSU version: {}", version);

    version != 0
}

pub fn has_kernelsu() -> bool {
    has_kernelsu_v2() || has_kernelsu_legacy()
}

#[cfg(test)]
mod tests {
    use super::*;

    fn elf_header(machine: u16) -> Vec<u8> {
        let mut bytes = vec![0; 64];
        bytes[..7].copy_from_slice(b"\x7fELF\x02\x01\x01");
        bytes[16..18].copy_from_slice(&header::ET_REL.to_le_bytes());
        bytes[18..20].copy_from_slice(&machine.to_le_bytes());
        bytes[20..24].copy_from_slice(&1u32.to_le_bytes());
        bytes[52..54].copy_from_slice(&64u16.to_le_bytes());
        bytes[54..56].copy_from_slice(&56u16.to_le_bytes());
        bytes[58..60].copy_from_slice(&64u16.to_le_bytes());
        bytes
    }

    #[test]
    fn accepts_riscv_module_and_rejects_other_machines() {
        for machine in [header::EM_RISCV, header::EM_AARCH64, header::EM_X86_64] {
            let data = elf_header(machine);
            let elf = Elf::parse(&data).unwrap();
            assert!(validate_module_elf(&elf, machine).is_ok());
            assert_eq!(validate_module_elf(&elf, header::EM_RISCV).is_ok(), machine == header::EM_RISCV);
        }
    }

    #[test]
    fn rejects_executable_before_module_loading() {
        let mut data = elf_header(header::EM_RISCV);
        data[16..18].copy_from_slice(&header::ET_EXEC.to_le_bytes());
        let elf = Elf::parse(&data).unwrap();
        assert!(validate_module_elf(&elf, header::EM_RISCV).is_err());
    }

    #[test]
    fn rejects_wrong_architecture_without_kernel_access() {
        let machine = if native_elf_machine().unwrap() == header::EM_RISCV {
            header::EM_AARCH64
        } else {
            header::EM_RISCV
        };
        let error = load_module(&elf_header(machine), c"").unwrap_err();
        assert!(error.to_string().contains("architecture does not match"));
    }
}
