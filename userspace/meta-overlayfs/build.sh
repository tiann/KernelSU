#!/bin/bash
set -e

# Configuration
VERSION=$(grep '^version' Cargo.toml | head -1 | sed 's/.*"\(.*\)".*/\1/')
OUTPUT_DIR="target"
METAMODULE_DIR="metamodule"
MODULE_OUTPUT_DIR="$OUTPUT_DIR/module"

echo "=========================================="
echo "Building meta-overlayfs v${VERSION}"
echo "=========================================="

# Detect build tool
if command -v cross >/dev/null 2>&1; then
    BUILD_TOOL="cross"
    echo "Using cross for compilation"
else
    BUILD_TOOL="cargo-ndk"
    echo "Using cargo ndk for compilation"
    if ! command -v cargo-ndk >/dev/null 2>&1; then
        echo "Error: Neither cross nor cargo-ndk found!"
        echo "Please install one of them:"
        echo "  - cross: cargo install cross"
        echo "  - cargo-ndk: cargo install cargo-ndk"
        exit 1
    fi
fi

# Clean output directory
echo "Cleaning output directory..."
rm -rf "$OUTPUT_DIR"
mkdir -p "$MODULE_OUTPUT_DIR"

# Build for multiple architectures
echo ""
echo "Building for aarch64-linux-android..."
if [ "$BUILD_TOOL" = "cross" ]; then
    cross build --release --target aarch64-linux-android
else
    cargo ndk build -t arm64-v8a --release
fi

echo ""
echo "Building for x86_64-linux-android..."
if [ "$BUILD_TOOL" = "cross" ]; then
    cross build --release --target x86_64-linux-android
else
    cargo ndk build -t x86_64 --release
fi

# Copy binaries
echo ""
echo "Copying binaries..."
cp target/aarch64-linux-android/release/meta-overlayfs \
   "$MODULE_OUTPUT_DIR/meta-overlayfs-aarch64"
cp target/x86_64-linux-android/release/meta-overlayfs \
   "$MODULE_OUTPUT_DIR/meta-overlayfs-x86_64"

# Copy metamodule files
echo "Copying metamodule files..."
cp "$METAMODULE_DIR"/module.prop "$MODULE_OUTPUT_DIR/"
cp "$METAMODULE_DIR"/*.sh "$MODULE_OUTPUT_DIR/"

# Set permissions
echo "Setting permissions..."
chmod 755 "$MODULE_OUTPUT_DIR"/*.sh
chmod 755 "$MODULE_OUTPUT_DIR"/meta-overlayfs-*

# Display binary sizes
echo ""
echo "Binary sizes:"
echo "  aarch64: $(du -h "$MODULE_OUTPUT_DIR"/meta-overlayfs-aarch64 | awk '{print $1}')"
echo "  x86_64:  $(du -h "$MODULE_OUTPUT_DIR"/meta-overlayfs-x86_64 | awk '{print $1}')"

# Package
echo ""
echo "Packaging..."
cd "$MODULE_OUTPUT_DIR"
ZIP_NAME="meta-overlayfs-v${VERSION}.zip"
zip -r "../$ZIP_NAME" .
cd ../..

echo ""
echo "=========================================="
echo "Build completed successfully!"
echo "Output: $OUTPUT_DIR/$ZIP_NAME"
echo "=========================================="
echo ""
echo "To install:"
echo "  adb push $OUTPUT_DIR/$ZIP_NAME /sdcard/"
echo "  adb shell su -c 'ksud module install /sdcard/$ZIP_NAME'"
