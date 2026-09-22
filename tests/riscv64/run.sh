#!/usr/bin/env sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
test_out=$(mktemp -d)
trap 'rm -f "$test_out/riscv64" "$test_out/arm64" "$test_out/x86_64"; rmdir "$test_out"' EXIT HUP INT TERM
for arch in riscv64 arm64 x86_64; do
    case "$arch" in
        riscv64) define=KSU_TEST_RISCV64 ;;
        arm64) define=KSU_TEST_ARM64 ;;
        x86_64) define=KSU_TEST_X86_64 ;;
    esac
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Wno-unused-function \
        -D"$define" -I"$root/tests/riscv64/include" -I"$root/kernel/include" \
        -I"$root/kernel" "$root/tests/riscv64/arch_test.c" -o "$test_out/$arch"
    "$test_out/$arch"
done
