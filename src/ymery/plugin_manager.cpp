#include "plugin_manager.hpp"
#include "frontend/widget.hpp"
#include "frontend/widget_factory.hpp"
#include "data_bag.hpp"
#include "static_plugins.hpp"
#include "embedded_plugins.hpp"
#include <ytrace/ytrace.hpp>
#include <filesystem>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <yaml-cpp/yaml.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <dirent.h>
#endif

// Platform-specific dynamic loading
#ifdef _WIN32
#include <windows.h>
#define YMERY_PLUGIN_EXT ".dll"
#define YMERY_PATH_SEP ';'
using PluginHandle = HMODULE;
inline PluginHandle ymery_dlopen(const wchar_t* path) {
    return LoadLibraryW(path);
}
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
// Unix-like systems (Linux, macOS, Emscripten)
#include <dlfcn.h>
#ifdef __APPLE__
#define YMERY_PLUGIN_EXT ".dylib"
#elif defined(__EMSCRIPTEN__)
#define YMERY_PLUGIN_EXT ".wasm"
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

// Load meta.yaml file for a plugin
static Dict load_plugin_meta(const std::string& plugin_path) {
    // Replace plugin extension with .meta.yaml
    std::string meta_path = plugin_path;
    auto pos = meta_path.rfind(YMERY_PLUGIN_EXT);
    if (pos != std::string::npos) {
        meta_path = meta_path.substr(0, pos) + ".meta.yaml";
    }

    Dict result;
    if (!fs::exists(meta_path)) {
        ydebug("No meta file found: {}", meta_path);
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
        ydebug("Loaded meta file: {}", meta_path);
    } catch (const std::exception& e) {
        ywarn("Failed to load meta file {}: {}", meta_path, e.what());
    }

    return result;
}

// Plugin function types (from dynamic libraries)
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

// New plugin system: create() returns Plugin* directly
using NewPluginCreateFn = void*(*)();

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
    // Register embedded plugins (linked directly into executable)
    yinfo("PluginManager: registering embedded plugins");

    // Register embedded imgui plugin
    auto imgui_plugin = std::shared_ptr<Plugin>(embedded::create_imgui_plugin());
    std::string imgui_name = imgui_plugin->name();
    _new_plugins[imgui_name] = imgui_plugin;
    yinfo("PluginManager: registered embedded plugin '{}' with widgets: {}",
        imgui_name,
        [&]() {
            std::string widgets;
            for (const auto& w : imgui_plugin->widgets()) {
                if (!widgets.empty()) widgets += ", ";
                widgets += w;
            }
            return widgets;
        }());

    // Register embedded tree-like plugins
    // data-tree
    {
        PluginMeta meta;
        meta.registered_name = "data-tree";
        meta.class_name = "data-tree";
        meta.create_fn = TreeLikeCreateFn([](
            std::shared_ptr<Dispatcher> /*dispatcher*/,
            std::shared_ptr<PluginManager> /*pm*/
        ) -> Result<TreeLikePtr> {
            return embedded::create_data_tree();
        });
        _plugins["tree-like"]["data-tree"] = meta;
        yinfo("PluginManager: registered embedded tree-like plugin 'data-tree'");
    }

    // simple-data-tree
    {
        PluginMeta meta;
        meta.registered_name = "simple-data-tree";
        meta.class_name = "simple-data-tree";
        meta.create_fn = TreeLikeCreateFn([](
            std::shared_ptr<Dispatcher> /*dispatcher*/,
            std::shared_ptr<PluginManager> /*pm*/
        ) -> Result<TreeLikePtr> {
            return embedded::create_simple_data_tree();
        });
        _plugins["tree-like"]["simple-data-tree"] = meta;
        yinfo("PluginManager: registered embedded tree-like plugin 'simple-data-tree'");
    }

    return Ok();
}

Result<void> PluginManager::dispose() {
    for (auto handle : _handles) {
        if (handle) {
            ymery_dlclose(static_cast<PluginHandle>(handle));
        }
    }
    _handles.clear();
    _plugins.clear();
    _new_plugins.clear();
    _discovered_plugins.clear();
    _plugins_discovered = false;
    return Ok();
}

Result<void> PluginManager::_ensure_plugins_discovered() {
    if (_plugins_discovered) {
        return Ok();
    }

    // Discover plugins without loading them (lazy loading)
    ydebug("PluginManager: discovering plugins from {}", _plugins_path);

    // Parse path-separated paths (: on Unix/web, ; on Windows)
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
    char exe_path[MAX_PATH];
    if (GetModuleFileNameA(NULL, exe_path, MAX_PATH) > 0) {
        fs::path exe_dir = fs::path(exe_path).parent_path();
        ydebug("Exe directory: {}", exe_dir.string());
        if (fs::exists(exe_dir / "ymery_lib.dll")) {
            if (SetDllDirectoryA(exe_dir.string().c_str())) {
                ydebug("Set DLL search directory to: {}", exe_dir.string());
            } else {
                ywarn("Failed to set DLL directory: error {}", GetLastError());
            }
        } else {
            ywarn("ymery_lib.dll not found in exe directory: {}", exe_dir.string());
        }
    } else {
        ywarn("Failed to get module filename: error {}", GetLastError());
    }
#endif

    // Scan each directory and record plugin paths (without loading)
    for (const auto& dir : plugin_dirs) {
        ydebug("PluginManager: scanning directory: {}", dir);
        if (!fs::exists(dir)) {
            ywarn("Plugin directory does not exist: {}", dir);
            continue;
        }

#ifdef __EMSCRIPTEN__
        // Emscripten: use POSIX opendir/readdir (more reliable with VFS than std::filesystem)
        DIR* d = opendir(dir.c_str());
        if (!d) {
            ywarn("PluginManager: opendir failed for {}", dir);
            continue;
        }
        struct dirent* entry;
        while ((entry = readdir(d)) != nullptr) {
            std::string name = entry->d_name;
            if (name == "." || name == "..") continue;

            std::string full_path = dir + "/" + name;

            // Check if it's a .wasm file
            if (name.size() > 5 && name.substr(name.size() - 5) == ".wasm") {
                // Extract plugin name from filename (e.g., "im-anim.wasm" -> "im-anim")
                std::string plugin_name = name.substr(0, name.size() - 5);
                _discovered_plugins[plugin_name] = full_path;
                ydebug("PluginManager: discovered plugin: {} -> {}", plugin_name, full_path);
            }
        }
        closedir(d);
#else
        // Native: use recursive_directory_iterator
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == YMERY_PLUGIN_EXT) {
                std::string path = entry.path().string();
                // Extract plugin name from filename (e.g., "im-anim.so" -> "im-anim")
                std::string plugin_name = entry.path().stem().string();
                _discovered_plugins[plugin_name] = path;
                ydebug("PluginManager: discovered plugin: {} -> {}", plugin_name, path);
            }
        }
#endif
    }

    _plugins_discovered = true;
    yinfo("PluginManager: discovered {} plugins (lazy loading enabled)", _discovered_plugins.size());

    return Ok();
}

Result<void> PluginManager::_ensure_plugin_loaded(const std::string& plugin_name) {
    yinfo("_ensure_plugin_loaded called for: {}", plugin_name);

    // Already loaded as frontend plugin?
    if (_new_plugins.find(plugin_name) != _new_plugins.end()) {
        yinfo("Plugin {} already loaded (frontend)", plugin_name);
        return Ok();
    }

    // Already loaded as backend plugin?
    for (const auto& [category, plugins] : _plugins) {
        if (plugins.find(plugin_name) != plugins.end()) {
            yinfo("Plugin {} already loaded (backend)", plugin_name);
            return Ok();
        }
    }

    // Ensure plugins are discovered
    if (auto res = _ensure_plugins_discovered(); !res) {
        return Err<void>("PluginManager: failed to discover plugins", res);
    }

    // Find the plugin path
    auto it = _discovered_plugins.find(plugin_name);
    if (it == _discovered_plugins.end()) {
        return Err<void>("PluginManager: plugin '" + plugin_name + "' not found");
    }

    // Load the plugin
    yinfo("PluginManager: lazy loading plugin: {} from {}", plugin_name, it->second);
    if (auto res = _load_plugin(it->second); !res) {
        return Err<void>("PluginManager: failed to load plugin '" + plugin_name + "'", res);
    }
    yinfo("PluginManager: lazy loading plugin {} COMPLETE", plugin_name);

    return Ok();
}

Result<void> PluginManager::_load_plugin(const std::string& path) {
    yinfo("Loading plugin: {}", path);

    fs::path plugin_path(path);

#ifdef _WIN32
    std::wstring wide_path = plugin_path.wstring();
    PluginHandle handle = ymery_dlopen(wide_path.c_str());
#else
    yinfo("Calling dlopen for: {}", plugin_path.string());
    PluginHandle handle = ymery_dlopen(plugin_path.string().c_str());
    yinfo("dlopen returned: {}", handle ? "success" : "null");
#endif
    if (!handle) {
        std::string error = ymery_dlerror();
        ywarn("dlopen failed for '{}': {}", path, error);
        return Err<void>(std::string("Failed to load plugin '") + path + "': " + error);
    }

    // Check for backend plugin exports (name, type) - these are tree-like/device-manager plugins
    auto name_fn = reinterpret_cast<PluginNameFn>(ymery_dlsym(handle, "name"));
    auto type_fn = reinterpret_cast<PluginTypeFn>(ymery_dlsym(handle, "type"));

    // If no name/type exports, it's a frontend plugin (new-style with Plugin class)
    if (!name_fn || !type_fn) {
        ymery_dlclose(handle);
        return _load_new_plugin(path);
    }

    // Backend plugin
    yinfo("Loading backend plugin, calling name()...");
    std::string plugin_name = name_fn();
    yinfo("name() returned: {}", plugin_name);
    yinfo("Calling type()...");
    std::string plugin_type = type_fn();
    yinfo("type() returned: {}", plugin_type);

    yinfo("Loaded backend plugin: {} (type: {})", plugin_name, plugin_type);

    Dict meta_dict = load_plugin_meta(path);

    PluginMeta meta;
    meta.registered_name = plugin_name;
    meta.class_name = plugin_name;
    meta.meta = meta_dict;

    if (auto cat_it = meta_dict.find("category"); cat_it != meta_dict.end()) {
        if (auto cat = get_as<std::string>(cat_it->second)) {
            meta.category = *cat;
        }
    }

    if (plugin_type == "tree-like") {
        auto raw_fn = reinterpret_cast<PluginTreeCreateFn>(ymery_dlsym(handle, "create"));
        if (!raw_fn) {
            ymery_dlclose(handle);
            return Err<void>("Tree-like plugin has no 'create' function: " + path);
        }
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
        ywarn("Unknown backend plugin type '{}' for {}", plugin_type, plugin_name);
    }

    _handles.push_back(handle);
    yinfo("Backend plugin {} loaded successfully", plugin_name);
    return Ok();
}

Result<void> PluginManager::_load_new_plugin(const std::string& path) {
    yinfo("Loading new-style plugin: {}", path);

    fs::path plugin_path(path);

#ifdef _WIN32
    std::wstring wide_path = plugin_path.wstring();
    PluginHandle handle = ymery_dlopen(wide_path.c_str());
#else
    PluginHandle handle = ymery_dlopen(plugin_path.string().c_str());
#endif
    if (!handle) {
        std::string error = ymery_dlerror();
        return Err<void>("Failed to load plugin '" + path + "': " + error);
    }

    // New-style plugins export only 'create()' that returns Plugin*
    yinfo("Looking for 'create' symbol in: {}", path);
    auto create_fn = reinterpret_cast<NewPluginCreateFn>(ymery_dlsym(handle, "create"));
    if (!create_fn) {
        ymery_dlclose(handle);
        return Err<void>("Plugin has no 'create' function: " + path);
    }
    yinfo("Found 'create' function at: {}", (void*)create_fn);

    // Create the plugin instance
    yinfo("Calling create() for: {}", path);
    void* raw_plugin = create_fn();
    yinfo("create() returned: {}", raw_plugin);
    if (!raw_plugin) {
        ymery_dlclose(handle);
        return Err<void>("Plugin create() returned null: " + path);
    }

    Plugin* plugin = static_cast<Plugin*>(raw_plugin);
    yinfo("Getting plugin name...");
    std::string plugin_name = plugin->name();
    yinfo("Plugin name: {}", plugin_name);

    // Wrap in shared_ptr (transfers ownership)
    auto plugin_ptr = std::shared_ptr<Plugin>(plugin);

    yinfo("Loaded new-style plugin '{}' with widgets: {}", plugin_name,
        [&]() {
            std::string widgets_str;
            for (const auto& w : plugin_ptr->widgets()) {
                if (!widgets_str.empty()) widgets_str += ", ";
                widgets_str += w;
            }
            return widgets_str;
        }());

    _new_plugins[plugin_name] = plugin_ptr;
    _handles.push_back(handle);
    return Ok();
}

// TreeLike interface implementation

Result<std::vector<std::string>> PluginManager::get_children_names(const DataPath& path) {
    if (auto res = _ensure_plugins_discovered(); !res) {
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
    if (auto res = _ensure_plugins_discovered(); !res) {
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
    // Check for new-style plugin.widget format (e.g., "imgui.button")
    auto dot_pos = name.find('.');
    if (dot_pos != std::string::npos) {
        std::string plugin_name = name.substr(0, dot_pos);
        std::string widget_name = name.substr(dot_pos + 1);

        // Lazy load the plugin if not already loaded
        if (auto res = _ensure_plugin_loaded(plugin_name); !res) {
            return Err<WidgetPtr>("PluginManager: failed to load plugin '" + plugin_name + "'", res);
        }

        auto plugin_it = _new_plugins.find(plugin_name);
        if (plugin_it != _new_plugins.end()) {
            yinfo("Creating widget '{}.{}' via plugin", plugin_name, widget_name);
            auto result = plugin_it->second->createWidget(widget_name, widget_factory, dispatcher, ns, data_bag);
            yinfo("Widget creation returned: {}", result.has_value() ? "success" : "error");
            return result;
        }
        return Err<WidgetPtr>("PluginManager: plugin '" + plugin_name + "' not found");
    }

    // Legacy: single widget name lookup - need to discover plugins first
    if (auto res = _ensure_plugins_discovered(); !res) {
        return Err<WidgetPtr>("PluginManager: error discovering plugins", res);
    }

    // Try to lazy load as legacy plugin
    if (auto res = _ensure_plugin_loaded(name); !res) {
        // Ignore error - may not be a loadable plugin
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
    // Lazy load the plugin
    if (auto res = _ensure_plugin_loaded(name); !res) {
        return Err<TreeLikePtr>("PluginManager: failed to load plugin '" + name + "'", res);
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
    // Need to cast away const to call discovery/loading
    auto* self = const_cast<PluginManager*>(this);
    self->_ensure_plugins_discovered();

    // Check for new-style plugin.widget format (e.g., "imgui.button")
    auto dot_pos = name.find('.');
    if (dot_pos != std::string::npos) {
        std::string plugin_name = name.substr(0, dot_pos);
        std::string widget_name = name.substr(dot_pos + 1);

        // Load plugin if discovered but not yet loaded
        if (_discovered_plugins.find(plugin_name) != _discovered_plugins.end()) {
            self->_ensure_plugin_loaded(plugin_name);
        }

        // Check loaded plugins for the specific widget
        auto plugin_it = _new_plugins.find(plugin_name);
        if (plugin_it != _new_plugins.end()) {
            const auto& widgets = plugin_it->second->widgets();
            return std::find(widgets.begin(), widgets.end(), widget_name) != widgets.end();
        }
        return false;
    }

    // Legacy: single widget name lookup
    auto cat_it = _plugins.find("widget");
    if (cat_it != _plugins.end() && cat_it->second.find(name) != cat_it->second.end()) {
        return true;
    }

    // Check discovered plugins
    return _discovered_plugins.find(name) != _discovered_plugins.end();
}

bool PluginManager::has_tree(const std::string& name) const {
    // Need to cast away const to call discovery
    const_cast<PluginManager*>(this)->_ensure_plugins_discovered();

    // Check already loaded plugins
    auto cat_it = _plugins.find("tree-like");
    if (cat_it != _plugins.end() && cat_it->second.find(name) != cat_it->second.end()) {
        return true;
    }
    cat_it = _plugins.find("device-manager");
    if (cat_it != _plugins.end() && cat_it->second.find(name) != cat_it->second.end()) {
        return true;
    }

    // Check discovered plugins
    return _discovered_plugins.find(name) != _discovered_plugins.end();
}

} // namespace ymery
