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
              echo "Emscripten web build environment ready"
              echo "Run: cmake -B build-web -G Ninja && cmake --build build-web"
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
