#include "embedded.hpp"
#include <spdlog/spdlog.h>

namespace ymery {

Result<std::shared_ptr<EmbeddedApp>> EmbeddedApp::create(const EmbeddedConfig& config) {
    auto app = std::shared_ptr<EmbeddedApp>(new EmbeddedApp());
    app->_config = config;

    if (auto res = app->_init_core(); !res) {
        return Err<std::shared_ptr<EmbeddedApp>>("EmbeddedApp::create: init_core failed", res);
    }

    return app;
}

Result<void> EmbeddedApp::_init_core() {
    spdlog::debug("EmbeddedApp::_init_core starting");

    // Create dispatcher
    auto disp_res = Dispatcher::create();
    if (!disp_res) {
        return Err<void>("EmbeddedApp::_init_core: dispatcher create failed", disp_res);
    }
    _dispatcher = *disp_res;

    // Build colon-separated plugin path string
    std::string plugins_path;
    for (const auto& p : _config.plugin_paths) {
        if (!plugins_path.empty()) plugins_path += ":";
        plugins_path += p.string();
    }

    // Create plugin manager
    auto pm_res = PluginManager::create(plugins_path);
    if (!pm_res) {
        return Err<void>("EmbeddedApp::_init_core: plugin manager create failed", pm_res);
    }
    _plugin_manager = *pm_res;

    // Load YAML modules
    auto lang_res = Lang::create(_config.layout_paths, _config.main_module);
    if (!lang_res) {
        return Err<void>("EmbeddedApp::_init_core: lang create failed", lang_res);
    }
    _lang = *lang_res;

    // Get data tree type from app config (default: simple-data-tree)
    std::string tree_type = "simple-data-tree";
    const auto& app_config = _lang->app_config();
    auto tree_it = app_config.find("data-tree");
    if (tree_it != app_config.end()) {
        if (auto t = get_as<std::string>(tree_it->second)) {
            tree_type = *t;
            spdlog::debug("Using data-tree type from config: {}", tree_type);
        }
    }

    // Create data tree from plugin
    auto tree_res = _plugin_manager->create_tree(tree_type, _dispatcher);
    if (!tree_res) {
        spdlog::warn("Could not create {} from plugin: {}", tree_type, error_msg(tree_res));
        return Err<void>("EmbeddedApp::_init_core: no tree-like plugin available", tree_res);
    }
    _data_tree = *tree_res;

    // Create widget factory with plugin manager
    auto wf_res = WidgetFactory::create(_lang, _dispatcher, _data_tree, _plugin_manager);
    if (!wf_res) {
        return Err<void>("EmbeddedApp::_init_core: widget factory create failed", wf_res);
    }
    _widget_factory = *wf_res;

    // Create root widget
    spdlog::debug("Creating root widget");
    auto root_res = _widget_factory->create_root_widget();
    if (!root_res) {
        spdlog::error("EmbeddedApp::_init_core: root widget create failed: {}", error_msg(root_res));
        return Err<void>("EmbeddedApp::_init_core: root widget create failed", root_res);
    }
    _root_widget = *root_res;
    spdlog::debug("Root widget created successfully");

    return Ok();
}

void EmbeddedApp::_dispose_core() {
    spdlog::debug("EmbeddedApp::_dispose_core");

    // Dispose in correct order:
    // 1. Root widget first (it holds refs to factory, dispatcher, etc.)
    if (_root_widget) {
        _root_widget->dispose();
        _root_widget.reset();
    }

    // 2. Widget factory (holds refs to plugin_manager, data_tree, etc.)
    if (_widget_factory) {
        _widget_factory->dispose();
        _widget_factory.reset();
    }

    // 3. Data tree
    _data_tree.reset();

    // 4. Plugin manager last (plugins must be unloaded after all widgets gone)
    if (_plugin_manager) {
        _plugin_manager->dispose();
        _plugin_manager.reset();
    }

    // 5. Other components
    _dispatcher.reset();
    _lang.reset();
}

void EmbeddedApp::dispose() {
    _dispose_core();
}

void EmbeddedApp::render_widgets() {
    if (_root_widget) {
        if (auto render_res = _root_widget->render(); !render_res) {
            spdlog::warn("EmbeddedApp::render_widgets: {}", error_msg(render_res));
        }
    }
}

} // namespace ymery
