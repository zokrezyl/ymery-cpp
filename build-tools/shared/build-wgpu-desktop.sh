#!/bin/bash
#
# Download pre-built wgpu-native for desktop (Linux, macOS, Windows)
# No build dependencies required - just downloads from GitHub releases
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"

# Accept build directory as argument, default to build-desktop-webgpu-release
BUILD_DIR="${1:-build-desktop-webgpu-release}"
OUTPUT_DIR="$PROJECT_ROOT/$BUILD_DIR/wgpu-native"

# Source centralized versions
source "$SCRIPT_DIR/versions.env"

mkdir -p "$OUTPUT_DIR"

# Detect platform and architecture
detect_platform() {
    local OS=$(uname -s)
    local UARCH=$(uname -m)

    case "$OS" in
        Linux)
            PLATFORM="linux"
            LIB_NAME="libwgpu_native.so"
            ;;
        Darwin)
            PLATFORM="macos"
            LIB_NAME="libwgpu_native.dylib"
            ;;
        MINGW*|MSYS*|CYGWIN*|Windows_NT)
            PLATFORM="windows"
            LIB_NAME="wgpu_native.dll"
            COMPILER_SUFFIX="-msvc"
            ;;
        *)
            echo "Unsupported OS: $OS"
            exit 1
            ;;
    esac

    case "$UARCH" in
        x86_64|amd64|AMD64)
            ARCH="x86_64"
            ;;
        aarch64|arm64|ARM64)
            ARCH="aarch64"
            ;;
        *)
            echo "Unsupported architecture: $UARCH"
            exit 1
            ;;
    esac

    echo "Detected: $PLATFORM-$ARCH"
}

detect_platform

# Check if already downloaded
if [ -f "$OUTPUT_DIR/lib/$LIB_NAME" ] && [ -f "$OUTPUT_DIR/include/webgpu/webgpu.h" ]; then
    echo "wgpu-native already exists: $OUTPUT_DIR"
    exit 0
fi

# Construct download URL
WGPU_ARCHIVE="${PLATFORM}-${ARCH}${COMPILER_SUFFIX:-}-release"
WGPU_URL="https://github.com/gfx-rs/wgpu-native/releases/download/${WGPU_VERSION}/wgpu-${WGPU_ARCHIVE}.zip"

echo "Downloading pre-built wgpu-native ${WGPU_VERSION} for ${PLATFORM}-${ARCH}..."
echo "  URL: $WGPU_URL"

TMP_DIR="/tmp/wgpu-download-$$"
mkdir -p "$TMP_DIR"

curl -fsSL -o "$TMP_DIR/wgpu.zip" "$WGPU_URL"

echo "Extracting..."
unzip -o "$TMP_DIR/wgpu.zip" -d "$TMP_DIR/wgpu"

# Copy library and headers
cp -r "$TMP_DIR/wgpu/lib" "$OUTPUT_DIR/"
cp -r "$TMP_DIR/wgpu/include" "$OUTPUT_DIR/"

rm -rf "$TMP_DIR"

echo ""
echo "wgpu-native downloaded:"
echo "  Library: $OUTPUT_DIR/lib/$LIB_NAME"
echo "  Headers: $OUTPUT_DIR/include/"
