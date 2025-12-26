#pragma once

#include "result.hpp"
#include "types.hpp"
#include "lang.hpp"
#include "dispatcher.hpp"
#include "plugin_manager.hpp"
#include "frontend/widget.hpp"
#include "frontend/widget_factory.hpp"
#include <filesystem>
#include <memory>

#ifdef YMERY_ANDROID
struct android_app;
#endif

namespace ymery {

// App configuration
struct AppConfig {
    std::vector<std::filesystem::path> layout_paths;
    std::vector<std::filesystem::path> plugin_paths;
    std::string main_module = "app";
    int window_width = 1280;
    int window_height = 720;
    std::string window_title = "Ymery App";
};

// App - main application class
class App {
public:
    // Factory method
    static Result<std::shared_ptr<App>> create(const AppConfig& config);

#ifdef YMERY_ANDROID
    // Android factory method
    static Result<std::shared_ptr<App>> create(struct android_app* android_app, const AppConfig& config);
#endif

    // Main loop
    Result<void> run();

    // Lifecycle
    Result<void> init();
    Result<void> dispose();

    // Single frame
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
    App() = default;

    // Initialize graphics backend
    Result<void> _init_graphics();
    Result<void> _shutdown_graphics();

    // Frame begin/end
    Result<void> _begin_frame();
    Result<void> _end_frame();

    // Platform-independent core initialization (implemented in app-core.cpp)
    Result<void> _init_core();
    void _dispose_core();

    AppConfig _config;

    std::shared_ptr<Lang> _lang;
    std::shared_ptr<Dispatcher> _dispatcher;
    std::shared_ptr<WidgetFactory> _widget_factory;
    std::shared_ptr<PluginManager> _plugin_manager;
    std::shared_ptr<TreeLike> _data_tree;

    WidgetPtr _root_widget;
    bool _should_close = false;

    // Graphics state (platform-specific)
    void* _window = nullptr;
    void* _gl_context = nullptr;

#ifdef YMERY_ANDROID
    struct android_app* _android_app = nullptr;
    void* _egl_display = nullptr;
    void* _egl_surface = nullptr;
    void* _egl_context = nullptr;
    int32_t _display_width = 0;
    int32_t _display_height = 0;
    float _display_density = 1.0f;
#endif
};

using AppPtr = std::shared_ptr<App>;

} // namespace ymery
