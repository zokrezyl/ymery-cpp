// Kernel backend plugin - central manager for providers, settings, and windows
#include "../../types.hpp"
#include "../../result.hpp"
#include "../../plugin_manager.hpp"
#include "../../dispatcher.hpp"
#include <map>
#include <vector>
#include <string>
#include <memory>
#include <ytrace/ytrace.hpp>

namespace ymery::plugins {

// Forward declarations
class Kernel;

/**
 * ProvidersProxy - delegates to device-manager providers lazily
 */
class ProvidersProxy {
public:
    ProvidersProxy(Kernel* kernel) : _kernel(kernel) {}

    Result<std::vector<std::string>> get_children_names(const DataPath& path);
    Result<Dict> get_metadata(const DataPath& path);
    Result<std::vector<std::string>> get_metadata_keys(const DataPath& path);
    Result<Value> get(const DataPath& path);
    Result<void> open(const DataPath& path, const Dict& params);

private:
    struct ProviderAndPath {
        TreeLikePtr provider;
        DataPath remaining;
    };

    Result<ProviderAndPath> _get_provider_and_path(const DataPath& path);

    Kernel* _kernel;
};

/**
 * SettingsManager - stub for settings tree
 */
class SettingsManager : public TreeLike {
public:
    static Result<std::shared_ptr<SettingsManager>> create() {
        return std::make_shared<SettingsManager>();
    }

    Result<std::vector<std::string>> get_children_names(const DataPath& path) override {
        return Ok(std::vector<std::string>{});
    }

    Result<Dict> get_metadata(const DataPath& path) override {
        return Ok(Dict{
            {"name", Value("settings")},
            {"label", Value("Settings")},
            {"type", Value("category")}
        });
    }

    Result<std::vector<std::string>> get_metadata_keys(const DataPath& path) override {
        auto res = get_metadata(path);
        if (!res) return Err<std::vector<std::string>>("get_metadata_keys failed", res);
        std::vector<std::string> keys;
        for (const auto& [k, _] : *res) keys.push_back(k);
        return Ok(keys);
    }

    Result<Value> get(const DataPath& path) override {
        auto parent = path.dirname();
        auto key = path.filename();
        auto meta_res = get_metadata(parent);
        if (!meta_res) return Err<Value>("get failed", meta_res);
        auto it = meta_res->find(key);
        if (it != meta_res->end()) return Ok(it->second);
        return Ok(Value{});
    }

    Result<void> set(const DataPath& path, const Value& value) override {
        return Err<void>("SettingsManager: set not implemented");
    }

    Result<void> add_child(const DataPath& path, const std::string& name, const Dict& data) override {
        return Err<void>("SettingsManager: add_child not implemented");
    }

    Result<std::string> as_tree(const DataPath& path, int depth) override {
        return Ok(path.to_string());
    }
};

/**
 * RegisteredObjectsManager - stub for windows/registered objects tree
 */
class RegisteredObjectsManager : public TreeLike {
public:
    static Result<std::shared_ptr<RegisteredObjectsManager>> create() {
        return std::make_shared<RegisteredObjectsManager>();
    }

    Result<std::vector<std::string>> get_children_names(const DataPath& path) override {
        return Ok(std::vector<std::string>{});
    }

    Result<Dict> get_metadata(const DataPath& path) override {
        return Ok(Dict{
            {"name", Value("windows")},
            {"label", Value("Windows")},
            {"type", Value("category")}
        });
    }

    Result<std::vector<std::string>> get_metadata_keys(const DataPath& path) override {
        auto res = get_metadata(path);
        if (!res) return Err<std::vector<std::string>>("get_metadata_keys failed", res);
        std::vector<std::string> keys;
        for (const auto& [k, _] : *res) keys.push_back(k);
        return Ok(keys);
    }

    Result<Value> get(const DataPath& path) override {
        auto parent = path.dirname();
        auto key = path.filename();
        auto meta_res = get_metadata(parent);
        if (!meta_res) return Err<Value>("get failed", meta_res);
        auto it = meta_res->find(key);
        if (it != meta_res->end()) return Ok(it->second);
        return Ok(Value{});
    }

    Result<void> set(const DataPath& path, const Value& value) override {
        return Err<void>("RegisteredObjectsManager: set not implemented");
    }

    Result<void> add_child(const DataPath& path, const std::string& name, const Dict& data) override {
        return Err<void>("RegisteredObjectsManager: add_child not implemented");
    }

    Result<std::string> as_tree(const DataPath& path, int depth) override {
        return Ok(path.to_string());
    }
};

/**
 * Kernel - central backend manager
 *
 * Asset tree structure:
 * /
 * ├─ /providers (device-manager providers like alsa, jack, waveform)
 * ├─ /settings (configuration)
 * └─ /windows (registered UI objects)
 */
class Kernel : public TreeLike {
public:
    static Result<TreeLikePtr> create(std::shared_ptr<Dispatcher> dispatcher, std::shared_ptr<PluginManager> plugin_manager) {
        auto kernel = std::make_shared<Kernel>(dispatcher, plugin_manager);
        if (auto res = kernel->init(); !res) {
            return Err<TreeLikePtr>("Kernel::create failed", res);
        }
        return kernel;
    }

    Kernel(std::shared_ptr<Dispatcher> dispatcher, std::shared_ptr<PluginManager> plugin_manager)
        : _dispatcher(dispatcher), _plugin_manager(plugin_manager) {}

    Result<void> init() override {
        // Create sub-managers
        _providers_proxy = std::make_unique<ProvidersProxy>(this);

        auto settings_res = SettingsManager::create();
        if (!settings_res) return Err<void>("Kernel: failed to create SettingsManager", settings_res);
        _settings_manager = *settings_res;

        auto windows_res = RegisteredObjectsManager::create();
        if (!windows_res) return Err<void>("Kernel: failed to create RegisteredObjectsManager", windows_res);
        _windows_manager = *windows_res;

        ydebug("Kernel: initialized (dispatcher={}, plugin_manager={})",
            _dispatcher ? "yes" : "no", _plugin_manager ? "yes" : "no");

        // Log available providers at startup
        auto providers = get_available_providers();
        ydebug("Kernel: available providers: {}", providers.size());
        for (const auto& p : providers) {
            ydebug("Kernel:   - {}", p);
        }

        return Ok();
    }

    Result<void> dispose() override {
        // Dispose all cached providers
        for (auto& [name, provider] : _providers) {
            provider->dispose();
        }
        _providers.clear();
        return Ok();
    }

    // ========== TreeLike Interface ==========

    Result<std::vector<std::string>> get_children_names(const DataPath& path) override {
        std::string path_str = path.to_string();
        ydebug("Kernel::get_children_names: path='{}'", path_str);

        // Root
        if (path_str == "/" || path_str.empty()) {
            ydebug("Kernel::get_children_names: returning root children");
            return Ok(std::vector<std::string>{"providers", "settings", "windows"});
        }

        auto parts = path.as_list();
        if (parts.empty()) return Ok(std::vector<std::string>{});

        std::string branch = parts[0];
        DataPath remaining(std::vector<std::string>(parts.begin() + 1, parts.end()));
        ydebug("Kernel::get_children_names: branch='{}', remaining='{}'", branch, remaining.to_string());

        if (branch == "providers") {
            return _providers_proxy->get_children_names(remaining);
        } else if (branch == "settings") {
            return _settings_manager->get_children_names(remaining);
        } else if (branch == "windows") {
            return _windows_manager->get_children_names(remaining);
        }

        return Ok(std::vector<std::string>{});
    }

    Result<Dict> get_metadata(const DataPath& path) override {
        std::string path_str = path.to_string();

        // Root
        if (path_str == "/" || path_str.empty()) {
            return Ok(Dict{
                {"name", Value("kernel")},
                {"label", Value("Kernel")},
                {"type", Value("kernel")},
                {"category", Value("kernel")}
            });
        }

        auto parts = path.as_list();
        if (parts.empty()) return Ok(Dict{});

        std::string branch = parts[0];
        DataPath remaining(std::vector<std::string>(parts.begin() + 1, parts.end()));

        // Branch metadata
        if (parts.size() == 1) {
            if (branch == "providers") {
                return Ok(Dict{
                    {"name", Value("providers")},
                    {"label", Value("Providers")},
                    {"type", Value("folder")},
                    {"category", Value("folder")}
                });
            } else if (branch == "settings") {
                return _settings_manager->get_metadata(DataPath("/"));
            } else if (branch == "windows") {
                return _windows_manager->get_metadata(DataPath("/"));
            }
        }

        // Delegate to sub-managers
        if (branch == "providers") {
            return _providers_proxy->get_metadata(remaining);
        } else if (branch == "settings") {
            return _settings_manager->get_metadata(remaining);
        } else if (branch == "windows") {
            return _windows_manager->get_metadata(remaining);
        }

        return Ok(Dict{});
    }

    Result<std::vector<std::string>> get_metadata_keys(const DataPath& path) override {
        auto res = get_metadata(path);
        if (!res) return Err<std::vector<std::string>>("get_metadata_keys failed", res);
        std::vector<std::string> keys;
        for (const auto& [k, _] : *res) keys.push_back(k);
        return Ok(keys);
    }

    Result<Value> get(const DataPath& path) override {
        std::string path_str = path.to_string();

        // Root - return empty
        if (path_str == "/" || path_str.empty()) {
            return Ok(Value{});
        }

        auto parts = path.as_list();
        if (parts.empty()) {
            return Ok(Value{});
        }

        std::string branch = parts[0];
        DataPath remaining(std::vector<std::string>(parts.begin() + 1, parts.end()));

        // Delegate to sub-managers
        if (branch == "providers") {
            ydebug("Kernel::get: providers path='{}', remaining='{}'", path_str, remaining.to_string());
            return _providers_proxy->get(remaining);
        } else if (branch == "settings") {
            return _settings_manager->get(remaining);
        } else if (branch == "windows") {
            return _windows_manager->get(remaining);
        }

        return Ok(Value{});
    }

    Result<void> set(const DataPath& path, const Value& value) override {
        // For root-level metadata (e.g., /selection)
        auto parts = path.as_list();
        if (parts.size() == 1) {
            _root_metadata[parts[0]] = value;
            return Ok();
        }
        return Err<void>("Kernel: set: only root-level metadata supported");
    }

    Result<void> add_child(const DataPath& path, const std::string& name, const Dict& data) override {
        return Err<void>("Kernel: add_child not implemented");
    }

    Result<std::string> as_tree(const DataPath& path, int depth) override {
        return Ok(path.to_string());
    }

    // ========== DeviceManager-like Interface ==========

    Result<Value> open(const DataPath& path, const Dict& params) {
        auto parts = path.as_list();
        if (parts.empty()) {
            return Err<Value>("Kernel::open: empty path");
        }

        std::string branch = parts[0];
        DataPath remaining(std::vector<std::string>(parts.begin() + 1, parts.end()));

        if (branch == "providers") {
            auto res = _providers_proxy->open(remaining, params);
            if (!res) {
                // Dispatch error event
                if (_dispatcher) {
                    Dict event{
                        {"name", Value("open")},
                        {"source", Value("kernel")},
                        {"path", Value(path.to_string())},
                        {"error", Value(error_msg(res))}
                    };
                    // _dispatcher->dispatch_event(event);
                }
                return Err<Value>("Kernel::open failed", res);
            }

            // Dispatch success event
            if (_dispatcher) {
                Dict event{
                    {"name", Value("open")},
                    {"source", Value("kernel")},
                    {"path", Value(path.to_string())},
                    {"status", Value("success")}
                };
                // _dispatcher->dispatch_event(event);
            }

            return Ok(Value{});
        }

        return Err<Value>("Kernel::open: unsupported branch '" + branch + "'");
    }

    Result<void> configure(const DataPath& path, const Dict& params) {
        return Err<void>("Kernel::configure not implemented");
    }

    Result<void> close(const DataPath& path) {
        return Err<void>("Kernel::close not implemented");
    }

    // ========== Provider Access ==========

    Result<TreeLikePtr> get_provider(const std::string& provider_name) {
        // Check cache
        auto it = _providers.find(provider_name);
        if (it != _providers.end()) {
            return Ok(it->second);
        }

        // Need plugin manager to load providers
        if (!_plugin_manager) {
            return Err<TreeLikePtr>("Kernel: no plugin manager set");
        }

        // Get provider from plugin manager
        auto tree_res = _plugin_manager->create_tree(provider_name, _dispatcher);
        if (!tree_res) {
            return Err<TreeLikePtr>("Kernel: failed to create provider '" + provider_name + "'", tree_res);
        }

        _providers[provider_name] = *tree_res;
        ydebug("Kernel: loaded provider '{}'", provider_name);
        return Ok(*tree_res);
    }

    std::vector<std::string> get_available_providers() {
        if (!_plugin_manager) {
            return {};
        }
        // Get device-manager plugins from plugin manager (NOT tree-like!)
        auto res = _plugin_manager->get_children_names(DataPath("/device-manager"));
        if (!res) {
            return {};
        }
        return *res;
    }

private:
    std::unique_ptr<ProvidersProxy> _providers_proxy;
    std::shared_ptr<SettingsManager> _settings_manager;
    std::shared_ptr<RegisteredObjectsManager> _windows_manager;

    std::map<std::string, TreeLikePtr> _providers;
    std::map<std::string, Value> _root_metadata;

    std::shared_ptr<PluginManager> _plugin_manager;
    std::shared_ptr<Dispatcher> _dispatcher;
};

// ========== ProvidersProxy Implementation ==========

Result<ProvidersProxy::ProviderAndPath> ProvidersProxy::_get_provider_and_path(const DataPath& path) {
    auto parts = path.as_list();
    if (parts.empty()) {
        return Err<ProviderAndPath>("ProvidersProxy: empty path");
    }

    std::string provider_name = parts[0];
    auto provider_res = _kernel->get_provider(provider_name);
    if (!provider_res) {
        return Err<ProviderAndPath>("ProvidersProxy: failed to get provider '" + provider_name + "'", provider_res);
    }

    DataPath remaining(std::vector<std::string>(parts.begin() + 1, parts.end()));
    return Ok(ProviderAndPath{*provider_res, remaining});
}

Result<std::vector<std::string>> ProvidersProxy::get_children_names(const DataPath& path) {
    std::string path_str = path.to_string();

    // Root - return available providers
    if (path_str == "/" || path_str.empty()) {
        return Ok(_kernel->get_available_providers());
    }

    // Delegate to provider
    auto res = _get_provider_and_path(path);
    if (!res) {
        return Err<std::vector<std::string>>("ProvidersProxy: get_children_names failed", res);
    }
    return res->provider->get_children_names(res->remaining);
}

Result<Dict> ProvidersProxy::get_metadata(const DataPath& path) {
    std::string path_str = path.to_string();

    // Root
    if (path_str == "/" || path_str.empty()) {
        return Ok(Dict{
            {"name", Value("providers")},
            {"label", Value("Providers")},
            {"type", Value("folder")},
            {"category", Value("folder")}
        });
    }

    // Delegate to provider
    auto res = _get_provider_and_path(path);
    if (!res) {
        return Err<Dict>("ProvidersProxy: get_metadata failed", res);
    }
    return res->provider->get_metadata(res->remaining);
}

Result<std::vector<std::string>> ProvidersProxy::get_metadata_keys(const DataPath& path) {
    auto res = get_metadata(path);
    if (!res) return Err<std::vector<std::string>>("get_metadata_keys failed", res);
    std::vector<std::string> keys;
    for (const auto& [k, _] : *res) keys.push_back(k);
    return Ok(keys);
}

Result<Value> ProvidersProxy::get(const DataPath& path) {
    auto res = _get_provider_and_path(path);
    if (!res) {
        return Err<Value>("ProvidersProxy: get failed", res);
    }
    return res->provider->get(res->remaining);
}

Result<void> ProvidersProxy::open(const DataPath& path, const Dict& params) {
    auto res = _get_provider_and_path(path);
    if (!res) {
        return Err<void>("ProvidersProxy: open failed - could not get provider", res);
    }

    // Check if provider has an open method (via metadata capabilities)
    // For now, we assume all providers support opening by delegating
    // This would typically call provider->open(remaining, params)

    // Since TreeLike doesn't have open(), we need to get metadata and check buffer
    auto meta_res = res->provider->get_metadata(res->remaining);
    if (!meta_res) {
        return Err<void>("ProvidersProxy: open failed - could not get metadata", meta_res);
    }

    // Check if there's a buffer in metadata (audio device pattern)
    auto& meta = *meta_res;
    if (meta.find("buffer") != meta.end()) {
        // Already has buffer - nothing to open
        return Ok();
    }

    // For actual opening, provider-specific logic would go here
    // Currently just validate the path exists
    return Ok();
}

} // namespace ymery::plugins

extern "C" const char* name() { return "kernel"; }
extern "C" const char* type() { return "tree-like"; }
extern "C" void* create(
    std::shared_ptr<ymery::Dispatcher> dispatcher,
    std::shared_ptr<ymery::PluginManager> plugin_manager
) {
    return static_cast<void*>(new ymery::Result<ymery::TreeLikePtr>(ymery::plugins::Kernel::create(dispatcher, plugin_manager)));
}
