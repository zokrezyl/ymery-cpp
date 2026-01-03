#!/bin/bash
#
# Package Windows release build (Git Bash / MSYS2)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"

BUILD_DIR="${1:-build}"
VERSION="${2:-dev}"
OUTPUT_DIR="${3:-$PROJECT_ROOT/dist}"

cd "$PROJECT_ROOT"

if [ ! -d "$BUILD_DIR" ]; then
    echo "Build directory not found: $BUILD_DIR"
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

ARCH="x86_64"
PACKAGE_NAME="ymery-windows-${ARCH}-${VERSION}"
PACKAGE_DIR="$OUTPUT_DIR/$PACKAGE_NAME"

rm -rf "$PACKAGE_DIR"
mkdir -p "$PACKAGE_DIR/plugins"

# Copy main executable
if [ -f "$BUILD_DIR/Release/ymery.exe" ]; then
    cp "$BUILD_DIR/Release/ymery.exe" "$PACKAGE_DIR/"
fi

# Copy plugins
if [ -d "$BUILD_DIR/plugins/Release" ]; then
    cp "$BUILD_DIR/plugins/Release"/*.dll "$PACKAGE_DIR/plugins/" 2>/dev/null || true
fi

# Copy wgpu library
if [ -f "$BUILD_DIR/wgpu-native/lib/wgpu_native.dll" ]; then
    cp "$BUILD_DIR/wgpu-native/lib/wgpu_native.dll" "$PACKAGE_DIR/"
fi

# Create archive
cd "$OUTPUT_DIR"
zip -r "${PACKAGE_NAME}.zip" "$PACKAGE_NAME"
rm -rf "$PACKAGE_DIR"

echo "Created: $OUTPUT_DIR/${PACKAGE_NAME}.zip"
