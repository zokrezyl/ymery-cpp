#pragma once

// ymery WebAssembly/Emscripten mode - standalone web application
// Uses Emscripten's browser WebGPU API which differs from wgpu-native
// Key differences: swapchain-based surface, different API for textures

#ifdef YMERY_WEB

#include "result.hpp"
#include "types.hpp"
#include "lang.hpp"
#include "dispatcher.hpp"
#include "plugin_manager.hpp"
#include "frontend/widget.hpp"
#include "frontend/widget_factory.hpp"
#include <filesystem>
#include <memory>
#include <webgpu/webgpu.h>

// Forward declare ImGui types
struct ImGuiContext;
struct ImPlotContext;

namespace ymery {

// Configuration for web app
struct WebAppConfig {
    // Canvas selector for WebGPU surface
    const char* canvas_selector = "#canvas";

    // Layout configuration
    std::vector<std::filesystem::path> layout_paths;
    std::vector<std::filesystem::path> plugin_paths;
    std::string main_module = "app";

    // Window configuration
    std::string window_title = "ymery";
    uint32_t window_width = 1280;
    uint32_t window_height = 720;
};

// WebAssembly ymery application - runs in browser with WebGPU
class WebApp {
public:
    // Factory method
    static Result<std::shared_ptr<WebApp>> create(const WebAppConfig& config);

    // Lifecycle
    Result<void> dispose();

    // Main loop entry point - registers with emscripten_set_main_loop
    Result<void> run();

    // Single frame (called by main loop)
    Result<void> frame();

    // Access components
    std::shared_ptr<Lang> lang() const { return _lang; }
    std::shared_ptr<Dispatcher> dispatcher() const { return _dispatcher; }
    std::shared_ptr<WidgetFactory> widget_factory() const { return _widget_factory; }
    std::shared_ptr<PluginManager> plugin_manager() const { return _plugin_manager; }
    std::shared_ptr<TreeLike> data_tree() const { return _data_tree; }

    // Check if app should close
    bool should_close() const { return _should_close; }
    void request_close() { _should_close = true; }

private:
    WebApp() = default;

    Result<void> _init();
    Result<void> _init_graphics();
    Result<void> _shutdown_graphics();
    Result<void> _begin_frame();
    Result<void> _end_frame();

    Result<void> _init_imgui();
    void _shutdown_imgui();

    WebAppConfig _config;

    // WebGPU state (Emscripten browser API)
    WGPUInstance _wgpu_instance = nullptr;
    WGPUAdapter _wgpu_adapter = nullptr;
    WGPUDevice _wgpu_device = nullptr;
    WGPUSurface _wgpu_surface = nullptr;
    WGPUQueue _wgpu_queue = nullptr;
    WGPUSwapChain _wgpu_swapchain = nullptr;
    WGPUTextureFormat _wgpu_format = WGPUTextureFormat_BGRA8Unorm;
    int _surface_width = 0;
    int _surface_height = 0;

    // ImGui context
    ImGuiContext* _imgui_ctx = nullptr;
    ImPlotContext* _implot_ctx = nullptr;

    // ymery components
    std::shared_ptr<Lang> _lang;
    std::shared_ptr<Dispatcher> _dispatcher;
    std::shared_ptr<WidgetFactory> _widget_factory;
    std::shared_ptr<PluginManager> _plugin_manager;
    std::shared_ptr<TreeLike> _data_tree;

    WidgetPtr _root_widget;
    bool _should_close = false;
};

using WebAppPtr = std::shared_ptr<WebApp>;

} // namespace ymery

#endif // YMERY_WEB
