#pragma once

// ymery embedded mode - minimal wrapper that calls _init_core()
// All ImGui/graphics initialization is done by the host (yetty)

#include "result.hpp"
#include "types.hpp"
#include "lang.hpp"
#include "dispatcher.hpp"
#include "plugin_manager.hpp"
#include "frontend/widget.hpp"
#include "frontend/widget_factory.hpp"
#include <filesystem>
#include <memory>

namespace ymery {

struct EmbeddedConfig {
    std::vector<std::filesystem::path> layout_paths;
    std::vector<std::filesystem::path> plugin_paths;
    std::string main_module = "app";
};

// Minimal embedded app - just wraps core initialization
// Host is responsible for ImGui context, rendering, input
class EmbeddedApp {
public:
    static Result<std::shared_ptr<EmbeddedApp>> create(const EmbeddedConfig& config);

    void dispose();

    // Render the widget tree (host must have called ImGui::NewFrame)
    void render_widgets();

    // Access components
    std::shared_ptr<Lang> lang() const { return _lang; }
    std::shared_ptr<Dispatcher> dispatcher() const { return _dispatcher; }
    std::shared_ptr<WidgetFactory> widget_factory() const { return _widget_factory; }
    std::shared_ptr<PluginManager> plugin_manager() const { return _plugin_manager; }
    std::shared_ptr<TreeLike> data_tree() const { return _data_tree; }
    WidgetPtr root_widget() const { return _root_widget; }

    bool should_close() const { return _should_close; }
    void request_close() { _should_close = true; }

private:
    EmbeddedApp() = default;

    Result<void> _init_core();
    void _dispose_core();

    EmbeddedConfig _config;

    std::shared_ptr<Lang> _lang;
    std::shared_ptr<Dispatcher> _dispatcher;
    std::shared_ptr<WidgetFactory> _widget_factory;
    std::shared_ptr<PluginManager> _plugin_manager;
    std::shared_ptr<TreeLike> _data_tree;

    WidgetPtr _root_widget;
    bool _should_close = false;
};

using EmbeddedAppPtr = std::shared_ptr<EmbeddedApp>;

} // namespace ymery
