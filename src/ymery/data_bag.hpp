#pragma once

#include "result.hpp"
#include "types.hpp"
#include "object.hpp"
#include <map>
#include <memory>
#include <regex>

namespace ymery {

class Dispatcher;
class PluginManager;

// DataBag - bridges widgets to data trees with reference resolution
class DataBag : public Object {
public:
    // Factory method
    static Result<std::shared_ptr<DataBag>> create(
        std::shared_ptr<Dispatcher> dispatcher,
        std::shared_ptr<PluginManager> plugin_manager,
        std::map<std::string, TreeLikePtr> data_trees,
        const std::string& main_data_key,
        const DataPath& main_data_path,
        const Dict& statics
    );

    // Create child DataBag with inherited context
    Result<std::shared_ptr<DataBag>> inherit(
        const std::string& data_path_spec,
        const Dict& statics
    );

    // Data access
    Result<Value> get(const std::string& key, const Value& default_value = {});
    Result<void> set(const std::string& key, const Value& value);

    // Static config access (no reference resolution)
    Result<Value> get_static(const std::string& key, const Value& default_value = {});

    // Metadata access
    Result<Dict> get_metadata();
    Result<std::vector<std::string>> get_metadata_keys();
    Result<std::vector<std::string>> get_children_names();

    // Path info
    Result<DataPath> get_data_path();
    Result<std::string> get_data_path_str();
    std::string main_data_key() const { return _main_data_key; }

    // Tree browsing (for editor/inspector use)
    std::vector<std::string> get_tree_names() const;
    Result<std::vector<std::string>> get_tree_children(const std::string& tree_name, const DataPath& path);

    // Child management
    Result<void> add_child(const Dict& child_spec);

    // Lifecycle
    Result<void> init() override;

private:
    DataBag() = default;

    // Reference resolution
    Result<Value> _resolve_reference(const std::string& ref_str);
    Result<std::pair<TreeLikePtr, DataPath>> _parse_data_path_spec(const std::string& spec);
    bool _is_reference(const std::string& str);
    bool _has_interpolation(const std::string& str);
    Result<std::string> _interpolate(const std::string& str);

    std::shared_ptr<Dispatcher> _dispatcher;
    std::shared_ptr<PluginManager> _plugin_manager;
    std::map<std::string, TreeLikePtr> _data_trees;
    TreeLikePtr _main_data_tree;
    std::string _main_data_key;
    DataPath _main_data_path;
    Dict _statics;
};

using DataBagPtr = std::shared_ptr<DataBag>;

} // namespace ymery
