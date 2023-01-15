#! /bin/bash

set -x

GKI_ROOT=$(pwd)

echo "[+] GKI_ROOT: $GKI_ROOT"

if test -d "$GKI_ROOT/common/drivers"; then
     DRIVER_DIR="$GKI_ROOT/common/drivers"
elif test -d "$GKI_ROOT/drivers"; then
     DRIVER_DIR="$GKI_ROOT/drivers"
else
     echo "[ERROR] "drivers/" directory is not found."
     echo "[+] You should modify this scrpit by yourself."
     exit 127
fi

test -d "$GKI_ROOT/KernelSU" || git clone https://github.com/tiann/KernelSU
cd "$GKI_ROOT/KernelSU"
git stash && git pull
cd "$GKI_ROOT"

echo "[+] GKI_ROOT: $GKI_ROOT"
echo "[+] Copy kernel su driver to $DRIVER_DIR"

test -e "$DRIVER_DIR/kernelsu" || ln -sf "$GKI_ROOT/KernelSU/kernel" "$DRIVER_DIR/kernelsu"

echo "[+] Add kernel su driver to Makefile"

DRIVER_MAKEFILE=$DRIVER_DIR/Makefile
grep -q "kernelsu" $DRIVER_MAKEFILE || echo "obj-y += kernelsu/" >> $DRIVER_MAKEFILE

echo "[+] Done."
