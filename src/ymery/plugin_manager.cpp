#include "plugin_manager.hpp"
#include "frontend/widget.hpp"
#include "frontend/widget_factory.hpp"
#include "data_bag.hpp"
#include <spdlog/spdlog.h>
#include <dlfcn.h>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

namespace ymery {

// Plugin function types (from .so files)
using PluginNameFn = const char*(*)();
using PluginTypeFn = const char*(*)();
using PluginWidgetCreateFn = Result<WidgetPtr>(*)(
    std::shared_ptr<WidgetFactory>,
    std::shared_ptr<Dispatcher>,
    const std::string&,
    std::shared_ptr<DataBag>
);
using PluginTreeCreateFn = Result<TreeLikePtr>(*)();

Result<std::shared_ptr<PluginManager>> PluginManager::create(const std::string& plugins_path) {
    auto manager = std::shared_ptr<PluginManager>(new PluginManager());
    manager->_plugins_path = plugins_path;

    if (auto res = manager->init(); !res) {
        return Err<std::shared_ptr<PluginManager>>("PluginManager::create: init failed", res);
    }

    return manager;
}

PluginManager::~PluginManager() {
    dispose();
}

Result<void> PluginManager::init() {
    return Ok();
}

Result<void> PluginManager::dispose() {
    for (auto handle : _handles) {
        if (handle) {
            dlclose(handle);
        }
    }
    _handles.clear();
    _plugins.clear();
    _plugins_loaded = false;
    return Ok();
}

Result<void> PluginManager::_ensure_plugins_loaded() {
    if (_plugins_loaded) {
        return Ok();
    }

    spdlog::info("PluginManager: loading plugins from {}", _plugins_path);

    // Parse colon-separated paths
    std::vector<std::string> plugin_dirs;
    std::stringstream ss(_plugins_path);
    std::string path_str;
    while (std::getline(ss, path_str, ':')) {
        if (!path_str.empty()) {
            plugin_dirs.push_back(path_str);
        }
    }

    // Scan each directory
    for (const auto& dir : plugin_dirs) {
        if (!fs::exists(dir)) {
            spdlog::warn("Plugin directory does not exist: {}", dir);
            continue;
        }

        // Recursively find all .so files
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".so") {
                std::string path = entry.path().string();
                if (auto res = _load_plugin(path); !res) {
                    spdlog::warn("Failed to load plugin {}: {}", path, error_msg(res));
                }
            }
        }
    }

    _plugins_loaded = true;
    spdlog::info("PluginManager: loaded plugins - widget: {}, tree-like: {}",
        _plugins["widget"].size(), _plugins["tree-like"].size());

    return Ok();
}

Result<void> PluginManager::_load_plugin(const std::string& path) {
    spdlog::debug("Loading plugin: {}", path);

    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        return Err<void>(std::string("dlopen failed: ") + dlerror());
    }

    auto name_fn = reinterpret_cast<PluginNameFn>(dlsym(handle, "name"));
    if (!name_fn) {
        dlclose(handle);
        return Err<void>("Plugin has no 'name' function: " + path);
    }

    auto type_fn = reinterpret_cast<PluginTypeFn>(dlsym(handle, "type"));
    if (!type_fn) {
        dlclose(handle);
        return Err<void>("Plugin has no 'type' function: " + path);
    }

    std::string plugin_name = name_fn();
    std::string plugin_type = type_fn();

    spdlog::info("Loaded plugin: {} (type: {})", plugin_name, plugin_type);

    PluginMeta meta;
    meta.registered_name = plugin_name;
    meta.class_name = plugin_name;  // Could be different if we add a class_name() export

    if (plugin_type == "widget") {
        auto create_fn = reinterpret_cast<PluginWidgetCreateFn>(dlsym(handle, "create"));
        if (!create_fn) {
            dlclose(handle);
            return Err<void>("Widget plugin has no 'create' function: " + path);
        }
        meta.create_fn = WidgetCreateFn(create_fn);
        _plugins["widget"][plugin_name] = meta;
    }
    else if (plugin_type == "tree") {
        auto create_fn = reinterpret_cast<PluginTreeCreateFn>(dlsym(handle, "create"));
        if (!create_fn) {
            dlclose(handle);
            return Err<void>("Tree plugin has no 'create' function: " + path);
        }
        meta.create_fn = TreeLikeCreateFn(create_fn);
        _plugins["tree-like"][plugin_name] = meta;
    }
    else {
        spdlog::warn("Unknown plugin type '{}' for {}", plugin_type, plugin_name);
    }

    _handles.push_back(handle);
    return Ok();
}

// TreeLike interface implementation

Result<std::vector<std::string>> PluginManager::get_children_names(const DataPath& path) {
    if (auto res = _ensure_plugins_loaded(); !res) {
        return Err<std::vector<std::string>>("PluginManager: error loading plugins", res);
    }

    const auto& parts = path.as_list();

    if (path.is_root()) {
        // Return category names
        std::vector<std::string> names;
        for (const auto& [category, _] : _plugins) {
            names.push_back(category);
        }
        return Ok(names);
    }

    if (parts.size() == 1) {
        // Return plugin names in category
        std::string category = parts[0];
        auto it = _plugins.find(category);
        if (it == _plugins.end()) {
            return Err<std::vector<std::string>>("PluginManager: category '" + category + "' not found");
        }
        std::vector<std::string> names;
        for (const auto& [name, _] : it->second) {
            names.push_back(name);
        }
        return Ok(names);
    }

    return Err<std::vector<std::string>>("PluginManager: path too deep: " + path.to_string());
}

Result<Dict> PluginManager::get_metadata(const DataPath& path) {
    if (auto res = _ensure_plugins_loaded(); !res) {
        return Err<Dict>("PluginManager: error loading plugins", res);
    }

    const auto& parts = path.as_list();

    if (path.is_root()) {
        Dict meta;
        meta["name"] = std::string("plugins");
        return Ok(meta);
    }

    if (parts.size() == 1) {
        std::string category = parts[0];
        if (_plugins.find(category) == _plugins.end()) {
            return Err<Dict>("PluginManager: category '" + category + "' not found");
        }
        Dict meta;
        meta["name"] = category;
        return Ok(meta);
    }

    if (parts.size() == 2) {
        std::string category = parts[0];
        std::string name = parts[1];
        auto cat_it = _plugins.find(category);
        if (cat_it == _plugins.end()) {
            return Err<Dict>("PluginManager: category '" + category + "' not found");
        }
        auto plugin_it = cat_it->second.find(name);
        if (plugin_it == cat_it->second.end()) {
            return Err<Dict>("PluginManager: '" + name + "' not found in '" + category + "'");
        }
        const PluginMeta& pm = plugin_it->second;
        Dict meta;
        meta["class-name"] = pm.class_name;
        meta["registered-name"] = pm.registered_name;
        // Note: can't store create_fn in Dict directly, use create_widget/create_tree methods
        return Ok(meta);
    }

    return Err<Dict>("PluginManager: path too deep: " + path.to_string());
}

Result<std::vector<std::string>> PluginManager::get_metadata_keys(const DataPath& path) {
    auto res = get_metadata(path);
    if (!res) {
        return Err<std::vector<std::string>>("PluginManager: failed to get metadata", res);
    }
    std::vector<std::string> keys;
    for (const auto& [k, _] : *res) {
        keys.push_back(k);
    }
    return Ok(keys);
}

Result<Value> PluginManager::get(const DataPath& path) {
    // Get metadata value - last component is the key
    DataPath node_path = path.dirname();
    std::string key = path.filename();

    auto res = get_metadata(node_path);
    if (!res) {
        return Err<Value>("PluginManager: failed to get metadata for " + node_path.to_string(), res);
    }

    auto it = res->find(key);
    if (it != res->end()) {
        return Ok(it->second);
    }
    return Err<Value>("PluginManager: key '" + key + "' not found at " + node_path.to_string());
}

Result<void> PluginManager::set(const DataPath& path, const Value& value) {
    return Err<void>("PluginManager: set not implemented");
}

Result<void> PluginManager::add_child(const DataPath& path, const std::string& name, const Dict& data) {
    return Err<void>("PluginManager: add_child not implemented");
}

Result<std::string> PluginManager::as_tree(const DataPath& path, int depth) {
    return Ok(path.to_string());
}

// Convenience methods

Result<WidgetPtr> PluginManager::create_widget(
    const std::string& name,
    std::shared_ptr<WidgetFactory> widget_factory,
    std::shared_ptr<Dispatcher> dispatcher,
    const std::string& ns,
    std::shared_ptr<DataBag> data_bag
) {
    if (auto res = _ensure_plugins_loaded(); !res) {
        return Err<WidgetPtr>("PluginManager: error loading plugins", res);
    }

    auto cat_it = _plugins.find("widget");
    if (cat_it == _plugins.end()) {
        return Err<WidgetPtr>("PluginManager: no widgets loaded");
    }

    auto plugin_it = cat_it->second.find(name);
    if (plugin_it == cat_it->second.end()) {
        return Err<WidgetPtr>("PluginManager: widget '" + name + "' not found");
    }

    try {
        auto create_fn = std::any_cast<WidgetCreateFn>(plugin_it->second.create_fn);
        return create_fn(widget_factory, dispatcher, ns, data_bag);
    } catch (const std::bad_any_cast&) {
        return Err<WidgetPtr>("PluginManager: invalid create function for widget '" + name + "'");
    }
}

Result<TreeLikePtr> PluginManager::create_tree(const std::string& name) {
    if (auto res = _ensure_plugins_loaded(); !res) {
        return Err<TreeLikePtr>("PluginManager: error loading plugins", res);
    }

    auto cat_it = _plugins.find("tree-like");
    if (cat_it == _plugins.end()) {
        return Err<TreeLikePtr>("PluginManager: no tree-like plugins loaded");
    }

    auto plugin_it = cat_it->second.find(name);
    if (plugin_it == cat_it->second.end()) {
        return Err<TreeLikePtr>("PluginManager: tree-like '" + name + "' not found");
    }

    try {
        auto create_fn = std::any_cast<TreeLikeCreateFn>(plugin_it->second.create_fn);
        return create_fn();
    } catch (const std::bad_any_cast&) {
        return Err<TreeLikePtr>("PluginManager: invalid create function for tree-like '" + name + "'");
    }
}

bool PluginManager::has_widget(const std::string& name) const {
    auto cat_it = _plugins.find("widget");
    if (cat_it == _plugins.end()) return false;
    return cat_it->second.find(name) != cat_it->second.end();
}

bool PluginManager::has_tree(const std::string& name) const {
    auto cat_it = _plugins.find("tree-like");
    if (cat_it == _plugins.end()) return false;
    return cat_it->second.find(name) != cat_it->second.end();
}

} // namespace ymery
