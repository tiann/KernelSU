# RISC-V port (development)

This port targets little-endian RV64 Linux and the Android riscv64 ABI, not a
particular SoC or vendor instruction extension. CPU options and private NDK
paths belong in the caller's build environment, not the KernelSU sources.

## Kernel and LKM requirements

Use the exact target kernel source, configuration, generated headers and symbol
versions. There is no bundled, universal RISC-V GKI module. In addition to the
normal KernelSU prerequisites, enable `CONFIG_RISCV`, `CONFIG_64BIT`, `CONFIG_MMU`
and `CONFIG_KALLSYMS_ALL`. LKM builds also require `CONFIG_MODULES` and the
target's syscall tracepoint and kprobe support.

`CONFIG_KALLSYMS_ALL` is necessary for data symbols such as `sys_call_table`,
`text_mutex` and `init_mm`. A module built for a kernel without it is not a
working port; initialization explicitly refuses that configuration.

RISC-V syscall wrappers consume argument zero from `orig_a0`. The ordinary C
calling convention uses `a0`. The port keeps these separate and preserves
the original syscall number and arguments through tracepoint redirection.

The standard LKM installation path is supported in source:

1. Build `kernelsu.ko` against the exact target kernel using its normal external
   module build infrastructure and the KernelSU LKM symbol-resolution setup.
2. Build a **static** riscv64 `ksuinit`; it runs before `/system` is available.
3. Use `ksud boot-patch --arch riscv64 --boot <image> --module <kernelsu.ko>
   --init <ksuinit> --out <output-directory>` on a host. On Android, the
   architecture is selected by the running binary, so omit `--arch`.
4. Inspect and test the generated image using a recoverable boot path before
   deploying it. Keep the original image and a recovery method available.

`ksuinit` resolves kernel symbol addresses and then calls `init_module()`.
RISC-V relocations and PLT/GOT allocation remain the responsibility of Linux's
module loader. Plain `insmod` is not a substitute for KernelSU's symbol resolver.
Merely suppressing modpost errors does not prove the module will load.

The separate `boot-patch-v2` raw kernel Image injector remains **ARM64-only**;
it is not the standard ramdisk LKM path above. Do not use it on RISC-V images.

## Android userspace

Select a Rust toolchain and Android NDK/sysroot which provide
`riscv64-linux-android`. Use their Android ISA baseline unless the test device
requires explicit, separately maintained CPU flags. Build in release mode;
some Android toolchains ship a panic-abort-only standard library.

Provide matching binaries in `userspace/ksud/bin/riscv64/` before packaging:

- `busybox`: required for module scripts; a missing binary is reported at runtime.
- `ksuinit`: statically linked boot loader (or pass `--init` explicitly).
- `bootctl`: required for the A/B OTA installation workflow.

The source tree intentionally does not substitute ARM binaries for missing
RISC-V assets. A successful compile with the empty asset directory is not an
installable release. As with upstream host builds, assembling the embedded
ARM64 `boot-patch-v2` payload also needs an ARM64-capable assembler; this does
not change the architecture of the RISC-V executable.

Android's RISC-V signal context is generated from the selected NDK headers for
the SIGSYS handler, because the current Rust libc bindings lack that context.

The Manager has opt-in `-PKSU_ABIS=riscv64` and `-PKSU_NDK_PATH=<ndk>` build
properties. Populate `app/src/main/jniLibs/riscv64/libksud.so` first. RISC-V
builds use the NDK static C++ runtime instead of the ARM/x86-only libcxx prefab.
Default ARM64/x86_64 builds are unchanged.

## Validation status

This is not yet a production-qualified port. Kernel object/module cross builds,
register/decoder tests and userspace compile/startup checks are distinct from
loading the LKM, granting/denying root through the Manager, reboot persistence,
and safe unload. Those integration tests must pass before shipping an image.

Run `sh tests/riscv64/run.sh` for the architecture-helper tests. The fake
`pt_regs` is a field fixture, not a claim about the kernel's binary layout.
Run the `ksuinit` library tests on Linux or Android, not macOS. Also retain the
ARM64 `cargo ndk` check/clippy/fmt regression checks required by the repository.
