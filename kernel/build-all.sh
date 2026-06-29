#!/bin/bash
set -e

if [ -z "$1" ]; then
    KMIS="android12-5.10 android13-5.10 android13-5.15 android14-5.15 android14-6.1 android15-6.6 android16-6.12"
else
    KMIS=$1
fi

# Some patch is required to use separate build dir when building for android16-6.12, see:
# https://github.com/5ec1cff/ddk#local-%E6%A8%A1%E5%BC%8F%E6%9E%84%E5%BB%BA%E9%80%82%E7%94%A8%E4%BA%8E%E5%A4%9A%E4%B8%AA-target-%E7%89%88%E6%9C%AC%E7%9A%84%E5%86%85%E6%A0%B8%E6%A8%A1%E5%9D%97

mv .ddk-version .ddk-version.bak 2> /dev/null || true

for kmi in $KMIS; do
    echo "========== Building $kmi =========="
    ODIR="$(realpath .)/out/$kmi"
    if ddk build "$kmi" "ODIR=$ODIR" -e CONFIG_KSU=m; then
        if [ -f "$ODIR/kernelsu.ko" ]; then
            cp "$ODIR/kernelsu.ko" "kernelsu-${kmi}.ko"
            llvm-strip -d "kernelsu-${kmi}.ko"
            echo "✓ Built kernelsu-${kmi}.ko"
        fi
    else
        echo "✗ Build failed for $kmi"
    fi
    echo ""
done

mv .ddk-version.bak .ddk-version 2> /dev/null || true

echo "========== Final output =========="
ls -l kernelsu-*.ko
