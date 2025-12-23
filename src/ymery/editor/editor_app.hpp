#pragma once

#include "layout_model.hpp"
#include "widget_tree.hpp"
#include "editor_canvas.hpp"
#include "../plugin_manager.hpp"
#include <memory>
#include <string>

#ifdef YMERY_ANDROID
struct android_app;
#else
struct GLFWwindow;
#endif

namespace ymery::editor {

struct EditorConfig {
    int window_width = 1280;
    int window_height = 720;
    std::string window_title = "Ymery Widget Editor";
    std::string plugins_path;  // Path to plugins directory
};

class EditorApp {
public:
    static std::unique_ptr<EditorApp> create(const EditorConfig& config);

#ifdef YMERY_ANDROID
    static std::unique_ptr<EditorApp> create(struct android_app* android_app, const EditorConfig& config);
#endif

    ~EditorApp();

    void run();
    void frame();  // Single frame - can be called from Android main loop
    void dispose();
    bool should_close() const { return _should_close; }

private:
    EditorApp() = default;

    bool init(const EditorConfig& config);

    void begin_frame();
    void end_frame();

    void render_menu_bar();
    void render_dockspace();
    void render_preview();
    void render_preview_node(LayoutNode* node);

    // Graphics
    bool _should_close = false;

#ifdef YMERY_ANDROID
    struct android_app* _android_app = nullptr;
    void* _egl_display = nullptr;
    void* _egl_surface = nullptr;
    void* _egl_context = nullptr;
    int32_t _display_width = 0;
    int32_t _display_height = 0;
    float _display_density = 1.0f;
#else
    GLFWwindow* _window = nullptr;
#ifdef YMERY_USE_WEBGPU
    // WebGPU context
    void* _wgpu_instance = nullptr;
    void* _wgpu_adapter = nullptr;
    void* _wgpu_device = nullptr;
    void* _wgpu_queue = nullptr;
    void* _wgpu_surface = nullptr;
#endif
#endif

    // Editor state
    LayoutModel _model;
    std::unique_ptr<WidgetTree> _widget_tree;
    std::unique_ptr<EditorCanvas> _canvas;
    ymery::PluginManagerPtr _plugin_manager;

    // File state
    std::string _current_file;
    bool _modified = false;
};

} // namespace ymery::editor
