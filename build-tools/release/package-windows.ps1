#
# Package Windows release build
#

param(
    [string]$BuildDir = "build",
    [string]$Version = "dev",
    [string]$OutputDir = "dist"
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $ScriptDir)

Set-Location $ProjectRoot

if (-not (Test-Path $BuildDir)) {
    Write-Error "Build directory not found: $BuildDir"
    exit 1
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$Arch = "x86_64"
$PackageName = "ymery-windows-${Arch}-${Version}"
$PackageDir = Join-Path $OutputDir $PackageName

if (Test-Path $PackageDir) {
    Remove-Item -Recurse -Force $PackageDir
}

New-Item -ItemType Directory -Force -Path $PackageDir | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $PackageDir "plugins") | Out-Null

# Copy main executable (MSVC multi-config puts it in Release/)
$ExePath = Join-Path $BuildDir "Release/ymery.exe"
if (Test-Path $ExePath) {
    Copy-Item $ExePath $PackageDir
} else {
    # Fallback to root (single-config generators)
    $ExePath = Join-Path $BuildDir "ymery.exe"
    if (Test-Path $ExePath) {
        Copy-Item $ExePath $PackageDir
    }
}

# Copy ymery_lib shared library (MSVC multi-config puts it in Release/)
$LibPath = Join-Path $BuildDir "Release/ymery_lib.dll"
if (Test-Path $LibPath) {
    Copy-Item $LibPath $PackageDir
} else {
    # Fallback to root (single-config generators)
    $LibPath = Join-Path $BuildDir "ymery_lib.dll"
    if (Test-Path $LibPath) {
        Copy-Item $LibPath $PackageDir
    }
}

# Copy plugins
# Try plugins/Release/ first (MSVC multi-config with RUNTIME_OUTPUT_DIRECTORY)
# Then plugins/ (single-config with RUNTIME_OUTPUT_DIRECTORY)
# Finally build root (MSVC without RUNTIME_OUTPUT_DIRECTORY - DLLs go to root)
$PluginsCopied = $false

$PluginsDir = Join-Path $BuildDir "plugins/Release"
if (Test-Path $PluginsDir) {
    Get-ChildItem -Path $PluginsDir -Filter "*.dll" | ForEach-Object {
        Copy-Item $_.FullName (Join-Path $PackageDir "plugins")
        $PluginsCopied = $true
    }
}

if (-not $PluginsCopied) {
    $PluginsDir = Join-Path $BuildDir "plugins"
    if (Test-Path $PluginsDir) {
        Get-ChildItem -Path $PluginsDir -Filter "*.dll" | ForEach-Object {
            Copy-Item $_.FullName (Join-Path $PackageDir "plugins")
            $PluginsCopied = $true
        }
    }
}

# Fallback: copy plugin DLLs from build root (excluding core DLLs)
if (-not $PluginsCopied) {
    Get-ChildItem -Path $BuildDir -Filter "*.dll" | Where-Object {
        $_.Name -notin @("ymery_lib.dll", "wgpu_native.dll")
    } | ForEach-Object {
        Copy-Item $_.FullName (Join-Path $PackageDir "plugins")
    }
}

# Copy wgpu library
$WgpuDll = Join-Path $BuildDir "wgpu-native/lib/wgpu_native.dll"
if (Test-Path $WgpuDll) {
    Copy-Item $WgpuDll $PackageDir
}

# Create archive
$ZipPath = Join-Path $OutputDir "${PackageName}.zip"
if (Test-Path $ZipPath) {
    Remove-Item $ZipPath
}

Compress-Archive -Path $PackageDir -DestinationPath $ZipPath
Remove-Item -Recurse -Force $PackageDir

Write-Host "Created: $ZipPath"
