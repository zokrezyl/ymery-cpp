// app-core.cpp - Platform-independent App initialization
// This file contains the core initialization logic that is shared across all platforms.
// Platform-specific code (graphics, windowing) remains in app.cpp.

#include "app.hpp"
#include <ytrace/ytrace.hpp>

namespace ymery {

Result<void> App::_init_core() {
    ydebug("_init_core starting");

    // Create dispatcher
    ydebug("Creating dispatcher");
    auto disp_res = Dispatcher::create();
    if (!disp_res) {
        return Err<void>("App::_init_core: dispatcher create failed", disp_res);
    }
    _dispatcher = *disp_res;

    // Build colon-separated plugin path string
    std::string plugins_path;
    for (const auto& p : _config.plugin_paths) {
        if (!plugins_path.empty()) plugins_path += ":";
        plugins_path += p.string();
    }

    // Create plugin manager (TreeLike that holds all plugins)
    ydebug("Creating plugin manager with path: {}", plugins_path);
    auto pm_res = PluginManager::create(plugins_path);
    if (!pm_res) {
        return Err<void>("App::_init_core: plugin manager create failed", pm_res);
    }
    _plugin_manager = *pm_res;

    ydebug("Plugin manager created");

    // Load YAML modules
    ydebug("Loading YAML modules, main_module: {}", _config.main_module);
    auto lang_res = Lang::create(_config.layout_paths, _config.main_module);
    if (!lang_res) {
        return Err<void>("App::_init_core: lang create failed", lang_res);
    }
    _lang = *lang_res;
    ydebug("Lang loaded successfully");

    // Get data tree type from app config (default: simple-data-tree)
    std::string tree_type = "simple-data-tree";
    const auto& app_config = _lang->app_config();
    auto tree_it = app_config.find("data-tree");
    if (tree_it != app_config.end()) {
        if (auto t = get_as<std::string>(tree_it->second)) {
            tree_type = *t;
            ydebug("Using data-tree type from config: {}", tree_type);
        }
    }

    // Create data tree from plugin
    ydebug("Creating data tree of type: {}", tree_type);
    auto tree_res = _plugin_manager->create_tree(tree_type, _dispatcher);
    if (!tree_res) {
        ywarn("Could not create {} from plugin: {}", tree_type, error_msg(tree_res));
        // Fallback to a minimal implementation if needed
        return Err<void>("App::_init_core: no tree-like plugin available", tree_res);
    }
    _data_tree = *tree_res;

    // Create widget factory with plugin manager
    auto wf_res = WidgetFactory::create(_lang, _dispatcher, _data_tree, _plugin_manager);
    if (!wf_res) {
        return Err<void>("App::_init_core: widget factory create failed", wf_res);
    }
    _widget_factory = *wf_res;

    // Create root widget
    ydebug("Creating root widget");
    auto root_res = _widget_factory->create_root_widget();
    if (!root_res) {
        ywarn("App::_init_core: root widget create failed: {}", error_msg(root_res));
        return Err<void>("App::_init_core: root widget create failed", root_res);
    }
    _root_widget = *root_res;
    ydebug("Root widget created successfully");

    return Ok();
}

void App::_dispose_core() {
    if (_root_widget) {
        _root_widget->dispose();
        _root_widget.reset();
    }

    if (_plugin_manager) {
        _plugin_manager->dispose();
        _plugin_manager.reset();
    }
}

} // namespace ymery
