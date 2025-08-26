# KernelSU Copilot Instructions

## Repository Overview

KernelSU is a kernel-based root solution for Android devices that provides kernel-level `su` and root access management through a module system based on OverlayFS. It serves as an alternative to Magisk, supporting Android GKI 2.0 devices (kernel 5.10+) with manual build support for older kernels (4.14+).

**Repository Size:** ~50MB with comprehensive multi-language documentation  
**Primary Languages:** C (kernel), Rust (userspace daemon), Kotlin (Android app)  
**Target Architectures:** arm64-v8a (major), x86_64  
**Supported Platforms:** Android, WSA, ChromeOS, container-based Android

## High-Level Architecture

The project consists of three main components:
1. **Kernel Module** (`kernel/`): C code that hooks into the Linux kernel to provide root access
2. **Userspace CLI Tools** (`userspace/ksud/`): Rust cli tool managing modules, policies, and system integration  
3. **Manager App** (`manager/`): Android application providing user interface for root management

## Build System & Commands

### Prerequisites
Always install these tools before building:
```bash
# Install just build tool (required)
cargo install just

# Install cross-compilation tool (required for Rust builds)  
cargo install cross --git https://github.com/cross-rs/cross --rev 66845c1
```
*Installation time: ~2 minutes for just, ~1 minute for cross*

### Core Build Commands

**Build Rust userspace daemon:**
```bash
just build_ksud
# Alternative: cross build --target aarch64-linux-android --release --manifest-path ./userspace/ksud/Cargo.toml
```
*Time: 2-5 minutes on first build, ~30 seconds incremental*

**Build complete manager app (includes ksud):**
```bash
just build_manager  
```
*Time: 5-10 minutes total (includes ksud build + Android gradle build)*

**Rust code quality checks:**
```bash
just clippy
# Runs: cargo fmt + cross clippy for multiple targets (x86_64-pc-windows-gnu, aarch64-linux-android)
```
*Time: 1-3 minutes (requires cross tool to be installed first)*

### Android Build Commands

**Manager app only (requires pre-built ksud):**
```bash
cd manager
./gradlew aDebug              # Debug build
./gradlew assembleRelease     # Release build  
```
*Time: 1-3 minutes*

**Required Android environment:**
- Java 17+ (specified in manager build config)
- Android SDK + NDK version 28.0.13004108
- Gradle 9.0.0 (handled by wrapper)

### Validation & Testing

**Rust validation:**
```bash
cd userspace/ksud
cargo check                   # Fast syntax/type check (~30 seconds)
cargo test                    # No unit tests exist currently
cargo fmt --check             # Format validation
```

**Android validation:**
```bash
cd manager  
./gradlew lint                # Android linting
```

**IMPORTANT:** No traditional unit tests exist. Validation is primarily through CI integration testing and manual testing.

## Known Build Issues & Workarounds

1. **Memory Requirements:** Kernel builds may fail with <24GB RAM. Use `LTO=thin` flag for kernel builds.

2. **Cross-compilation Dependencies:** Must install `cross` tool before any Rust builds targeting Android. Without it, `just clippy` will fail with "cross: not found".

3. **Android NDK Version:** Exactly version 28.0.13004108 required - other versions may cause build failures.

4. **Git Dependencies:** Build system uses git for version numbering. Ensure `.git` directory exists and is not shallow.

5. **Rust Warnings:** Current codebase has unused import/variable warnings but builds successfully. These are non-blocking.

6. **Incremental Builds:** Clean builds recommended when switching between debug/release or when dependencies change.

## Project Layout & Key Files

### Root Directory Structure
```
├── .github/           # GitHub workflows and configurations
├── docs/             # Multi-language README files  
├── js/               # JavaScript bindings
├── justfile          # Main build configuration
├── kernel/           # C kernel module code
├── manager/          # Android application
├── scripts/          # Python utility scripts
├── userspace/        # Rust userspace components
└── website/          # Documentation website source
```

### Critical Configuration Files

**Build Configuration:**
- `justfile`: Primary build commands and aliases
- `userspace/ksud/Cargo.toml`: Rust dependencies and build settings
- `manager/build.gradle.kts`: Android build configuration
- `manager/gradle/libs.versions.toml`: Android dependency versions

**Kernel Integration:**
- `kernel/Makefile`: Kernel module build configuration  
- `kernel/Kconfig`: Kernel configuration options
- `kernel/setup.sh`: Script for integrating KernelSU into kernel source

**CI/CD Workflows:**
- `.github/workflows/build-manager.yml`: Manager app builds
- `.github/workflows/ksud.yml`: Rust daemon builds  
- `.github/workflows/clippy.yml`: Rust linting
- `.github/workflows/rustfmt.yml`: Rust formatting validation

### Source Code Organization

**Kernel Module (`kernel/`):**
- `ksu.c`: Main KernelSU kernel module
- `core_hook.c`: Core kernel hooks for root access
- `allowlist.c`: App permission management
- `selinux/`: SELinux policy management

**Rust Daemon (`userspace/ksud/src/`):**
- `main.rs`: CLI entry point
- `boot_patch.rs`: Boot image patching logic
- `module.rs`: Module management system
- `sepolicy.rs`: SELinux policy parsing

**Android Manager (`manager/app/src/main/`):**
- Kotlin/Compose UI application
- JNI integration with ksud native library
- Material 3 design with custom theming

## GitHub Actions CI/CD

### Pre-check Validation Pipeline
Before code submission, these workflows run automatically:

1. **rustfmt**: Code formatting validation (fails on formatting issues)
2. **clippy**: Rust linting with `-Dwarnings` (fails on warnings)  
3. **shellcheck**: Shell script validation
4. **Build validation**: Multi-target builds for all supported architectures

### Release Pipeline
On main branch and tags:
1. Build LKM (Loadable Kernel Module)
2. Build ksud for all target architectures
3. Build Android manager app with signing
4. Upload artifacts and deploy to Telegram

**IMPORTANT:** Always run local validation before pushing:
```bash
cargo fmt --manifest-path ./userspace/ksud/Cargo.toml --check
just clippy
```

## Version Management

Version numbers are automatically generated from git:
- **Kernel:** `10000 + git_rev_count + 200`
- **Rust:** Same formula via build.rs
- **Android:** `major * 10000 + git_rev_count + 200`

## Dependencies & External Libraries

**Rust Dependencies (key ones):**
- `clap`: CLI argument parsing
- `anyhow`: Error handling  
- `zip`: Archive handling for modules
- `rustix`: Linux system calls (custom fork)
- `android-properties`: Android property access

**Android Dependencies:**
- Jetpack Compose for UI
- Material 3 design components
- libsu for privileged operations
- Navigation component for app flow

## Module System

KernelSU uses an OverlayFS-based module system similar to Magisk:
- Modules stored in `/data/adb/modules/`
- Each module has `module.prop` configuration
- Boot scripts: `post-fs-data.sh`, `service.sh`, `boot-completed.sh`
- SELinux rules: `sepolicy.rule`
- System properties: `system.prop`

## Development Workflow Recommendations

1. **Always validate before committing:**
   ```bash
   cargo fmt --manifest-path ./userspace/ksud/Cargo.toml --check
   just clippy
   cd manager && ./gradlew lint
   ```

2. **For kernel changes:** Test with kernel setup script:
   ```bash
   curl -LSs "https://raw.githubusercontent.com/tiann/KernelSU/main/kernel/setup.sh" | bash -
   ```

3. **For module development:** Refer to `website/docs/guide/module.md` for module structure and APIs.

4. **Environment setup:** Clean builds are recommended when switching between debug/release or when dependencies change.

**TRUST THESE INSTRUCTIONS:** Only search for additional information if these instructions are incomplete or found to be incorrect. This comprehensive guide covers all common development scenarios for KernelSU.
