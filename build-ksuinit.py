
import os
from pathlib import Path
import re
from subprocess import Popen
import sys
import shutil

def version_key(version: str):
    nums = re.findall(r"\d+", version)
    return tuple(int(n) for n in nums) if nums else (0,)

def find_ndk_bin():
    ndk_root = os.environ.get("ANDROID_NDK_HOME") or os.environ.get("ANDROID_NDK")
    if not ndk_root:
        sdk_root = os.environ.get("ANDROID_SDK_ROOT") or os.environ.get("ANDROID_HOME")
        if sdk_root:
            ndk_dir = Path(sdk_root) / "ndk"
            if ndk_dir.exists():
                versions = sorted(
                    [d for d in ndk_dir.iterdir() if d.is_dir()],
                    key=lambda d: version_key(d.name),
                    reverse=True,
                )
                if versions:
                    ndk_root = str(versions[0])

    if ndk_root:
        toolchain_bin = Path(ndk_root) / "toolchains" / "llvm" / "prebuilt"
        if toolchain_bin.exists():
            return next(toolchain_bin.iterdir()) / "bin"

    raise FileNotFoundError('no android ndk bin found!')


def build_ksuinit(release):
    bin = find_ndk_bin()
    my_env = os.environ.copy()
    CLANG = 'aarch64-linux-android26-clang'
    STRIP = 'llvm-strip'
    if os.name == 'nt':
        CLANG += '.cmd'
        STRIP += '.exe'
    my_env['CARGO_TARGET_AARCH64_UNKNOWN_LINUX_MUSL_LINKER'] = str(bin / CLANG)
    my_env['RUSTFLAGS'] = '-C link-arg=-no-pie'
    args = ['cargo', 'build', '-p', 'ksuinit', '--target', 'aarch64-unknown-linux-musl']
    if release:
        args += ['--release']
    Popen(
        args,
        env=my_env
    ).wait()
    out_dir = Path('target/aarch64-unknown-linux-musl') / ('release' if release else 'debug') / 'ksuinit'
    ksud_bin = Path('userspace/ksud/bin/aarch64/ksuinit')
    shutil.copy(out_dir, ksud_bin)
    print('copy ksud', out_dir, '->', ksud_bin)
    Popen([str(bin / STRIP), str(ksud_bin)]).wait()
    print('stripped', ksud_bin)

if __name__ == '__main__':
    release = False
    if len(sys.argv) > 1:
        release = sys.argv[1] == '--release'
    build_ksuinit(release)
