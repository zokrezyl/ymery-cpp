#!/bin/bash
#
# Download pre-built wgpu-native for Android
# This script downloads the pre-built wgpu-native library from GitHub releases
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"

# Output directory - must match path in android/ymery/app/src/main/cpp/CMakeLists.txt
OUTPUT_DIR="$SCRIPT_DIR/app/libs"

# wgpu-native version with Android pre-builts
WGPU_VERSION="v24.0.0.2"

# Check if already downloaded
if [ -f "$OUTPUT_DIR/arm64-v8a/libwgpu_native.so" ]; then
    echo "wgpu-native already exists: $OUTPUT_DIR/arm64-v8a/libwgpu_native.so"
    exit 0
fi

# Create output directory
mkdir -p "$OUTPUT_DIR/arm64-v8a"

echo "Downloading pre-built wgpu-native ${WGPU_VERSION} for Android..."
WGPU_URL="https://github.com/gfx-rs/wgpu-native/releases/download/${WGPU_VERSION}/wgpu-android-aarch64-release.zip"

curl -L -o "/tmp/wgpu-android.zip" "$WGPU_URL"
unzip -o "/tmp/wgpu-android.zip" -d "/tmp/wgpu-android"

# Copy library
cp "/tmp/wgpu-android/lib/libwgpu_native.so" "$OUTPUT_DIR/arm64-v8a/"

# Cleanup
rm -rf "/tmp/wgpu-android" "/tmp/wgpu-android.zip"

echo ""
echo "wgpu-native downloaded: $OUTPUT_DIR/arm64-v8a/libwgpu_native.so"
