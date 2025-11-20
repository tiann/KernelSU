# KernelSU Agent Guide

## Agent Quick Start

- For significant features or refactors, sketch an Plan first; keep it updated as you work.
- Use Context7 to pull library/API docs when you touch unfamiliar crates, Android APIs, or JS deps.
- Default to `rg` for searching and keep edits ASCII unless the file already uses non-ASCII.
- Run the component-specific checks below before handing work off; do not skip failing steps.
- When unsure which path to take, favor minimal risk changes that keep kernel/userspace contracts intact.

## Project Overview

KernelSU is a kernel-based root solution for Android with a kernel module, Rust userspace daemons, a Kotlin Manager app, and docs/web assets.

## Repository Structure

```bash
/kernel/                      # Kernel module - C code for Linux kernel integration
/userspace/ksud/              # Userspace daemon - Rust binary for userspace-kernel communication
/userspace/meta-overlayfs/    # Meta-overlay filesystem implementation - Rust binary + scripts
/manager/                     # Android manager app - Kotlin/Jetpack Compose UI
/website/                     # Documentation website - VitePress
/js/                          # JavaScript library for module WebUI
/.github/workflows/           # CI/CD workflows for building and testing
/scripts/                     # Build automation scripts (Python)
```

## Core Concepts

- **supercall**: Kernel-side IOCTL interface exposed by the `[ksu_driver]` anon-inode and installed via the reboot kprobe hook in `kernel/supercalls.c`. It maps commands like allowlist/app-profile management, feature toggles, and sepolicy changes. Rust userspace reaches it through `userspace/ksud/src/ksucalls.rs` (scans or installs the FD, wraps IOCTLs), while the Manager JNI bridge mirrors the same IOCTLs in `manager/app/src/main/cpp/ksu.cc`.
- **module**: A flashable ZIP unpacked by `userspace/ksud/src/module.rs` into `/data/adb/modules/` (`userspace/ksud/src/defs.rs`), with lifecycle scripts (`post-fs-data.sh`, `service.sh`, etc.) executed by ksud init events (`userspace/ksud/src/init_event.rs`). The Android Manager surfaces module state from ksud in `manager/app/src/main/java/me/weishu/kernelsu/ui/viewmodel/ModuleViewModel.kt`.
- **metamodule**: A special module marked by `metamodule=1` in `module.prop` (see `userspace/meta-overlayfs/metamodule/module.prop`). ksud enforces a single active metamodule, creates `/data/adb/metamodule -> /data/adb/modules/<id>` symlink, and delegates mounting/meta install hooks via `userspace/ksud/src/metamodule.rs`; metamodule scripts run before regular modules in `userspace/ksud/src/init_event.rs`. The Manager UI highlights metamodules and warns on uninstall (`manager/app/src/main/java/me/weishu/kernelsu/ui/screen/Module.kt`).
- **app profile**: Per-app policy struct defined in `kernel/app_profile.h` and validated/persisted in `kernel/allowlist.c` to control root grants and non-root behavior (e.g., cumulative umount policy). supercall IOCTLs `KSU_IOCTL_GET/SET_APP_PROFILE` live in `kernel/supercalls.c` and are consumed by ksud/Manager via the JNI bridge (`manager/app/src/main/cpp/ksu.cc`) and Kotlin model `Natives.Profile` (`manager/app/src/main/java/me/weishu/kernelsu/Natives.kt`).
- **sucompat**: Exec/FS compatibility layer that reroutes `/system/bin/su` to ksud for allowed UIDs, keeping legacy “call su to root” flows working. The hooks live in `kernel/sucompat.c` and are registered by the syscall hook manager; feature toggle `KSU_FEATURE_SU_COMPAT` is exposed through supercalls and surfaced to the Manager via `manager/app/src/main/cpp/ksu.cc` (`is_su_enabled` / `set_su_enabled`).
- **allowlist**: Kernel-managed list of UIDs permitted for root, persisted at `/data/adb/ksu/.allowlist` with default root/non-root profiles. Core logic is in `kernel/allowlist.c` (bitmap storage, persistence, default profile caching) and is initialized from `kernel/ksu.c`; supercall handlers in `kernel/supercalls.c` expose getters, deny-list checks, and “should umount modules” decisions. Manager reads and edits it through the JNI calls in `manager/app/src/main/cpp/ksu.cc` and Kotlin `Natives` façade.

## Component Workflows

### Kernel (`kernel/`)

- Kernel changes are C-only; keep interfaces aligned with supercall and allowlist expectations in userspace/Manager.
- If you alter IOCTLs or profiles, update the corresponding wrappers in ksud (`ksucalls.rs`) and Manager JNI (`manager/app/src/main/cpp/ksu.cc`).

### Userspace Rust (`userspace/ksud`, `userspace/meta-overlayfs`)

For Rust projects in `userspace/ksud` and `userspace/meta-overlayfs`, ALWAYS run these commands in sequence after making code changes:

1. `cargo ndk -t arm64-v8a check` (verify compilation)
2. `cargo ndk -t arm64-v8a clippy` (lints and warnings)
3. `cargo fmt` (format)
4. Fix any errors or warnings before considering the task complete.

### Android Manager App (`manager/`)

```bash
cd manager
# Must have ksud binaries first!
mkdir -p app/src/main/jniLibs/arm64-v8a
cp ../userspace/ksud/target/aarch64-linux-android/release/ksud app/src/main/jniLibs/arm64-v8a/libksud.so

# Then build
./gradlew clean assembleRelease
```

Important: Manager build REQUIRES ksud binaries to be present in `jniLibs` before building.

### Website (`website/`)

```bash
cd website
# Using bun (preferred)
bun install
bun run docs:build  # Production build
```

### JavaScript Web UI (`js/`)

- JS packages back module WebUI pieces; follow existing package manager lockfile and run the relevant lint/test scripts before publishing changes.

## Common Pitfalls

- Only one metamodule can be active; keep meta hooks in sync with ksud expectations.
- Manager JNI mirrors every supercall; kernel or ksud API changes must be reflected there to avoid runtime drift.
- Do not skip the `cargo ndk` steps; plain `cargo check` will not validate Android targets.
- Manager builds fail if `libksud.so` is missing; create it before any Gradle command.

## Git Commit

- Mirror existing history style: `<scope>: <summary>` with a short lowercase scope tied to the touched area (e.g., `kernel`, `ksud`, `manager`, `meta-overlayfs`, `docs`, `scripts`). Keep the summary concise, sentence case, and avoid trailing period.
- Prefer one scope; if multiple areas change, pick the primary one rather than chaining scopes. For doc-only changes use `docs:`; for multi-lang string updates use `translations:` if that matches log history.
- Keep subject lines brief (target ≤72 chars), no body unless necessary. If referencing a PR/issue, append `(#1234)` at the end as seen in history.
- Before committing, glance at recent `git log --oneline` to stay consistent with current prefixes and capitalization used in this repo.
