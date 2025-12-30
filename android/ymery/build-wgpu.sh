#!/bin/bash
#
# Download pre-built wgpu-native for Android
# This script downloads the pre-built wgpu-native library from GitHub releases
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"

# Output directories
LIB_DIR="$SCRIPT_DIR/app/libs"
INCLUDE_DIR="$PROJECT_ROOT/external/wgpu-native/include"

# wgpu-native version - use latest stable release
WGPU_VERSION="v27.0.4.0"

# Check if already downloaded
if [ -f "$LIB_DIR/arm64-v8a/libwgpu_native.so" ] && [ -f "$INCLUDE_DIR/webgpu/webgpu.h" ]; then
    echo "wgpu-native already exists:"
    echo "  Library: $LIB_DIR/arm64-v8a/libwgpu_native.so"
    echo "  Headers: $INCLUDE_DIR"
    exit 0
fi

# Create output directories
mkdir -p "$LIB_DIR/arm64-v8a"
mkdir -p "$INCLUDE_DIR"

echo "Downloading pre-built wgpu-native ${WGPU_VERSION} for Android..."
WGPU_URL="https://github.com/gfx-rs/wgpu-native/releases/download/${WGPU_VERSION}/wgpu-android-aarch64-release.zip"

curl -L -o "/tmp/wgpu-android.zip" "$WGPU_URL"
unzip -o "/tmp/wgpu-android.zip" -d "/tmp/wgpu-android"

# Copy library
cp "/tmp/wgpu-android/lib/libwgpu_native.so" "$LIB_DIR/arm64-v8a/"

# Copy headers (shared with desktop)
cp -r "/tmp/wgpu-android/include"/* "$INCLUDE_DIR/"

# Cleanup
rm -rf "/tmp/wgpu-android" "/tmp/wgpu-android.zip"

echo ""
echo "wgpu-native downloaded:"
echo "  Library: $LIB_DIR/arm64-v8a/libwgpu_native.so"
echo "  Headers: $INCLUDE_DIR"
