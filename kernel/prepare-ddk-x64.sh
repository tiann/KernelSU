#!/usr/bin/bash

set -e

KMIS=(android12-5.10 android13-5.10 android13-5.15 android14-5.15 android14-6.1 android15-6.6 android16-6.12)
CLANGS=(clang-r416183b clang-r450784e clang-r450784e clang-r487747c clang-r487747c clang-r510928 clang-r536225)
RUSTS=(none none none none none none rust-1.82.0)


for i in ${!KMIS[@]}; do
    kmi=${KMIS[i]}
    clangv=${CLANGS[i]}
    rustv=${RUSTS[i]}
    echo "$kmi clang=$clangv rust=$rustv"

    SRC=$(realpath /opt/ddk/src/$kmi)
    KDIR=/opt/ddk/kdir-x64/$kmi
    mkdir -p $KDIR
    ARM64_KDIR=$(realpath /opt/ddk/kdir/$kmi)

    export CROSS_COMPILE=x86_64-linux-gnu-
    export ARCH=x86_64
    export LLVM=1
    export LLVM_IAS=1

    ORIG_PATH=$PATH

    CLANG_PATH=$(realpath /opt/ddk/clang/$clangv/bin)
    NEW_PATH=$CLANG_PATH

    if [ "$rustv" != "none" ]; then
        RUST_PATH=$(realpath /opt/ddk/rust/$rustv/bin)
        NEW_PATH=$NEW_PATH:$RUST_PATH
    fi
    echo "$NEW_PATH"

    NEW_PATH=$NEW_PATH:$PATH
    export PATH=$NEW_PATH

    pushd $SRC
    make O=$KDIR gki_defconfig
    scripts/config --file $KDIR/.config -d LTO_CLANG -e LTO_NONE -d LTO_CLANG_THIN -d LTO_CLANG_FULL -d THINLTO
    if [ "$kmi" == "android16-6.12" ]; then
        scripts/config --file $KDIR/.config -e CONFIG_CFI_ICALL_NORMALIZE_INTEGERS
    fi
    make O=$KDIR modules_prepare
    popd

    SELINUX_PATH=security/selinux
    mkdir -p $KDIR/$SELINUX_PATH
    cp $ARM64_KDIR/$SELINUX_PATH/*.h $KDIR/$SELINUX_PATH

    PATH=$ORIG_PATH
done
