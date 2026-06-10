#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage:
  tools/localize_symbols.sh <input.ko> <output.ko> [options]

Options:
  --keep <a,b,c>
  --keep-file <path>
  --no-default-keep
  --no-anon-locals
USAGE
}

if [[ $# -lt 2 ]]; then
  usage
  exit 1
fi

in_ko="$1"
out_ko="$2"
shift 2

extra_keep=""
extra_keep_file=""
no_default_keep=0
anon_locals=1
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
gen_bin="${repo_root}/gen_keep_globals"
trim_bin="${repo_root}/trim_local_symnames"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --keep)
      extra_keep="$2"
      shift 2
      ;;
    --keep-file)
      extra_keep_file="$2"
      shift 2
      ;;
    --no-default-keep)
      no_default_keep=1
      shift
      ;;
    --no-anon-locals)
      anon_locals=0
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "unknown arg: $1" >&2
      usage
      exit 1
      ;;
  esac
done

if [[ ! -f "$in_ko" ]]; then
  echo "input file not found: $in_ko" >&2
  exit 1
fi

if ! command -v llvm-objcopy >/dev/null 2>&1; then
  echo "llvm-objcopy not found" >&2
  exit 1
fi

if [[ ! -x "${gen_bin}" || ! -x "${trim_bin}" ]]; then
  echo "missing tools: ${gen_bin} or ${trim_bin}" >&2
  echo "run: make tools" >&2
  exit 1
fi

tmp_keep="$(mktemp /tmp/ksu-keep.XXXXXX)"
tmp_out=""
trap 'rm -f "$tmp_keep" "${tmp_out}"' EXIT

gen_args=("$in_ko" "--out" "$tmp_keep")
if [[ -n "$extra_keep" ]]; then
  gen_args+=("--keep" "$extra_keep")
fi
if [[ -n "$extra_keep_file" ]]; then
  gen_args+=("--keep-file" "$extra_keep_file")
fi
if [[ "$no_default_keep" -eq 1 ]]; then
  gen_args+=("--no-default-keep")
fi

"${gen_bin}" "${gen_args[@]}"

work_ko="$out_ko"
if [[ "$in_ko" == "$out_ko" ]]; then
  tmp_out="$(mktemp /tmp/ksu-localize.XXXXXX.ko)"
  cp -f "$in_ko" "$tmp_out"
  work_ko="$tmp_out"
else
  cp -f "$in_ko" "$work_ko"
fi

llvm-objcopy --keep-global-symbols="$tmp_keep" "$work_ko"
llvm-objcopy --strip-unneeded "$work_ko"

if [[ "$anon_locals" -eq 1 ]]; then
  "${trim_bin}" "$work_ko" --keep-file "$tmp_keep"
fi

if [[ "$in_ko" == "$out_ko" ]]; then
  mv -f "$work_ko" "$out_ko"
  tmp_out=""
fi

echo "final stats:"
readelf -Ws "$out_ko" | awk 'NR>3{all++; if($5=="LOCAL")l++; if($5=="GLOBAL")g++; if($5=="WEAK")w++} END{printf("all=%d local=%d global=%d weak=%d\n",all,l,g,w)}'
