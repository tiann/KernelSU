#!/bin/bash
set -e

if [ -z "$1" ]; then
    TARGETS="android12-5.10 android13-5.10 android13-5.15 android14-5.15 android14-6.1 android15-6.6 android16-6.12"
    #KMIS="android16-6.12"
else
    TARGETS=$1
fi

KMIS=(android12-5.10 android13-5.10 android13-5.15 android14-5.15 android14-6.1 android15-6.6 android16-6.12)
CLANGS=(clang-r416183b clang-r450784e clang-r450784e clang-r487747c clang-r487747c clang-r510928 clang-r536225)
RUSTS=(none none none none none none rust-1.82.0)

# Some patch is required to use separate build dir when building for android16-6.12, see:
# https://github.com/5ec1cff/ddk#local-%E6%A8%A1%E5%BC%8F%E6%9E%84%E5%BB%BA%E9%80%82%E7%94%A8%E4%BA%8E%E5%A4%9A%E4%B8%AA-target-%E7%89%88%E6%9C%AC%E7%9A%84%E5%86%85%E6%A0%B8%E6%A8%A1%E5%9D%97

export ARCH=x86_64
export LLVM=1
export LLVM_IAS=1

#mv .ddk-version .ddk-version.bak 2> /dev/null || true

for i in ${!KMIS[@]}; do
    kmi=${KMIS[i]}
    skip=true
    for t in $TARGETS; do
        if [ "$t" == "$kmi" ]; then
            skip=false
            break
        fi
    done
    if $skip; then
        continue
    fi
    clangv=${CLANGS[i]}
    rustv=${RUSTS[i]}
    echo "========== Building $kmi =========="
    ODIR="$(realpath .)/out-x64/$kmi"
    
    ORIG_PATH="$PATH"

    CLANG_PATH=$(realpath /opt/ddk/clang/$clangv/bin)
    NEW_PATH=$CLANG_PATH

    if [ "$rustv" != "none" ]; then
        RUST_PATH=$(realpath /opt/ddk/rust/$rustv/bin)
        NEW_PATH=$NEW_PATH:$RUST_PATH
    fi
    echo "$NEW_PATH"

    NEW_PATH=$NEW_PATH:$PATH
    export PATH=$NEW_PATH

    ODIR=$(realpath .)/out-x64/$kmi
    MDIR=$(realpath .)
    KDIR=/opt/ddk/kdir-x64/$kmi

    make -C $KDIR M=$ODIR src=$MDIR compile_commands.json modules CONFIG_KSU=m CONFIG_KSU_X86_PATCH_SYSCALL_DISPATCHER=y

    export PATH="$ORIG_PATH"
    echo ""
done

#mv .ddk-version.bak .ddk-version 2> /dev/null || true

echo "========== Final output =========="
ls -l out-x64/*/kernelsu.ko
