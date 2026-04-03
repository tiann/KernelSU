import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo


def workspace_root() -> Path:
    return Path(__file__).resolve().parent


def load_jsonc(path: Path) -> dict:
    raw = path.read_text(encoding="utf-8")
    try:
        return json.loads(raw)
    except json.JSONDecodeError:
        try:
            import jsonc  # type: ignore[import-not-found]
        except ImportError as exc:
            raise RuntimeError(
                "JSONC config requires optional dependency 'json-with-comments'. "
                "Install it with: pip install json-with-comments"
            ) from exc
        return jsonc.loads(raw)


def merge_config(file_cfg: dict, args: argparse.Namespace) -> dict:
    cfg = {
        "signing": {
            "keystore_path": "",
            "key_alias": "",
            "keystore_pass": "",
            "key_pass": "",
        },
        "app_build_type": "debug",
        "ksud_build_type": "debug",
        "arch": [],
        "output_name": "",
        "strip": False,
    }

    cfg.update({k: v for k, v in file_cfg.items() if k != "signing"})
    file_signing = file_cfg.get("signing", {})
    if isinstance(file_signing, dict):
        cfg["signing"].update(file_signing)

    if args.app_build_type:
        cfg["app_build_type"] = args.app_build_type
    if args.ksud_build_type:
        cfg["ksud_build_type"] = args.ksud_build_type
    if args.arch:
        cfg["arch"] = normalize_arch_values(args.arch)
    if args.output_name:
        cfg["output_name"] = args.output_name
    if args.strip is not None:
        cfg["strip"] = args.strip

    if args.keystore_path:
        cfg["signing"]["keystore_path"] = args.keystore_path
    if args.key_alias:
        cfg["signing"]["key_alias"] = args.key_alias
    if args.keystore_pass:
        cfg["signing"]["keystore_pass"] = args.keystore_pass
    if args.key_pass:
        cfg["signing"]["key_pass"] = args.key_pass

    cfg["arch"] = normalize_arch_values(cfg.get("arch", []))
    return cfg


def normalize_arch_values(values: Iterable[str]) -> List[str]:
    out: List[str] = []
    seen = set()
    for item in values:
        for part in str(item).split(","):
            arch = part.strip()
            if arch and arch not in seen:
                seen.add(arch)
                out.append(arch)
    return out


def version_key(version: str) -> Tuple[int, ...]:
    nums = re.findall(r"\d+", version)
    return tuple(int(n) for n in nums) if nums else (0,)


def find_strip_tool() -> Optional[Path]:
    """Locate llvm-strip (preferred) or strip from the Android NDK toolchain."""
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
            for prebuilt in toolchain_bin.iterdir():
                bin_dir = prebuilt / "bin"
                for name in ("llvm-strip", "llvm-strip.exe", "strip", "strip.exe"):
                    candidate = bin_dir / name
                    if candidate.exists():
                        return candidate

    # Fall back to PATH.
    for name in ("llvm-strip", "strip"):
        found = shutil.which(name)
        if found:
            return Path(found)
    return None


def find_android_tool(tool_base_name: str) -> Optional[Path]:
    direct = shutil.which(tool_base_name)
    if direct:
        return Path(direct)

    executable_names = [tool_base_name]
    if os.name == "nt":
        executable_names = [f"{tool_base_name}.exe", f"{tool_base_name}.bat", tool_base_name]

    sdk_root = os.environ.get("ANDROID_SDK_ROOT") or os.environ.get("ANDROID_HOME")
    if not sdk_root:
        return None

    build_tools = Path(sdk_root) / "build-tools"
    if not build_tools.exists():
        return None

    candidates: List[Tuple[Tuple[int, ...], Path]] = []
    for version_dir in build_tools.iterdir():
        if not version_dir.is_dir():
            continue
        for name in executable_names:
            candidate = version_dir / name
            if candidate.exists():
                candidates.append((version_key(version_dir.name), candidate))

    if not candidates:
        return None
    candidates.sort(key=lambda x: x[0], reverse=True)
    return candidates[0][1]


def run_cmd(args: List[str], fail_msg: str) -> None:
    proc = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if proc.returncode != 0:
        output = proc.stdout.strip()
        raise RuntimeError(f"{fail_msg}\nCommand: {' '.join(args)}\n{output}")


def find_latest_apk(app_build_type: str) -> Path:
    pattern = workspace_root() / "manager" / "app" / "build" / "outputs" / "apk" / app_build_type
    apks = sorted(pattern.glob("*.apk"), key=lambda p: p.stat().st_mtime)
    if not apks:
        raise FileNotFoundError(f"No APK found under: {pattern}")
    if len(apks) > 1:
        print(f"[WARN] Multiple APKs found, using latest: {apks[-1]}", file=sys.stderr)
    return apks[-1]


ARCH_TO_TRIPLE = {
    "arm64-v8a": "aarch64-linux-android",
    "armeabi-v7a": "armv7-linux-androideabi",
    "x86": "i686-linux-android",
    "x86_64": "x86_64-linux-android",
}


def find_ksud_binaries_by_arch(ksud_build_type: str, arch_filters: List[str]) -> Dict[str, Path]:
    result: Dict[str, Path] = {}
    target_root = workspace_root() / "target"
    for arch in arch_filters:
        triple = ARCH_TO_TRIPLE.get(arch)
        if not triple:
            print(f"[WARN] Unknown arch '{arch}', cannot map to target triple.", file=sys.stderr)
            continue
        candidate = target_root / triple / ksud_build_type / "ksud"
        if candidate.exists():
            result[arch] = candidate
        else:
            print(
                f"[WARN] ksud not found for {arch}: {candidate}",
                file=sys.stderr,
            )
    return result


def collect_existing_arches(apk_path: Path) -> List[str]:
    arches = []
    seen = set()
    with ZipFile(apk_path, "r") as zin:
        for name in zin.namelist():
            if not name.startswith("lib/"):
                continue
            parts = name.split("/")
            if len(parts) >= 3 and parts[1] and parts[1] not in seen:
                seen.add(parts[1])
                arches.append(parts[1])
    return arches


def collect_existing_ksud_arches(apk_path: Path) -> List[str]:
    arches = []
    seen = set()
    with ZipFile(apk_path, "r") as zin:
        for name in zin.namelist():
            if not (name.startswith("lib/") and name.endswith("/libksud.so")):
                continue
            parts = name.split("/")
            if len(parts) >= 3 and parts[1] and parts[1] not in seen:
                seen.add(parts[1])
                arches.append(parts[1])
    return arches


def strip_binary(src: Path, strip_tool: Path, tmp_dir: Path) -> bytes:
    dst = tmp_dir / src.name
    shutil.copy2(src, dst)
    run_cmd([str(strip_tool), "--strip-all", str(dst)], "strip failed")
    return dst.read_bytes()


def repack_apk(
    apk_path: Path,
    out_unsigned_path: Path,
    arch_filters: List[str],
    ksud_by_arch: Dict[str, Path],
    strip_tool: Optional[Path] = None,
) -> None:
    with tempfile.TemporaryDirectory() as tmp_dir:
        ksud_bytes_by_arch: Dict[str, bytes] = {}
        for arch, ksud_path in ksud_by_arch.items():
            if strip_tool is not None:
                ksud_bytes_by_arch[arch] = strip_binary(ksud_path, strip_tool, Path(tmp_dir))
            else:
                ksud_bytes_by_arch[arch] = ksud_path.read_bytes()

        with ZipFile(apk_path, "r") as zin, ZipFile(out_unsigned_path, "w") as zout:
            for info in zin.infolist():
                name = info.filename

                if name.startswith("lib/") and arch_filters:
                    parts = name.split("/")
                    if len(parts) >= 3 and parts[1] not in arch_filters:
                        continue

                # Drop original libksud.so only for arches that have a replacement binary.
                if name.startswith("lib/") and name.endswith("/libksud.so"):
                    parts = name.split("/")
                    if len(parts) >= 3 and parts[1] in ksud_bytes_by_arch:
                        continue

                data = zin.read(name)
                new_info = ZipInfo(filename=name, date_time=info.date_time)
                new_info.compress_type = info.compress_type
                new_info.external_attr = info.external_attr
                new_info.comment = info.comment
                new_info.create_system = info.create_system
                new_info.extra = info.extra
                if info.compress_type == ZIP_DEFLATED:
                    zout.writestr(new_info, data, compress_type=ZIP_DEFLATED)
                else:
                    zout.writestr(new_info, data)

            for arch in arch_filters:
                ksud_bytes = ksud_bytes_by_arch.get(arch)
                if ksud_bytes is None:
                    continue
                lib_path = f"lib/{arch}/libksud.so"
                entry = ZipInfo(filename=lib_path)
                entry.compress_type = ZIP_DEFLATED
                zout.writestr(entry, ksud_bytes)


def assert_required_libs(apk_path: Path, arch_filters: List[str]) -> None:
    with ZipFile(apk_path, "r") as zf:
        names = set(zf.namelist())
    missing = [arch for arch in arch_filters if f"lib/{arch}/libksud.so" not in names]
    if missing:
        raise RuntimeError(
            "Missing libksud.so in APK for architecture(s): " + ", ".join(missing)
        )


def validate_signing_config(signing: Dict[str, str]) -> None:
    required = ["keystore_path", "key_alias", "keystore_pass", "key_pass"]
    missing = [k for k in required if not str(signing.get(k, "")).strip()]
    if missing:
        raise ValueError("Signing config is incomplete, missing: " + ", ".join(missing))
    if not Path(signing["keystore_path"]).exists():
        raise FileNotFoundError(f"Keystore not found: {signing['keystore_path']}")


def do_repack(args: argparse.Namespace) -> int:
    ws_root = workspace_root()
    config_path = Path(args.config).resolve() if args.config else ws_root / "repack-config.json"
    if config_path.exists():
        file_cfg = load_jsonc(config_path)
    else:
        if args.config:
            raise FileNotFoundError(f"Config not found: {config_path}")
        print(f"[WARN] Config not found, using defaults and CLI overrides: {config_path}", file=sys.stderr)
        file_cfg = {}
    cfg = merge_config(file_cfg, args)

    apk = find_latest_apk(cfg["app_build_type"])
    arch_filters = cfg.get("arch", [])
    if not arch_filters:
        inferred = collect_existing_arches(apk)
        if inferred:
            arch_filters = inferred
        else:
            arch_filters = ["arm64-v8a"]
        print(f"[INFO] No arch configured, using: {', '.join(arch_filters)}")

    ksud_by_arch = find_ksud_binaries_by_arch(cfg["ksud_build_type"], arch_filters)
    missing_ksud_arches = [arch for arch in arch_filters if arch not in ksud_by_arch]
    if missing_ksud_arches:
        existing_ksud_arches = set(collect_existing_ksud_arches(apk))
        missing_in_apk = [arch for arch in missing_ksud_arches if arch not in existing_ksud_arches]
        if missing_in_apk:
            raise RuntimeError(
                "ksud binary not found and APK has no existing libksud.so for architecture(s): "
                + ", ".join(missing_in_apk)
            )
        print(
            "[WARN] ksud binary not found for architecture(s): "
            + ", ".join(missing_ksud_arches)
            + ". Using existing libksud.so from input APK.",
            file=sys.stderr,
        )

    out_dir = Path(args.out_dir).resolve() if args.out_dir else ws_root / "dist"
    out_dir.mkdir(parents=True, exist_ok=True)

    output_name = cfg.get("output_name") or apk.stem
    unsigned_path = out_dir / f"{output_name}-repack-unsigned.apk"
    aligned_path = out_dir / f"{output_name}-repack-aligned.apk"
    signed_path = out_dir / f"{output_name}.apk"

    # Clean stale outputs before repacking.
    for stale in (unsigned_path, aligned_path, signed_path):
        if stale.exists():
            stale.unlink()

    # Resolve strip tool.
    do_strip: bool = bool(cfg.get("strip", False))
    strip_tool: Optional[Path] = None
    if do_strip:
        strip_tool = find_strip_tool()
        if strip_tool is None:
            print("[WARN] strip requested but no strip tool found; skipping strip.", file=sys.stderr)
        else:
            print(f"[INFO] Strip tool: {strip_tool}")

    try:
        repack_apk(apk, unsigned_path, arch_filters, ksud_by_arch, strip_tool)
        assert_required_libs(unsigned_path, arch_filters)

        zipalign = find_android_tool("zipalign")
        if zipalign is None:
            raise FileNotFoundError("zipalign not found in PATH or Android SDK build-tools")
        run_cmd(
            [str(zipalign), "-P", "16", "-f", "4", str(unsigned_path), str(aligned_path)],
            "zipalign failed",
        )

        signing = cfg.get("signing", {})
        validate_signing_config(signing)

        apksigner = find_android_tool("apksigner")
        if apksigner is None:
            raise FileNotFoundError("apksigner not found in PATH or Android SDK build-tools")

        run_cmd(
            [
                str(apksigner),
                "sign",
                "--v1-signing-enabled",
                "false",
                "--v2-signing-enabled",
                "true",
                "--v3-signing-enabled",
                "false",
                "--v4-signing-enabled",
                "false",
                "--ks",
                str(Path(signing["keystore_path"]).resolve()),
                "--ks-key-alias",
                signing["key_alias"],
                "--ks-pass",
                f"pass:{signing['keystore_pass']}",
                "--key-pass",
                f"pass:{signing['key_pass']}",
                "--out",
                str(signed_path),
                str(aligned_path),
            ],
            "apksigner failed",
        )
    finally:
        # Remove intermediate files regardless of success/failure.
        for tmp in (unsigned_path, aligned_path):
            if tmp.exists():
                tmp.unlink()

    print(f"Input APK : {apk}")
    if ksud_by_arch:
        ksud_desc = ", ".join(f"{arch}={path}" for arch, path in ksud_by_arch.items())
    else:
        ksud_desc = "NOT FOUND"
    print(f"ksud      : {ksud_desc}")
    print(f"Strip     : {'yes (' + str(strip_tool) + ')' if strip_tool else ('requested but unavailable' if do_strip else 'no')}")
    print(f"Arch      : {', '.join(arch_filters)}")
    print(f"Output    : {signed_path}")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Repack manager APK with ksud injection, zipalign(16KB), and resign."
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    repack = subparsers.add_parser("repack", help="Repack and resign APK")
    repack.add_argument("-c", "--config", help="Path to jsonc config file")
    repack.add_argument("-b", "--app-build-type", help="APK build type override, e.g. debug/release")
    repack.add_argument("-t", "--ksud-build-type", help="ksud build type override, e.g. debug/release")
    repack.add_argument(
        "-a",
        "--arch",
        action="append",
        help="Target architecture(s), repeat or use comma list, e.g. -a arm64-v8a -a armeabi-v7a",
    )
    repack.add_argument("-K", "--keystore-path", help="Keystore path override")
    repack.add_argument("-A", "--key-alias", help="Key alias override")
    repack.add_argument("-P", "--keystore-pass", help="Keystore password override")
    repack.add_argument("-S", "--key-pass", help="Private key password override")
    repack.add_argument("-n", "--output-name", help="Base name for output APK files (default: input APK stem)")
    strip_group = repack.add_mutually_exclusive_group()
    strip_group.add_argument(
        "--strip",
        dest="strip",
        action="store_true",
        default=None,
        help="Strip libksud.so before packing (uses NDK llvm-strip)",
    )
    strip_group.add_argument(
        "--no-strip",
        dest="strip",
        action="store_false",
        default=None,
        help="Disable strip even if config enables it",
    )
    repack.add_argument("-o", "--out-dir", help="Output directory override (default: dist)")
    repack.set_defaults(func=do_repack)

    return parser


def main() -> int:
    os.chdir(workspace_root())
    parser = build_parser()
    args = parser.parse_args()
    try:
        return args.func(args)
    except Exception as exc:  # noqa: BLE001
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
