{
  description = "ymery-cpp development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.05";
    nixpkgs-unstable.url = "github:NixOS/nixpkgs/nixos-unstable";
    rust-overlay.url = "github:oxalica/rust-overlay";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, nixpkgs-unstable, rust-overlay, flake-utils }:
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
        # Use unstable for emscripten (needs Clang 18+ for libc++)
        pkgsUnstable = import nixpkgs-unstable {
          inherit system;
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

          # Web/Emscripten build environment (uses unstable for newer emscripten with Clang 18+)
          web = pkgs.mkShell {
            buildInputs = [
              pkgsUnstable.emscripten
              pkgs.cmake
              pkgs.ninja
              pkgsUnstable.nodejs
              pkgs.python3
            ];

            # Use XDG cache or /tmp for emscripten cache to avoid permission issues
            EM_CACHE = "/tmp/ymery-em-cache";

            shellHook = ''
              mkdir -p "$EM_CACHE"
              echo "Emscripten cache: $EM_CACHE"
              echo "Emscripten version: $(emcc --version | head -1)"
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
