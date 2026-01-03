# Release Process

## Overview

This directory contains scripts for packaging ymery builds and a GitHub Actions workflow for automated releases.

## Supported Platforms

| Platform | Backend | Artifact |
|----------|---------|----------|
| Linux x86_64 | WebGPU | `ymery-linux-x86_64-{version}.zip` |
| Windows x86_64 | WebGPU | `ymery-windows-x86_64-{version}.zip` |
| macOS arm64 | WebGPU | `ymery-macos-arm64-{version}.zip` |
| Android | OpenGL | `ymery-android-opengl-{version}.apk` |
| Android | WebGPU | `ymery-android-webgpu-{version}.apk` |
| Web | WebGPU | `ymery-web-{version}.zip` |

## Creating a Release

### Automated (Recommended)

1. Tag the commit:
   ```bash
   git tag v1.0.0
   git push origin v1.0.0
   ```

2. The GitHub Actions workflow automatically:
   - Builds all platforms in parallel
   - Packages each build
   - Creates a GitHub Release with all artifacts

3. Find the release at: `https://github.com/{owner}/ymery-cpp/releases`

### Manual

1. Build the desired target:
   ```bash
   make desktop-webgpu-release
   ```

2. Package it:
   ```bash
   build-tools/release/package-linux.sh build-desktop-webgpu-release v1.0.0
   ```

3. Upload manually or use gh CLI:
   ```bash
   gh release create v1.0.0 dist/*.zip --generate-notes
   ```

## Packaging Scripts

### package-linux.sh

```bash
./package-linux.sh [BUILD_DIR] [VERSION] [OUTPUT_DIR]

# Example
./package-linux.sh build-desktop-webgpu-release v1.0.0 ./dist
```

Packages: `ymery`, `plugins/*.so`, `libwgpu_native.so`

### package-windows.sh / package-windows.ps1

```bash
# Git Bash / MSYS2
./package-windows.sh [BUILD_DIR] [VERSION] [OUTPUT_DIR]

# PowerShell
.\package-windows.ps1 -BuildDir build -Version v1.0.0 -OutputDir dist
```

Packages: `ymery.exe`, `plugins/*.dll`, `wgpu_native.dll`

### package-macos.sh

```bash
./package-macos.sh [BUILD_DIR] [VERSION] [OUTPUT_DIR]
```

Packages: `ymery`, `plugins/*.dylib`, `libwgpu_native.dylib`

### package-android.sh

```bash
./package-android.sh [VERSION] [OUTPUT_DIR]
```

Copies APKs from Gradle build output.

### package-web.sh

```bash
./package-web.sh [BUILD_DIR] [VERSION] [OUTPUT_DIR]
```

Packages: `ymery.js`, `ymery.wasm`, `index.html`, `ymery.data`

## GitHub Actions Workflow

Located at `.github/workflows/release.yml`

### Trigger

Triggered on push of tags matching `v*` pattern:
- `v1.0.0` - stable release
- `v1.0.0-beta.1` - prerelease (marked automatically)
- `v2.0.0-rc.1` - prerelease

### Jobs

```
build-linux ─────────┐
build-windows ───────┤
build-macos ─────────┼──► release (creates GitHub Release)
build-android-opengl ┤
build-android-webgpu ┤
build-web ───────────┘
```

All build jobs run in parallel. The `release` job waits for all builds to complete before creating the GitHub Release.

### Artifacts

Each build job uploads its artifact. The release job downloads all artifacts and attaches them to the GitHub Release.

## Version Naming

Follow semantic versioning:

- `v1.0.0` - Major.Minor.Patch
- `v1.0.0-alpha.1` - Alpha prerelease
- `v1.0.0-beta.1` - Beta prerelease
- `v1.0.0-rc.1` - Release candidate

Tags containing `-` are automatically marked as prereleases.

## Troubleshooting

### Build fails on specific platform

Check the GitHub Actions logs. Each platform has its own job, so failures are isolated.

### Missing artifacts in release

Artifacts with `if-no-files-found: ignore` won't fail the workflow. Check if the build produced the expected files.

### Local packaging fails

Ensure you've built the correct target first:
```bash
make desktop-webgpu-release  # Then package
```
