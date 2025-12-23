#pragma once

#include "result.hpp"
#include "types.hpp"
#include "dispatcher.hpp"
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <any>
#include <functional>

namespace ymery {

class WidgetFactory;
class DataBag;
class Widget;
using WidgetPtr = std::shared_ptr<Widget>;

// Plugin create function types
using WidgetCreateFn = std::function<Result<WidgetPtr>(
    std::shared_ptr<WidgetFactory>,
    std::shared_ptr<Dispatcher>,
    const std::string&,
    std::shared_ptr<DataBag>
)>;

using TreeLikeCreateFn = std::function<Result<TreeLikePtr>()>;

// Plugin metadata
struct PluginMeta {
    std::string class_name;
    std::string registered_name;
    std::string category;           // Widget category (e.g., "Inputs", "Containers")
    Dict meta;                       // Full meta.yaml contents as Dict
    std::any create_fn;             // Holds the appropriate create function
};

// PluginManager - TreeLike that holds all plugins organized by category
// Structure:
//   /widget/text -> PluginMeta for text widget
//   /widget/button -> PluginMeta for button widget
//   /tree-like/simple-tree -> PluginMeta for simple-tree
class PluginManager : public TreeLike, public std::enable_shared_from_this<PluginManager> {
public:
    static Result<std::shared_ptr<PluginManager>> create(const std::string& plugins_path);

    ~PluginManager();

    // TreeLike interface
    Result<std::vector<std::string>> get_children_names(const DataPath& path) override;
    Result<Dict> get_metadata(const DataPath& path) override;
    Result<std::vector<std::string>> get_metadata_keys(const DataPath& path) override;
    Result<Value> get(const DataPath& path) override;
    Result<void> set(const DataPath& path, const Value& value) override;
    Result<void> add_child(const DataPath& path, const std::string& name, const Dict& data) override;
    Result<std::string> as_tree(const DataPath& path, int depth = -1) override;

    // Lifecycle
    Result<void> init() override;
    Result<void> dispose() override;

    // Convenience methods for creating instances
    Result<WidgetPtr> create_widget(
        const std::string& name,
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    );

    Result<TreeLikePtr> create_tree(const std::string& name);

    // Check if plugin exists
    bool has_widget(const std::string& name) const;
    bool has_tree(const std::string& name) const;

private:
    PluginManager() = default;

    Result<void> _ensure_plugins_loaded();
    Result<void> _load_plugin(const std::string& path);

    std::string _plugins_path;
    bool _plugins_loaded = false;

    // Category -> Name -> PluginMeta
    std::map<std::string, std::map<std::string, PluginMeta>> _plugins;

    // Loaded .so handles
    std::vector<void*> _handles;
};

using PluginManagerPtr = std::shared_ptr<PluginManager>;

} // namespace ymery
