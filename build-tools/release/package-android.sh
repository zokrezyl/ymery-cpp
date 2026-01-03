#!/bin/bash
#
# Package Android release APKs
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"

VERSION="${1:-dev}"
OUTPUT_DIR="${2:-$PROJECT_ROOT/dist}"

cd "$PROJECT_ROOT"

APK_BASE="build-tools/android/ymery/app/build/outputs/apk"

mkdir -p "$OUTPUT_DIR"

# Package OpenGL APK
OPENGL_APK="$APK_BASE/opengl/release/app-opengl-release.apk"
if [ -f "$OPENGL_APK" ]; then
    cp "$OPENGL_APK" "$OUTPUT_DIR/ymery-android-opengl-${VERSION}.apk"
    echo "Created: $OUTPUT_DIR/ymery-android-opengl-${VERSION}.apk"
else
    echo "OpenGL APK not found: $OPENGL_APK"
fi

# Package WebGPU APK
WEBGPU_APK="$APK_BASE/webgpu/release/app-webgpu-release.apk"
if [ -f "$WEBGPU_APK" ]; then
    cp "$WEBGPU_APK" "$OUTPUT_DIR/ymery-android-webgpu-${VERSION}.apk"
    echo "Created: $OUTPUT_DIR/ymery-android-webgpu-${VERSION}.apk"
else
    echo "WebGPU APK not found: $WEBGPU_APK"
fi
