#!/usr/bin/bash

set -e

KMIS=(android12-5.10 android13-5.10 android13-5.15 android14-5.15 android14-6.1 android15-6.6 android16-6.12 android17-6.18)
CLANGS=(clang-r416183b clang-r450784e clang-r450784e clang-r487747c clang-r487747c clang-r510928 clang-r536225 clang-r584948c)
RUSTS=(none none none none none none rust-1.82.0 rust-1.91.1.p3)


for i in "${!KMIS[@]}"; do
    kmi=${KMIS[i]}
    clangv=${CLANGS[i]}
    rustv=${RUSTS[i]}
    echo "$kmi clang=$clangv rust=$rustv"

    SRC=$(realpath /opt/ddk/src/"$kmi")
    KDIR=/opt/ddk/kdir-x64/$kmi
    mkdir -p "$KDIR"

    export CROSS_COMPILE=x86_64-linux-gnu-
    export ARCH=x86_64
    export LLVM=1
    export LLVM_IAS=1

    ORIG_PATH=$PATH

    CLANG_PATH=$(realpath /opt/ddk/clang/"$clangv"/bin)
    NEW_PATH=$CLANG_PATH

    if [ "$rustv" != "none" ]; then
        RUST_PATH=$(realpath /opt/ddk/rust/"$rustv"/bin)
        NEW_PATH=$NEW_PATH:$RUST_PATH
    fi
    echo "$NEW_PATH"

    NEW_PATH=$NEW_PATH:$PATH
    export PATH=$NEW_PATH

    pushd "$SRC"

    # ddk-min source archives may predate the modpost fix used by the
    # full kdir.  Apply all four edits before modules_prepare builds the
    # host modpost binary.  Each edit is idempotent for updated archives.
    MODPOST=scripts/mod/modpost.c
    sed -i -E 's/^([[:space:]]*)check_exports\(mod\);/\1\/\/ check_exports(mod);/' "$MODPOST"
    sed -i -E 's/^([[:space:]]*)s->module = exp->module;/\1\/\/ s->module = exp->module;/' "$MODPOST"
    sed -i 's/^static void check_exports(/static void __attribute__((unused)) check_exports(/' "$MODPOST"
    sed -i 's/__version_ext_names\\") =\\n/__version_ext_names\\") = \\"\\"\\n/' "$MODPOST"

    make "O=$KDIR" gki_defconfig
    scripts/config --file "$KDIR/.config" -d LTO_CLANG -e LTO_NONE -d LTO_CLANG_THIN -d LTO_CLANG_FULL -d THINLTO
    if [ "$kmi" == "android16-6.12" ] || [ "$kmi" == "android17-6.18" ]; then
        scripts/config --file "$KDIR/.config" -e CONFIG_CFI_ICALL_NORMALIZE_INTEGERS
    fi
    make "O=$KDIR" modules_prepare
    # Let Kbuild generate the SELinux headers in this x86 output tree.
    make "O=$KDIR" security/selinux/built-in.a
    popd

    PATH=$ORIG_PATH
done
