{
  description = "ymery-cpp development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        devShells = {
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
