# Ymery Embedded Mode Integration

This shows how to integrate ymery widgets into another application that owns the WebGPU/OpenGL render loop.

## Integration Pattern

The host app (e.g., yetty terminal) owns:
- WebGPU/OpenGL context initialization
- Window management (GLFW or other)
- Main render loop
- ImGui context creation and initialization

Ymery in embedded mode:
- Skips all graphics initialization
- Only manages widgets and data binding
- Renders widgets when asked (between NewFrame and Render)

## Example Integration Code

```cpp
#include <ymery/embedded.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_wgpu.h>

// In your app initialization (AFTER ImGui is initialized):
ymery::EmbeddedConfig config;
config.layout_paths.push_back("path/to/layouts");
config.plugin_paths.push_back("path/to/plugins");
config.main_module = "app";

auto ymery_res = ymery::EmbeddedApp::create(config);
if (!ymery_res) {
    // Handle error
    std::cerr << "Failed to create ymery: " << ymery::error_msg(ymery_res) << std::endl;
    return 1;
}
auto ymery = *ymery_res;

// In your render loop:
void render_frame() {
    // ... your pre-frame work ...

    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Render your own ImGui content
    render_my_terminal();

    // Render ymery widgets (they appear as additional ImGui windows)
    ymery->render_widgets();

    ImGui::Render();

    // ... submit to GPU ...
}

// On shutdown (BEFORE destroying ImGui):
ymery->dispose();
ymery.reset();

ImGui_ImplWGPU_Shutdown();
ImGui_ImplGlfw_Shutdown();
ImGui::DestroyContext();
```

## For yetty integration

In yetty's main.cpp, after WebGPU and ImGui are initialized:

```cpp
#include <ymery/embedded.hpp>

// After ImGui init...
ymery::EmbeddedConfig ymery_config;
ymery_config.layout_paths.push_back("ymery-layouts");
ymery_config.plugin_paths.push_back("ymery-plugins");
ymery_config.main_module = "terminal-overlay";

auto ymery = ymery::EmbeddedApp::create(ymery_config);

// In mainLoopIteration(), after terminal rendering:
if (ymery) {
    // Make sure ImGui NewFrame was called
    ymery->render_widgets();
}
```

This allows ymery widgets to overlay on top of the terminal rendering.
