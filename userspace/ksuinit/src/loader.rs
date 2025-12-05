use anyhow::{Context, Result};
use goblin::elf::{section_header, sym::Sym, Elf};
use rustix::{cstr, system::init_module};
use scroll::{ctx::SizeWith, Pwrite};
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
                    .find("$").or_else(|| symbol.find(".llvm."))
                    .map_or(symbol, |pos| &symbol[0..pos])
                    .to_owned(),
                addr,
            )
        })
        .collect::<HashMap<_, _>>();

    Ok(allsyms)
}

pub fn load_module(path: &str) -> Result<()> {
    // check if self is init process(pid == 1)
    if !rustix::process::getpid().is_init() {
        anyhow::bail!("{}", "Invalid process");
    }

    let mut buffer =
        fs::read(path).with_context(|| format!("Cannot read file {}", path))?;
    let elf = Elf::parse(&buffer)?;

    let kernel_symbols =
        parse_kallsyms().context("Cannot parse kallsyms")?;

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
    init_module(&buffer, cstr!("")).context("init_module failed.")?;
    Ok(())
}
