use anyhow::{Context, Result};
use goblin::elf::{Elf, section_header, sym::Sym};
use scroll::{Pwrite, ctx::SizeWith};
use std::collections::HashMap;
use std::fs;

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

fn parse_kallsyms() -> Result<HashMap<String, u64>> {
    let _dontdrop = Kptr::new()?;

    let allsyms = fs::read_to_string("/proc/kallsyms")?
        .lines()
        .map(|line| line.split_whitespace())
        .filter_map(|mut splits| {
            splits
                .next()
                .and_then(|addr| u64::from_str_radix(addr, 16).ok())
                .and_then(|addr| splits.nth(1).map(|symbol| (symbol, addr)))
        })
        .map(|(symbol, addr)| {
            (
                symbol
                    .find("$")
                    .or_else(|| symbol.find(".llvm."))
                    .map_or(symbol, |pos| &symbol[0..pos])
                    .to_owned(),
                addr,
            )
        })
        .collect::<HashMap<_, _>>();

    Ok(allsyms)
}

/// Relocate undefined symbols in an ELF kernel module buffer using /proc/kallsyms,
/// then load it via init_module syscall.
pub fn load_module(data: &[u8]) -> Result<()> {
    let mut buffer = data.to_vec();
    let elf = Elf::parse(&buffer)?;

    let kernel_symbols = parse_kallsyms().context("Cannot parse kallsyms")?;

    let mut modifications = Vec::new();
    for (index, mut sym) in elf.syms.iter().enumerate() {
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
        let Some(real_addr) = kernel_symbols.get(name) else {
            log::warn!("Cannot find symbol: {}", &name);
            continue;
        };
        sym.st_shndx = section_header::SHN_ABS as usize;
        sym.st_value = *real_addr;
        modifications.push((sym, offset));
    }

    let ctx = *elf.syms.ctx();
    for ele in modifications {
        buffer.pwrite_with(ele.0, ele.1, ctx)?;
    }

    rustix::system::init_module(&buffer, c"").context("init_module failed")?;
    Ok(())
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
    const KSU_IOCTL_GET_INFO: u32 = 0x80004b02; // _IOC(_IOC_READ, 'K', 2, 0)

    #[repr(C)]
    #[derive(Default)]
    struct GetInfoCmd {
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
                Err(_) => 0,
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
