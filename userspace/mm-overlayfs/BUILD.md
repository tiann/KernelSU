# Build Guide

This document explains how to build the KernelSU Official Mount Handler from source.

## Prerequisites

### 1. Rust Toolchain

Install Rust using rustup:

```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

After installation, reload your shell or run:

```bash
source $HOME/.cargo/env
```

### 2. Android NDK

You need the Android NDK to cross-compile for Android targets.

**Option A: Download from Google**

1. Download from: https://developer.android.com/ndk/downloads
2. Extract to a directory (e.g., `~/android-ndk`)
3. Set environment variable:
   ```bash
   export ANDROID_NDK_HOME=~/android-ndk/android-ndk-r26b
   ```

**Option B: Using Android SDK Manager**

```bash
sdkmanager --install "ndk;26.1.10909125"
export ANDROID_NDK_HOME=$ANDROID_SDK_ROOT/ndk/26.1.10909125
```

### 3. Rust Android Targets

Add the required compilation targets:

```bash
rustup target add aarch64-linux-android
rustup target add x86_64-linux-android
```

### 4. Build Tool Selection

You need to choose one of the following cross-compilation tools:

**Option A: cross (Recommended for simplicity)**

```bash
cargo install cross
```

Cross provides a seamless cross-compilation experience with Docker. No NDK configuration required.

**Option B: cargo-ndk**

```bash
cargo install cargo-ndk
```

Requires manual NDK configuration (see step 5 below).

**Note:** The build script (`build.sh`) automatically detects which tool you have installed and uses it accordingly.

### 5. Configure Cargo for NDK (Only for cargo-ndk)

If using `cargo-ndk`, create or edit `~/.cargo/config.toml`:

```toml
[target.aarch64-linux-android]
linker = "<NDK_PATH>/toolchains/llvm/prebuilt/<HOST>/bin/aarch64-linux-android21-clang"

[target.x86_64-linux-android]
linker = "<NDK_PATH>/toolchains/llvm/prebuilt/<HOST>/bin/x86_64-linux-android21-clang"
```

Replace:
- `<NDK_PATH>` with your `$ANDROID_NDK_HOME` path
- `<HOST>` with your platform: `linux-x86_64`, `darwin-x86_64`, or `windows-x86_64`

**Or use a local config:**

Create `.cargo/config.toml` in this project directory with the same content.

**Note:** If using `cross`, you can skip this step entirely.

## Build Steps

### Quick Build (Automated)

1. Clone the KernelSU repository:
   ```bash
   git clone https://github.com/tiann/KernelSU
   cd KernelSU/userspace/ksu-metamodule
   ```

2. Run the build script:
   ```bash
   ./build.sh
   ```

3. Find the output:
   ```bash
   ls -lh out/ksu-official-mount-v*.zip
   ```

### Manual Build

If you want more control or the automated script doesn't work:

#### Using cross

```bash
# Build for aarch64
cross build --release --target aarch64-linux-android

# Build for x86_64
cross build --release --target x86_64-linux-android
```

#### Using cargo ndk

```bash
# Build for aarch64
cargo ndk build -t arm64-v8a --release

# Build for x86_64
cargo ndk build -t x86_64 --release
```

**Note:** Both tools produce binaries in the same location: `target/<rust-target>/release/overlayfs-metamodule`

#### Collect Binaries and Package

```bash
mkdir -p out/module

# Copy binaries
cp target/aarch64-linux-android/release/overlayfs-metamodule \
   out/module/mm-overlayfs-aarch64
cp target/x86_64-linux-android/release/overlayfs-metamodule \
   out/module/mm-overlayfs-x86_64

# Copy metamodule files
cp metamodule/module.prop out/module/
cp metamodule/module-mount.sh out/module/
cp metamodule/customize.sh out/module/

# Set permissions
chmod 755 out/module/*.sh
chmod 755 out/module/mm-overlayfs-*

# Create ZIP package
cd out/module
zip -r ../ksu-official-mount-v1.0.0.zip .
cd ../..
```

## Development Builds

For faster development iteration (without optimization):

```bash
cargo build --target aarch64-linux-android
```

The debug binary will be at: `target/aarch64-linux-android/debug/overlayfs-metamodule`

## Installation Process

### Architecture Selection

During installation, the `customize.sh` script automatically:

1. **Detects device architecture** using `ro.product.cpu.abi` property
2. **Validates support** - Only arm64-v8a and x86_64 are supported
3. **Selects the correct binary** - Renames architecture-specific binary to `mm-overlayfs`
4. **Removes unused binaries** - Deletes the other architecture's binary
5. **Sets permissions** - Ensures the binary is executable

**Result**: Only ~500KB of the required binary is kept on the device, saving storage space.

### File Layout

**Before installation (ZIP contents)**:
- `mm-overlayfs-aarch64` - ARM64 binary
- `mm-overlayfs-x86_64` - x86_64 binary
- `customize.sh` - Installation script
- `module-mount.sh` - Mount script
- `module.prop` - Metadata

**After installation**:
- `mm-overlayfs` - Selected architecture binary only
- `customize.sh` - Kept for reference
- `module-mount.sh` - Mount script
- `module.prop` - Metadata

## Testing

### Run Clippy (Linter)

```bash
cargo clippy --target aarch64-linux-android
```

### Format Code

```bash
cargo fmt
```

### Check Compilation

```bash
cargo check --target aarch64-linux-android
```

## Binary Size Optimization

The `Cargo.toml` already includes aggressive optimization settings:

```toml
[profile.release]
strip = true           # Remove symbols
opt-level = "z"        # Optimize for size
lto = true             # Link-time optimization
codegen-units = 1      # Better optimization
panic = "abort"        # Smaller panic handler
```

Typical binary sizes (stripped):
- aarch64: ~400-500 KB
- x86_64: ~450-550 KB

## Troubleshooting

### Build Tool Not Found

**Error:**
```
Error: Neither cross nor cargo-ndk found!
```

**Solution:**
Install one of the build tools:
```bash
# Option 1: Install cross (recommended)
cargo install cross

# Option 2: Install cargo-ndk
cargo install cargo-ndk
```

If using `cross`, ensure Docker is installed and running.

### NDK Linker Not Found (cargo-ndk only)

**Error:**
```
error: linker `aarch64-linux-android21-clang` not found
```

**Solution:**
1. Verify `ANDROID_NDK_HOME` is set correctly
2. Check your `.cargo/config.toml` paths
3. Ensure NDK version compatibility

**Note:** If you're using `cross`, you can ignore this error - it doesn't apply to cross.

### Target Not Installed

**Error:**
```
error: failed to run `rustc` to learn about target-specific information
```

**Solution:**
```bash
rustup target add aarch64-linux-android
```

### Rustix Dependency Issues

**Error:**
```
error: failed to compile `rustix`
```

**Solution:**
The project uses a forked version of rustix. Ensure you have network access to:
```
https://github.com/Kernel-SU/rustix.git
```

If building offline, you might need to vendor dependencies:
```bash
cargo vendor
```

### Cross-Compilation Fails on macOS

If you encounter linking errors on macOS with Apple Silicon:

```bash
# Install cross-compilation tools
brew install llvm

# Update PATH
export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
```

## CI/CD Build

For automated builds in GitHub Actions, see the CI configuration in the main KernelSU repository.

Example workflow snippet:

```yaml
- name: Setup NDK
  uses: nttld/setup-ndk@v1
  with:
    ndk-version: r26b

- name: Build
  run: |
    cd userspace/ksu-metamodule
    ./build.sh
```

## Advanced Topics

### Custom Build Targets

To add support for more architectures:

1. Add target to rustup:
   ```bash
   rustup target add <target-triple>
   ```

2. Add linker configuration to `.cargo/config.toml`

3. Update `build.sh` to build for the new target

4. Update `module-mount.sh` to detect and use the new binary

### Debugging

To include debug symbols in release builds:

```toml
[profile.release]
strip = false
debug = true
```

Then use `addr2line` or similar tools for stack trace analysis.

### Static Linking

The project uses dynamic linking by default. For static builds:

```toml
[target.'cfg(target_env = "musl")']
rustflags = ["-C", "target-feature=+crt-static"]
```

Note: Android uses Bionic libc, not musl, so full static linking may not be possible.

## Building on Different Platforms

### Linux

Should work out of the box after NDK setup.

### macOS

Works with both Intel and Apple Silicon. Use Homebrew for dependencies.

### Windows

Use WSL2 (Windows Subsystem for Linux) for the best experience:

```powershell
wsl --install
```

Then follow Linux instructions inside WSL.

## Contributing

When submitting builds:

1. Always test on real hardware
2. Verify all architectures build successfully
3. Check binary sizes are reasonable
4. Run `cargo clippy` and `cargo fmt`
5. Update version in `Cargo.toml` and `module.prop`

## Questions?

- Open an issue: https://github.com/tiann/KernelSU/issues
- Telegram: https://t.me/KernelSU
