#!/bin/bash
#
# Package Linux release build
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"

BUILD_DIR="${1:-build-desktop-webgpu-release}"
VERSION="${2:-dev}"
OUTPUT_DIR="${3:-$PROJECT_ROOT/dist}"

cd "$PROJECT_ROOT"

if [ ! -d "$BUILD_DIR" ]; then
    echo "Build directory not found: $BUILD_DIR"
    echo "Run 'make desktop-webgpu-release' first"
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

ARCH=$(uname -m)
PACKAGE_NAME="ymery-linux-${ARCH}-${VERSION}"
PACKAGE_DIR="$OUTPUT_DIR/$PACKAGE_NAME"

rm -rf "$PACKAGE_DIR"
mkdir -p "$PACKAGE_DIR/plugins"

# Copy main executable
if [ -f "$BUILD_DIR/ymery" ]; then
    cp "$BUILD_DIR/ymery" "$PACKAGE_DIR/"
fi

# Copy plugins
if [ -d "$BUILD_DIR/plugins" ]; then
    cp "$BUILD_DIR/plugins"/*.so "$PACKAGE_DIR/plugins/" 2>/dev/null || true
fi

# Copy wgpu library
if [ -f "$BUILD_DIR/wgpu-native/lib/libwgpu_native.so" ]; then
    cp "$BUILD_DIR/wgpu-native/lib/libwgpu_native.so" "$PACKAGE_DIR/"
fi

# Create archive
cd "$OUTPUT_DIR"
zip -r "${PACKAGE_NAME}.zip" "$PACKAGE_NAME"
rm -rf "$PACKAGE_DIR"

echo "Created: $OUTPUT_DIR/${PACKAGE_NAME}.zip"
