# KernelSU Development Guide for Copilot

## Project Overview

KernelSU is a kernel-based root solution for Android devices that provides:
- Kernel-based `su` and root access management
- Module system based on metamodules for systemless modifications
- App Profile system to control root permissions

**Repository Size**: ~14MB (376K kernel, 4.9M userspace, 2.4M manager, 1.4M website)
**Languages**: C (kernel module), Rust (userspace daemon), Kotlin/Java (Android manager), TypeScript (website)
**Target Platforms**: Android GKI 2.0+ devices (kernel 5.10+), arm64-v8a and x86_64 architectures

## Repository Structure

```
/kernel/                 # Kernel module (GPL-2.0-only) - C code for Linux kernel integration
/userspace/ksud/        # Userspace daemon (GPL-3.0+) - Rust binary for root management
/userspace/meta-overlayfs/ # Meta-overlay filesystem implementation
/manager/               # Android manager app (GPL-3.0+) - Kotlin/Jetpack Compose UI
/website/               # Documentation website - VitePress
/js/                    # JavaScript library for module WebUI
/.github/workflows/     # 21 CI/CD workflows for building and testing
/scripts/               # Build automation scripts (Python)
```

## Build System & Dependencies

### Kernel Module (kernel/)
- **Build Tool**: Linux kernel make system
- **Configuration**: `CONFIG_KSU=m` in kernel config
- **Key Files**: `Makefile`, `Kconfig`, `setup.sh`
- **Build Command**: `CONFIG_KSU=m make` (in kernel directory, requires kernel source tree)
- **Integration Script**: `kernel/setup.sh` - sets up KernelSU in kernel source tree
- **CI Builds**: Uses DDK (Driver Development Kit) containers with specific KMI versions
  - Supported KMI: android12-5.10, android13-5.10, android13-5.15, android14-5.15, android14-6.1, android15-6.6, android16-6.12
  - DDK Release: 20251104

### Userspace Daemon (userspace/ksud/)
- **Language**: Rust 2024 edition
- **Build Tool**: `cross` (cross-compilation tool) - version from git rev 66845c1
- **Rust Toolchain**: Requires stable Rust (tested with 1.91.1+)
- **Target Platforms**: 
  - Primary: `aarch64-linux-android`, `x86_64-linux-android`
  - Extra: `x86_64-pc-windows-gnu`, `x86_64-apple-darwin`, `aarch64-apple-darwin`, `aarch64-unknown-linux-musl`, `x86_64-unknown-linux-musl`
- **Build Command**: 
  ```bash
  # Install cross if not present
  RUSTFLAGS="" cargo install cross --git https://github.com/cross-rs/cross --rev 66845c1
  
  # Build for Android
  CROSS_NO_WARNINGS=0 cross build --target aarch64-linux-android --release --manifest-path ./userspace/ksud/Cargo.toml
  ```
- **Build Time**: ~25 seconds for check, longer for cross-compilation
- **Dependencies**: See `userspace/ksud/Cargo.toml` - includes zip, clap, rustix, android-properties
- **Embedded Assets**: Contains LKM files in `bin/aarch64/` directory (from build-lkm workflow)

### Android Manager App (manager/)
- **Build Tool**: Gradle 8.13.1+ with Kotlin 2.2.21
- **Java Version**: Java 21 (Temurin distribution in CI)
- **SDK Requirements**: 
  - minSdk: 26
  - targetSdk: 36
  - compileSdk: 36
  - NDK: 29.0.13599879-beta2
  - Build Tools: 36.1.0
  - CMake: 3.28.0+
- **Build Commands**:
  ```bash
  cd manager
  # Must have ksud binaries first!
  mkdir -p app/src/main/jniLibs/arm64-v8a
  mkdir -p app/src/main/jniLibs/x86_64
  cp ../userspace/ksud/target/aarch64-linux-android/release/ksud app/src/main/jniLibs/arm64-v8a/libksud.so
  cp ../userspace/ksud/target/x86_64-linux-android/release/ksud app/src/main/jniLibs/x86_64/libksud.so
  
  # Then build
  ./gradlew clean assembleRelease
  ```
- **Important**: Manager build REQUIRES ksud binaries to be present in jniLibs before building
- **Native Code**: C++ components in `app/src/main/cpp/` built with CMake
- **Output**: APK named `KernelSU_{version}_{versionCode}-{variant}.apk`

### Website (website/)
- **Framework**: VitePress 1.6.4+ with Vue 3.5.22+
- **Build Tool**: npm/yarn
- **Commands**:
  ```bash
  cd website
  npm install
  npm run docs:dev    # Development server
  npm run docs:build  # Production build
  ```

### Just Build Tool (justfile)
Optional convenience tool for quick builds:
```bash
just build_ksud     # Build ksud for aarch64-android
just build_manager  # Build both ksud and manager
just clippy         # Run Rust linting
```

## CI/CD Workflows & Validation

### Rust Code Quality (runs on main/dev branches and PRs)
1. **rustfmt.yml**: Format checking
   - Uses nightly Rust toolchain
   - Command: `cargo fmt --check` in `userspace/ksud/`
   - **Always run `cargo fmt` before committing Rust code**

2. **clippy.yml**: Linting with warnings as errors
   - Environment: `RUSTFLAGS='-Dwarnings'`
   - Targets: aarch64-linux-android, x86_64-linux-android
   - Command: `cross clippy --manifest-path userspace/ksud/Cargo.toml --target {target} --release`
   - **All clippy warnings must be fixed**

### Shell Script Validation
3. **shellcheck.yml**: Shell script linting
   - Uses action-shellcheck@2.0.0
   - Ignores: `gradlew`, `userspace/ksud/src/installer.sh`
   - **Run shellcheck on any modified .sh files**

### Build Workflows
4. **build-lkm.yml** → **ddk-lkm.yml**: Builds kernel modules
   - Creates `.ko` files for all supported Android versions
   - Uses Docker containers with pre-configured kernels
   - Output: `{kmi}_kernelsu.ko` artifacts

5. **ksud.yml**: Builds userspace daemon
   - Matrix build for 7 different targets
   - Uses cross-compilation
   - Requires rust-cache for efficiency
   - Output: `ksud-{target}` artifacts

6. **build-manager.yml**: Complete manager build pipeline
   - Depends on: build-lkm → build-ksud → build-manager
   - Downloads ksud artifacts from previous jobs
   - Copies binaries to jniLibs directories
   - Runs `./gradlew clean assembleRelease`
   - **Build time**: Several minutes for full pipeline

### Additional Workflows
- **build-kernel-*.yml**: Build complete kernel images for specific Android versions
- **gki-kernel.yml**, **wsa-kernel.yml**: Kernel builds for specific platforms
- **meta-overlay.yml**: Build meta-overlayfs component
- **release.yml**: Release automation
- **deploy-website.yml**: Website deployment

## Common Build Issues & Workarounds

### Cross Tool Installation
- **Issue**: cross tool not in PATH
- **Fix**: Install with exact revision: `RUSTFLAGS="" cargo install cross --git https://github.com/cross-rs/cross --rev 66845c1`
- **Note**: Must clear RUSTFLAGS during cross installation to avoid conflicts

### Manager Build Without ksud
- **Issue**: Manager build fails with missing jniLibs
- **Fix**: **ALWAYS build ksud first and copy to jniLibs** before building manager
- **Order**: build-lkm → build-ksud → copy to jniLibs → build-manager

### Rust Format Issues
- **Issue**: rustfmt check fails in CI
- **Fix**: Run `cargo fmt --manifest-path userspace/ksud/Cargo.toml` before committing
- **Note**: CI uses nightly toolchain for rustfmt

### Clippy Warnings
- **Issue**: Build passes locally but fails in CI
- **Fix**: Run with `-Dwarnings` flag: `RUSTFLAGS='-Dwarnings' cargo clippy`
- **Note**: CI treats all warnings as errors

### Android SDK/NDK Versions
- **Issue**: Gradle build fails with SDK version mismatch
- **Fix**: Ensure NDK 29.0.13599879-beta2, compileSdk 36, build tools 36.1.0
- **Note**: Versions are specified in `manager/build.gradle.kts`

## Development Workflow

### For Kernel Module Changes
1. Modify files in `/kernel/`
2. Test with local kernel build or wait for CI
3. Ensure C code follows kernel coding standards
4. Run `shellcheck` on any modified .sh files

### For Userspace Daemon Changes
1. Modify files in `/userspace/ksud/src/`
2. **Always run before committing**:
   ```bash
   cd userspace/ksud
   cargo fmt
   cargo clippy --target aarch64-linux-android --release
   cargo test
   ```
3. Test cross-compilation: `cross build --target aarch64-linux-android --release`
4. Verify no warnings with: `RUSTFLAGS='-Dwarnings' cargo clippy`

### For Manager Changes
1. Modify files in `/manager/app/src/`
2. Ensure you have ksud binaries in jniLibs
3. Build: `cd manager && ./gradlew clean assembleDebug`
4. For release: `./gradlew clean assembleRelease`
5. Check lint: `./gradlew lint`

### For Documentation Changes
1. Modify files in `/website/docs/` or `/docs/`
2. Test locally: `cd website && npm run docs:dev`
3. No linting required for documentation-only changes

## Key Configuration Files

- `/kernel/Makefile` - Kernel module build configuration
- `/kernel/Kconfig` - Kernel configuration options
- `/userspace/ksud/Cargo.toml` - Rust dependencies and build settings
- `/manager/build.gradle.kts` - Root Gradle configuration, version code/name
- `/manager/app/build.gradle.kts` - Manager app build configuration
- `/manager/gradle/libs.versions.toml` - Dependency versions (AGP 8.13.1, Kotlin 2.2.21)
- `/justfile` - Just command runner shortcuts
- `/.github/workflows/*.yml` - CI/CD pipeline definitions

## Version Management

- **Kernel Module Version**: Calculated from git commit count: `20000 + $(git rev-list --count HEAD)`
- **Manager Version Code**: `2 * 10000 + $(git rev-list --count HEAD)`
- **Manager Version Name**: `$(git describe --tags --always)`
- **Defined in**: `manager/build.gradle.kts` functions

## Security & Signing

- Manager APK signing configured via `gradle.properties` (see `sign.example.properties`)
- Kernel module signature verification via `KSU_EXPECTED_SIZE` and `KSU_EXPECTED_HASH`
- Default values in `kernel/Makefile` can be overridden

## Testing Strategy

- **Kernel Module**: Typically tested on device or emulator
- **ksud**: Run `cargo test` in userspace/ksud directory
- **Manager**: Gradle test tasks, primarily manual testing on device
- **CI**: Builds for all targets but no automated runtime tests

## Important Notes

1. **Trust these instructions**: Only search/explore if information is incomplete or incorrect
2. **Build order matters**: LKM → ksud → manager (each step depends on previous)
3. **CI runs on**: pushes to main/dev/ci branches, PRs to main/dev
4. **Licensing**: kernel/ is GPL-2.0-only, everything else is GPL-3.0-or-later
5. **Cross-compilation required**: ksud cannot be built natively, must use `cross` tool
6. **Java 21 required**: Manager requires Java 21 (Temurin distribution in CI)
7. **Format before commit**: Always run `cargo fmt` on Rust code before committing
8. **No warnings allowed**: Clippy runs with `-Dwarnings`, all warnings must be fixed
