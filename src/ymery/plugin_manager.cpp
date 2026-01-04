#include "plugin_manager.hpp"
#include "frontend/widget.hpp"
#include "frontend/widget_factory.hpp"
#include "data_bag.hpp"
#include "static_plugins.hpp"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <sstream>
#include <fstream>
#include <yaml-cpp/yaml.h>

// Platform-specific dynamic loading (not used on web - uses static plugins)
#ifndef YMERY_WEB
#ifdef _WIN32
#include <windows.h>
#define YMERY_PLUGIN_EXT ".dll"
#define YMERY_PATH_SEP ';'
using PluginHandle = HMODULE;
inline PluginHandle ymery_dlopen(const char* path) { return LoadLibraryA(path); }
inline void* ymery_dlsym(PluginHandle h, const char* sym) { return (void*)GetProcAddress(h, sym); }
inline void ymery_dlclose(PluginHandle h) { FreeLibrary(h); }
inline std::string ymery_dlerror() {
    DWORD err = GetLastError();
    if (err == 0) return "";
    char* msg = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                   nullptr, err, 0, (LPSTR)&msg, 0, nullptr);
    std::string result = msg ? msg : "Unknown error";
    LocalFree(msg);
    return result;
}
#else
#include <dlfcn.h>
#ifdef __APPLE__
#define YMERY_PLUGIN_EXT ".dylib"
#else
#define YMERY_PLUGIN_EXT ".so"
#endif
#define YMERY_PATH_SEP ':'
using PluginHandle = void*;
inline PluginHandle ymery_dlopen(const char* path) { return dlopen(path, RTLD_NOW | RTLD_LOCAL); }
inline void* ymery_dlsym(PluginHandle h, const char* sym) { return dlsym(h, sym); }
inline void ymery_dlclose(PluginHandle h) { dlclose(h); }
inline std::string ymery_dlerror() { const char* e = dlerror(); return e ? e : ""; }
#endif
#endif // !YMERY_WEB

namespace fs = std::filesystem;

namespace ymery {

// Helper to convert YAML node to Value
static Value yaml_to_value(const YAML::Node& node) {
    if (node.IsScalar()) {
        // Try to parse as different types
        try {
            if (node.Tag() == "!") return Value(node.as<std::string>());
            // Try bool first
            std::string s = node.as<std::string>();
            if (s == "true" || s == "false") {
                return Value(node.as<bool>());
            }
            // Try int
            try {
                size_t pos;
                int64_t i = std::stoll(s, &pos);
                if (pos == s.size()) return Value(i);
            } catch (...) {}
            // Try double
            try {
                size_t pos;
                double d = std::stod(s, &pos);
                if (pos == s.size()) return Value(d);
            } catch (...) {}
            // Default to string
            return Value(s);
        } catch (...) {
            return Value(std::string());
        }
    }
    if (node.IsSequence()) {
        List list;
        for (const auto& item : node) {
            list.push_back(yaml_to_value(item));
        }
        return Value(list);
    }
    if (node.IsMap()) {
        Dict dict;
        for (const auto& pair : node) {
            std::string key = pair.first.as<std::string>();
            dict[key] = yaml_to_value(pair.second);
        }
        return Value(dict);
    }
    return Value();
}

#ifndef YMERY_WEB
// Load meta.yaml file for a plugin (native only)
static Dict load_plugin_meta(const std::string& plugin_path) {
    // Replace plugin extension with .meta.yaml
    std::string meta_path = plugin_path;
    auto pos = meta_path.rfind(YMERY_PLUGIN_EXT);
    if (pos != std::string::npos) {
        meta_path = meta_path.substr(0, pos) + ".meta.yaml";
    }

    Dict result;
    if (!fs::exists(meta_path)) {
        spdlog::debug("No meta file found: {}", meta_path);
        return result;
    }

    try {
        YAML::Node node = YAML::LoadFile(meta_path);
        if (node.IsMap()) {
            for (const auto& pair : node) {
                std::string key = pair.first.as<std::string>();
                result[key] = yaml_to_value(pair.second);
            }
        }
        spdlog::debug("Loaded meta file: {}", meta_path);
    } catch (const std::exception& e) {
        spdlog::warn("Failed to load meta file {}: {}", meta_path, e.what());
    }

    return result;
}

// Plugin function types (from .so files - native only)
// Returns void* pointing to heap-allocated Result - caller takes ownership
using PluginNameFn = const char*(*)();
using PluginTypeFn = const char*(*)();
using PluginWidgetCreateFn = void*(*)(
    std::shared_ptr<WidgetFactory>,
    std::shared_ptr<Dispatcher>,
    const std::string&,
    std::shared_ptr<DataBag>
);
using PluginTreeCreateFn = void*(*)(std::shared_ptr<Dispatcher>, std::shared_ptr<PluginManager>);
#endif // !YMERY_WEB

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
#ifndef YMERY_WEB
    for (auto handle : _handles) {
        if (handle) {
            ymery_dlclose(static_cast<PluginHandle>(handle));
        }
    }
    _handles.clear();
#endif
    _plugins.clear();
    _plugins_loaded = false;
    return Ok();
}

Result<void> PluginManager::_ensure_plugins_loaded() {
    if (_plugins_loaded) {
        return Ok();
    }

#ifdef YMERY_WEB
    // Web builds use static plugins (no dlopen available)
    spdlog::debug("PluginManager: loading static plugins (web build)");

    const auto& static_plugins = get_static_plugins();
    for (const auto& plugin : static_plugins) {
        std::string plugin_name = plugin.name();
        std::string plugin_type = plugin.type();

        spdlog::debug("Loaded static plugin: {} (type: {})", plugin_name, plugin_type);

        PluginMeta meta;
        meta.registered_name = plugin_name;
        meta.class_name = plugin_name;

        if (plugin_type == "widget") {
            auto raw_fn = reinterpret_cast<WidgetCreateFnPtr>(plugin.create);
            meta.create_fn = WidgetCreateFn([raw_fn](
                std::shared_ptr<WidgetFactory> factory,
                std::shared_ptr<Dispatcher> dispatcher,
                const std::string& name,
                std::shared_ptr<DataBag> bag
            ) -> Result<WidgetPtr> {
                void* ptr = raw_fn(factory, dispatcher, name, bag);
                auto* result = static_cast<Result<WidgetPtr>*>(ptr);
                Result<WidgetPtr> ret = std::move(*result);
                delete result;
                return ret;
            });
            _plugins["widget"][plugin_name] = meta;
        }
        else if (plugin_type == "tree-like") {
            auto raw_fn = reinterpret_cast<TreeLikeCreateFnPtr>(plugin.create);
            meta.create_fn = TreeLikeCreateFn([raw_fn](
                std::shared_ptr<Dispatcher> dispatcher,
                std::shared_ptr<PluginManager> pm
            ) -> Result<TreeLikePtr> {
                void* ptr = raw_fn(dispatcher, pm);
                auto* result = static_cast<Result<TreeLikePtr>*>(ptr);
                Result<TreeLikePtr> ret = std::move(*result);
                delete result;
                return ret;
            });
            _plugins["tree-like"][plugin_name] = meta;
        }
        else if (plugin_type == "device-manager") {
            auto raw_fn = reinterpret_cast<TreeLikeCreateFnPtr>(plugin.create);
            meta.create_fn = TreeLikeCreateFn([raw_fn](
                std::shared_ptr<Dispatcher> dispatcher,
                std::shared_ptr<PluginManager> pm
            ) -> Result<TreeLikePtr> {
                void* ptr = raw_fn(dispatcher, pm);
                auto* result = static_cast<Result<TreeLikePtr>*>(ptr);
                Result<TreeLikePtr> ret = std::move(*result);
                delete result;
                return ret;
            });
            _plugins["device-manager"][plugin_name] = meta;
        }
        else {
            spdlog::warn("Unknown plugin type '{}' for {}", plugin_type, plugin_name);
        }
    }

#else
    // Native builds use dynamic loading
    spdlog::debug("PluginManager: loading plugins from {}", _plugins_path);

    // Parse path-separated paths (: on Unix, ; on Windows)
    std::vector<std::string> plugin_dirs;
    std::stringstream ss(_plugins_path);
    std::string path_str;
    while (std::getline(ss, path_str, YMERY_PATH_SEP)) {
        if (!path_str.empty()) {
            plugin_dirs.push_back(path_str);
        }
    }

#ifdef _WIN32
    // On Windows, add the exe directory to DLL search path so plugins can find ymery_lib.dll
    if (!plugin_dirs.empty()) {
        fs::path first_dir = plugin_dirs[0];
        fs::path exe_dir = first_dir.parent_path();  // plugins dir is typically exe_dir/plugins
        if (fs::exists(exe_dir / "ymery_lib.dll")) {
            SetDllDirectoryA(exe_dir.string().c_str());
            spdlog::debug("Set DLL search directory to: {}", exe_dir.string());
        }
    }
#endif

    // Scan each directory
    for (const auto& dir : plugin_dirs) {
        if (!fs::exists(dir)) {
            spdlog::warn("Plugin directory does not exist: {}", dir);
            continue;
        }

        // Recursively find all plugin files (.so on Unix, .dll on Windows)
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == YMERY_PLUGIN_EXT) {
                std::string path = entry.path().string();
                if (auto res = _load_plugin(path); !res) {
                    spdlog::warn("Failed to load plugin {}: {}", path, error_msg(res));
                }
            }
        }
    }
#endif // YMERY_WEB

    _plugins_loaded = true;
    spdlog::info("PluginManager: loaded plugins - widget: {}, tree-like: {}, device-manager: {}",
        _plugins["widget"].size(), _plugins["tree-like"].size(), _plugins["device-manager"].size());

    return Ok();
}

#ifndef YMERY_WEB
Result<void> PluginManager::_load_plugin(const std::string& path) {
    spdlog::debug("Loading plugin: {}", path);

    PluginHandle handle = ymery_dlopen(path.c_str());
    if (!handle) {
        return Err<void>(std::string("Failed to load plugin: ") + ymery_dlerror());
    }

    auto name_fn = reinterpret_cast<PluginNameFn>(ymery_dlsym(handle, "name"));
    if (!name_fn) {
        ymery_dlclose(handle);
        return Err<void>("Plugin has no 'name' function: " + path);
    }

    auto type_fn = reinterpret_cast<PluginTypeFn>(ymery_dlsym(handle, "type"));
    if (!type_fn) {
        ymery_dlclose(handle);
        return Err<void>("Plugin has no 'type' function: " + path);
    }

    std::string plugin_name = name_fn();
    std::string plugin_type = type_fn();

    spdlog::debug("Loaded plugin: {} (type: {})", plugin_name, plugin_type);

    // Load meta.yaml for this plugin
    Dict meta_dict = load_plugin_meta(path);

    PluginMeta meta;
    meta.registered_name = plugin_name;
    meta.class_name = plugin_name;
    meta.meta = meta_dict;

    // Extract category from meta
    if (auto cat_it = meta_dict.find("category"); cat_it != meta_dict.end()) {
        if (auto cat = get_as<std::string>(cat_it->second)) {
            meta.category = *cat;
        }
    }

    if (plugin_type == "widget") {
        auto raw_fn = reinterpret_cast<PluginWidgetCreateFn>(ymery_dlsym(handle, "create"));
        if (!raw_fn) {
            ymery_dlclose(handle);
            return Err<void>("Widget plugin has no 'create' function: " + path);
        }
        // Wrap raw function to convert void* to Result
        meta.create_fn = WidgetCreateFn([raw_fn](
            std::shared_ptr<WidgetFactory> factory,
            std::shared_ptr<Dispatcher> dispatcher,
            const std::string& name,
            std::shared_ptr<DataBag> bag
        ) -> Result<WidgetPtr> {
            void* ptr = raw_fn(factory, dispatcher, name, bag);
            auto* result = static_cast<Result<WidgetPtr>*>(ptr);
            Result<WidgetPtr> ret = std::move(*result);
            delete result;
            return ret;
        });
        _plugins["widget"][plugin_name] = meta;
    }
    else if (plugin_type == "tree-like") {
        auto raw_fn = reinterpret_cast<PluginTreeCreateFn>(ymery_dlsym(handle, "create"));
        if (!raw_fn) {
            ymery_dlclose(handle);
            return Err<void>("Tree-like plugin has no 'create' function: " + path);
        }
        // Wrap raw function to convert void* to Result
        meta.create_fn = TreeLikeCreateFn([raw_fn](
            std::shared_ptr<Dispatcher> dispatcher,
            std::shared_ptr<PluginManager> pm
        ) -> Result<TreeLikePtr> {
            void* ptr = raw_fn(dispatcher, pm);
            auto* result = static_cast<Result<TreeLikePtr>*>(ptr);
            Result<TreeLikePtr> ret = std::move(*result);
            delete result;
            return ret;
        });
        _plugins["tree-like"][plugin_name] = meta;
    }
    else if (plugin_type == "device-manager") {
        auto raw_fn = reinterpret_cast<PluginTreeCreateFn>(ymery_dlsym(handle, "create"));
        if (!raw_fn) {
            ymery_dlclose(handle);
            return Err<void>("Device-manager plugin has no 'create' function: " + path);
        }
        // Wrap raw function to convert void* to Result
        meta.create_fn = TreeLikeCreateFn([raw_fn](
            std::shared_ptr<Dispatcher> dispatcher,
            std::shared_ptr<PluginManager> pm
        ) -> Result<TreeLikePtr> {
            void* ptr = raw_fn(dispatcher, pm);
            auto* result = static_cast<Result<TreeLikePtr>*>(ptr);
            Result<TreeLikePtr> ret = std::move(*result);
            delete result;
            return ret;
        });
        _plugins["device-manager"][plugin_name] = meta;
    }
    else {
        spdlog::warn("Unknown plugin type '{}' for {}", plugin_type, plugin_name);
    }

    _handles.push_back(handle);
    return Ok();
}
#else // YMERY_WEB
Result<void> PluginManager::_load_plugin(const std::string& path) {
    // Web builds use static plugins, dynamic loading not available
    return Err<void>("Dynamic plugin loading not available in web build");
}
#endif // !YMERY_WEB

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

    if (parts.size() == 2) {
        // Return children of a specific plugin (e.g., /widget/button -> ["meta"])
        std::string category = parts[0];
        std::string name = parts[1];
        auto cat_it = _plugins.find(category);
        if (cat_it == _plugins.end()) {
            return Err<std::vector<std::string>>("PluginManager: category '" + category + "' not found");
        }
        auto plugin_it = cat_it->second.find(name);
        if (plugin_it == cat_it->second.end()) {
            return Err<std::vector<std::string>>("PluginManager: '" + name + "' not found in '" + category + "'");
        }
        // Each plugin has a "meta" child
        return Ok(std::vector<std::string>{"meta"});
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
        meta["category"] = pm.category;
        return Ok(meta);
    }

    if (parts.size() == 3 && parts[2] == "meta") {
        // Return the full meta.yaml contents at /widget/button/meta
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
        return Ok(plugin_it->second.meta);
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

Result<TreeLikePtr> PluginManager::create_tree(const std::string& name, std::shared_ptr<Dispatcher> dispatcher) {
    if (auto res = _ensure_plugins_loaded(); !res) {
        return Err<TreeLikePtr>("PluginManager: error loading plugins", res);
    }

    // Search in both tree-like and device-manager categories
    PluginMeta* plugin_meta = nullptr;

    auto cat_it = _plugins.find("tree-like");
    if (cat_it != _plugins.end()) {
        auto plugin_it = cat_it->second.find(name);
        if (plugin_it != cat_it->second.end()) {
            plugin_meta = &plugin_it->second;
        }
    }

    if (!plugin_meta) {
        cat_it = _plugins.find("device-manager");
        if (cat_it != _plugins.end()) {
            auto plugin_it = cat_it->second.find(name);
            if (plugin_it != cat_it->second.end()) {
                plugin_meta = &plugin_it->second;
            }
        }
    }

    if (!plugin_meta) {
        return Err<TreeLikePtr>("PluginManager: tree/device-manager '" + name + "' not found");
    }

    try {
        auto create_fn = std::any_cast<TreeLikeCreateFn>(plugin_meta->create_fn);
        return create_fn(dispatcher, shared_from_this());
    } catch (const std::bad_any_cast&) {
        return Err<TreeLikePtr>("PluginManager: invalid create function for '" + name + "'");
    }
}

bool PluginManager::has_widget(const std::string& name) const {
    // Need to cast away const to call _ensure_plugins_loaded
    const_cast<PluginManager*>(this)->_ensure_plugins_loaded();
    auto cat_it = _plugins.find("widget");
    if (cat_it == _plugins.end()) return false;
    return cat_it->second.find(name) != cat_it->second.end();
}

bool PluginManager::has_tree(const std::string& name) const {
    // Need to cast away const to call _ensure_plugins_loaded
    const_cast<PluginManager*>(this)->_ensure_plugins_loaded();
    // Check both tree-like and device-manager categories
    auto cat_it = _plugins.find("tree-like");
    if (cat_it != _plugins.end() && cat_it->second.find(name) != cat_it->second.end()) {
        return true;
    }
    cat_it = _plugins.find("device-manager");
    if (cat_it != _plugins.end() && cat_it->second.find(name) != cat_it->second.end()) {
        return true;
    }
    return false;
}

} // namespace ymery
