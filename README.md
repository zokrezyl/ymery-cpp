# ymery-cpp

C++ implementation of the ymery UI framework using ImGui with YAML-based widget definitions.

For the sister project, python based implementation, see https://github.com/zokrezyl/ymery. That repo contains more documentation as well as an online demo https://zokrezyl.github.io/ymery/demo/index.html

The two repos will be merged at a certain moment.

## Features

- YAML-based UI layout definitions
- Plugin-based widget system
- Multiple graphics backends: OpenGL, WebGPU (native), WebGPU (web/Emscripten)
- ImGui with docking support
- ImPlot for charts
- imgui-node-editor for node graphs
- imgui_markdown for markdown rendering
- powerfull layout editor

## Building

### Prerequisites

- CMake 3.20+
- C++23 compatible compiler (GCC 13+, Clang 16+)
- For OpenGL: OpenGL development libraries
- For Web: Emscripten SDK 3.1.40+ (for WebGPU support with ImGui 1.91+)

### OpenGL Backend (Default)

```bash
mkdir build && cd build
cmake .. -DCPM_SOURCE_CACHE=~/.cache/CPM
make -j$(nproc)
```

### WebGPU Backend (Native)

Uses wgpu-native for native WebGPU support.

```bash
mkdir build-webgpu && cd build-webgpu
cmake .. -DYMERY_USE_WEBGPU=ON -DCPM_SOURCE_CACHE=~/.cache/CPM
make -j$(nproc)
```

### Web (Emscripten)

Builds to WebAssembly with WebGPU support for browsers.

**Note:** Requires Emscripten 3.1.40+ for WebGPU. Ubuntu's packaged version (3.1.6) is too old.
Install via emsdk:

```bash
# Install emsdk (one-time setup)
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
```

Build:

```bash
# Activate Emscripten environment
source /path/to/emsdk/emsdk_env.sh

mkdir build-web && cd build-web
emcmake cmake .. -DCPM_SOURCE_CACHE=~/.cache/CPM
emmake make -j$(nproc)
```

This generates:
- `ymery-cli.html` - Main HTML file
- `ymery-cli.js` - JavaScript glue code
- `ymery-cli.wasm` - WebAssembly binary
- `ymery-cli.data` - Preloaded assets (layout files)

To test locally:
```bash
python3 -m http.server 8080
# Open http://localhost:8080/ymery-cli.html in a WebGPU-capable browser
```

## Running

### Native

```bash
./build/ymery-cli -l ./layout -m app
# or for WebGPU:
./build-webgpu/ymery-cli -l ./layout -m app
```

### Web

Open `build-web/ymery-cli.html` in a WebGPU-capable browser (Chrome 113+, Edge 113+, Firefox Nightly with flags).

## Project Structure

```
ymery-cpp/
├── src/
│   ├── main.cpp              # Entry point
│   └── ymery/
│       ├── app.cpp           # Application class
│       ├── types.cpp         # Type system
│       ├── dispatcher.cpp    # Event dispatcher
│       ├── lang.cpp          # YAML layout parser
│       ├── plugin_manager.cpp# Plugin loading
│       ├── frontend/         # Widget system
│       │   ├── widget.cpp
│       │   ├── composite.cpp
│       │   └── widget_factory.cpp
│       └── plugins/          # Widget plugins
│           ├── frontend/     # UI widgets (button, text, etc.)
│           └── backend/      # Data tree plugins
├── layout/                   # YAML layout files
├── examples/                 # Example applications
└── CMakeLists.txt
```

## Configuration

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `YMERY_USE_WEBGPU` | OFF | Use WebGPU instead of OpenGL |
| `CPM_SOURCE_CACHE` | - | Cache directory for CPM dependencies |

## License

See LICENSE file.
