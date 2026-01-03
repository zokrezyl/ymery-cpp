#!/bin/bash
#
# Package WebAssembly release build
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"

BUILD_DIR="${1:-build-webasm-webgpu-release}"
VERSION="${2:-dev}"
OUTPUT_DIR="${3:-$PROJECT_ROOT/dist}"

cd "$PROJECT_ROOT"

if [ ! -d "$BUILD_DIR" ]; then
    echo "Build directory not found: $BUILD_DIR"
    echo "Run 'make webasm-webgpu-release' first"
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

PACKAGE_NAME="ymery-web-${VERSION}"
PACKAGE_DIR="$OUTPUT_DIR/$PACKAGE_NAME"

rm -rf "$PACKAGE_DIR"
mkdir -p "$PACKAGE_DIR"

# Copy WebAssembly files
cp "$BUILD_DIR/ymery.js" "$PACKAGE_DIR/" 2>/dev/null || true
cp "$BUILD_DIR/ymery.wasm" "$PACKAGE_DIR/" 2>/dev/null || true
cp "$BUILD_DIR/ymery.html" "$PACKAGE_DIR/index.html" 2>/dev/null || true
cp "$BUILD_DIR/ymery.data" "$PACKAGE_DIR/" 2>/dev/null || true

# Copy any worker files
cp "$BUILD_DIR"/*.worker.js "$PACKAGE_DIR/" 2>/dev/null || true

# Create archive
cd "$OUTPUT_DIR"
zip -r "${PACKAGE_NAME}.zip" "$PACKAGE_NAME"
rm -rf "$PACKAGE_DIR"

echo "Created: $OUTPUT_DIR/${PACKAGE_NAME}.zip"
