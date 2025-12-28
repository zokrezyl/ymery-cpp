{
  description = "ymery-cpp development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.05";
    rust-overlay.url = "github:oxalica/rust-overlay";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, rust-overlay, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        overlays = [ (import rust-overlay) ];
        pkgs = import nixpkgs {
          inherit system overlays;
          config = {
            allowUnfree = true;
            android_sdk.accept_license = true;
          };
        };

        # Android SDK/NDK setup
        androidComposition = pkgs.androidenv.composeAndroidPackages {
          platformVersions = [ "34" ];
          buildToolsVersions = [ "34.0.0" ];
          ndkVersions = [ "26.1.10909125" ];
          includeNDK = true;
        };

        androidSdk = androidComposition.androidsdk;
        androidNdk = "${androidSdk}/libexec/android-sdk/ndk/26.1.10909125";

        # Rust with Android target
        rustToolchain = pkgs.rust-bin.stable.latest.default.override {
          targets = [ "aarch64-linux-android" ];
        };
      in
      {
        devShells = {
          # Android build environment (for wgpu-native cross-compilation)
          android = pkgs.mkShell {
            buildInputs = with pkgs; [
              rustToolchain
              androidSdk
              llvmPackages.libclang
              llvmPackages.clang
              cmake
              ninja
              pkg-config
              git
              zlib
              openssl
            ];

            ANDROID_HOME = "${androidSdk}/libexec/android-sdk";
            ANDROID_SDK_ROOT = "${androidSdk}/libexec/android-sdk";
            ANDROID_NDK_HOME = androidNdk;
            NDK_HOME = androidNdk;
            JAVA_HOME = "${pkgs.jdk17}";
            LIBCLANG_PATH = "${pkgs.llvmPackages.libclang.lib}/lib";

            shellHook = ''
              export IN_NIX_SHELL=1
              echo "ymery Android build environment"
              echo "  Rust: $(rustc --version)"
              echo "  Android SDK: $ANDROID_HOME"
              echo "  Android NDK: $ANDROID_NDK_HOME"
            '';
          };

          # Web/Emscripten build environment
          web = pkgs.mkShell {
            buildInputs = with pkgs; [
              emscripten
              cmake
              ninja
              nodejs
              python3
            ];

            shellHook = ''
              export EM_CACHE="$PWD/.em_cache"
              mkdir -p "$EM_CACHE"
              # Workaround: Nix emscripten has file_packager.py but not file_packager wrapper
              # Create wrapper in tools dir overlay
              export EMSCRIPTEN_TOOLS_ORIG="$(dirname $(which emcc))/../share/emscripten/tools"
              mkdir -p "$EM_CACHE/tools"
              if [ -f "$EMSCRIPTEN_TOOLS_ORIG/file_packager.py" ]; then
                cat > "$EM_CACHE/tools/file_packager" << 'WRAPPER'
#!/usr/bin/env python3
import sys, os
script_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, script_dir)
exec(open(os.environ.get("EMSCRIPTEN_TOOLS_ORIG") + "/file_packager.py").read())
WRAPPER
                chmod +x "$EM_CACHE/tools/file_packager"
                # Copy all tools and replace file_packager
                cp -rn "$EMSCRIPTEN_TOOLS_ORIG"/* "$EM_CACHE/tools/" 2>/dev/null || true
              fi
            '';
          };

          # Default shell with all tools
          default = pkgs.mkShell {
            buildInputs = with pkgs; [
              # Build tools
              cmake
              ninja
              pkg-config
              gnumake

              # C++ compiler
              gcc

              # Desktop dependencies (OpenGL)
              glfw
              libGL
              mesa

              # X11 dependencies
              xorg.libX11
              xorg.libXrandr
              xorg.libXinerama
              xorg.libXcursor
              xorg.libXi
              xorg.libXext

              # Emscripten for web builds
              emscripten
              nodejs
              python3
            ];

            shellHook = ''
              export EM_CACHE="$PWD/.em_cache"
              mkdir -p "$EM_CACHE"
              echo "ymery-cpp development environment ready"
            '';
          };
        };
      }
    );
}
