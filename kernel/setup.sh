#! /bin/bash

set +eux

git clone https://github.com/tiann/KernelSU

GKI_ROOT=$(pwd)

echo "[+] GKI_ROOT: $GKI_ROOT"
echo "[+] Copy kernel su driver to $GKI_ROOT/common/drivers"

ln -sf $(pwd)/KernelSU/kernel $GKI_ROOT/common/drivers/kernelsu

echo "[+] Add kernel su driver to Makefile"

DRIVER_MAKEFILE=$GKI_ROOT/common/drivers/Makefile
grep -q "kernelsu" $DRIVER_MAKEFILE || echo "obj-$(CONFIG_KPROBES) += kernelsu/" >> $DRIVER_MAKEFILE

echo "[+] Done."
