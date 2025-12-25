#pragma once

// ymery embedded mode - for integrating ymery into another app's render loop
// The host app owns the graphics context and window, ymery renders with its own ImGui context
// Supports both WebGPU and OpenGL backends via conditional compilation

#include "result.hpp"
#include "types.hpp"
#include "lang.hpp"
#include "dispatcher.hpp"
#include "plugin_manager.hpp"
#include "frontend/widget.hpp"
#include "frontend/widget_factory.hpp"
#include <filesystem>
#include <memory>

#ifdef YMERY_USE_WEBGPU
// Forward declare WebGPU types
typedef struct WGPUDeviceImpl* WGPUDevice;
typedef struct WGPUQueueImpl* WGPUQueue;
typedef struct WGPURenderPassEncoderImpl* WGPURenderPassEncoder;
#endif

// Forward declare ImGui types
struct ImGuiContext;
struct ImPlotContext;

namespace ymery {

// Configuration for embedded mode
struct EmbeddedConfig {
#ifdef YMERY_USE_WEBGPU
    // WebGPU context (provided by host) - REQUIRED
    WGPUDevice device = nullptr;
    WGPUQueue queue = nullptr;
    int format = 0;  // WGPUTextureFormat, 0 = BGRA8Unorm
#else
    // OpenGL configuration
    const char* glsl_version = "#version 130";  // GLSL version string for ImGui
#endif

    // Layout configuration
    std::vector<std::filesystem::path> layout_paths;
    std::vector<std::filesystem::path> plugin_paths;
    std::string main_module = "app";

    // Initial size (can be updated via resize)
    uint32_t width = 800;
    uint32_t height = 600;
};

// Embedded ymery instance - runs within another app's render loop
// Each instance has its own ImGui context for isolation
class EmbeddedApp {
public:
    // Factory method
    static Result<std::shared_ptr<EmbeddedApp>> create(const EmbeddedConfig& config);

    // Lifecycle - dispose must be called before host destroys WebGPU context
    void dispose();

    // Frame rendering (called by host each frame)
    // 1. begin_frame(deltaTime) - starts ImGui frame
    // 2. render_widgets() - renders ymery widget tree
    // 3. end_frame() - finalizes and renders (WebGPU: pass encoder, OpenGL: direct)
    void begin_frame(float delta_time);
    void render_widgets();
#ifdef YMERY_USE_WEBGPU
    void end_frame(WGPURenderPassEncoder pass);
#else
    void end_frame();
#endif

    // Resize handling - call when plugin area changes size
    void resize(uint32_t width, uint32_t height);

    // Set display position offset for embedded rendering
    // This offsets where ImGui renders within the render target
    void set_display_pos(float x, float y);

    // Input forwarding (from host)
    // Host should forward events when mouse is in plugin bounds
    void on_mouse_pos(float x, float y);
    void on_mouse_button(int button, bool pressed);
    void on_mouse_scroll(float x_offset, float y_offset);
    void on_key(int key, int scancode, int action, int mods);
    void on_char(unsigned int codepoint);
    void on_focus(bool focused);

    // State queries - host uses these to decide input routing
    bool wants_keyboard() const;  // ImGui wants keyboard input
    bool wants_mouse() const;     // ImGui wants mouse input

    // Access components
    std::shared_ptr<Lang> lang() const { return _lang; }
    std::shared_ptr<Dispatcher> dispatcher() const { return _dispatcher; }
    std::shared_ptr<WidgetFactory> widget_factory() const { return _widget_factory; }
    std::shared_ptr<PluginManager> plugin_manager() const { return _plugin_manager; }
    std::shared_ptr<TreeLike> data_tree() const { return _data_tree; }

    // Check if app should close (for host app to check)
    bool should_close() const { return _should_close; }
    void request_close() { _should_close = true; }

private:
    EmbeddedApp() = default;

    Result<void> _init();
    Result<void> _init_imgui();
    void _shutdown_imgui();

    EmbeddedConfig _config;

    // Separate ImGui context for this instance
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

    uint32_t _width = 800;
    uint32_t _height = 600;
    float _display_pos_x = 0;
    float _display_pos_y = 0;
};

using EmbeddedAppPtr = std::shared_ptr<EmbeddedApp>;

} // namespace ymery
