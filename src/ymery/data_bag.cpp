#include "data_bag.hpp"
#include <regex>

namespace ymery {

Result<std::shared_ptr<DataBag>> DataBag::create(
    std::shared_ptr<Dispatcher> dispatcher,
    std::shared_ptr<PluginManager> plugin_manager,
    std::map<std::string, TreeLikePtr> data_trees,
    const std::string& main_data_key,
    const DataPath& main_data_path,
    const Dict& statics
) {
    auto bag = std::shared_ptr<DataBag>(new DataBag());
    bag->_dispatcher = dispatcher;
    bag->_plugin_manager = plugin_manager;
    bag->_data_trees = std::move(data_trees);
    bag->_main_data_key = main_data_key;
    bag->_main_data_path = main_data_path;
    bag->_statics = statics;

    // Set main data tree
    if (!main_data_key.empty()) {
        auto it = bag->_data_trees.find(main_data_key);
        if (it != bag->_data_trees.end()) {
            bag->_main_data_tree = it->second;
        }
    }

    if (auto res = bag->init(); !res) {
        return Err<std::shared_ptr<DataBag>>("DataBag::create: init failed", res);
    }

    return bag;
}

Result<void> DataBag::init() {
    // Process 'data' section from statics to create local data trees
    auto data_it = _statics.find("data");
    if (data_it != _statics.end()) {
        // TODO: Process data definitions and create local trees
        // This requires PluginManager to instantiate TreeLike implementations
    }

    // Process 'main-data' to override main tree
    auto main_data_it = _statics.find("main-data");
    if (main_data_it != _statics.end()) {
        if (auto key = get_as<std::string>(main_data_it->second)) {
            _main_data_key = *key;
            auto it = _data_trees.find(*key);
            if (it != _data_trees.end()) {
                _main_data_tree = it->second;
            }
        }
    }

    return Ok();
}

Result<std::shared_ptr<DataBag>> DataBag::inherit(
    const std::string& data_path_spec,
    const Dict& statics
) {
    DataPath new_path = _main_data_path;
    TreeLikePtr new_tree = _main_data_tree;
    std::string new_key = _main_data_key;

    // Parse data-path spec if provided
    if (!data_path_spec.empty()) {
        auto parsed = _parse_data_path_spec(data_path_spec);
        if (!parsed) {
            return Err<std::shared_ptr<DataBag>>(
                "DataBag::inherit: failed to parse data-path", parsed);
        }
        new_tree = (*parsed).first;
        new_path = (*parsed).second;

        // Find key for the new tree
        for (const auto& [k, v] : _data_trees) {
            if (v == new_tree) {
                new_key = k;
                break;
            }
        }
    }

    // Copy data_trees (sibling isolation)
    auto trees_copy = _data_trees;

    return create(
        _dispatcher,
        _plugin_manager,
        std::move(trees_copy),
        new_key,
        new_path,
        statics
    );
}

Result<Value> DataBag::get(const std::string& key, const Value& default_value) {
    // First check statics
    auto it = _statics.find(key);
    if (it != _statics.end()) {
        Value val = it->second;

        // Check if it's a reference
        if (auto str = get_as<std::string>(val)) {
            if (_is_reference(*str)) {
                return _resolve_reference(*str);
            }
            if (_has_interpolation(*str)) {
                auto interp = _interpolate(*str);
                if (!interp) {
                    return Err<Value>("DataBag::get: interpolation failed", interp);
                }
                return Ok(Value(*interp));
            }
        }
        return Ok(val);
    }

    // Then try main data tree
    if (_main_data_tree) {
        DataPath path = _main_data_path / key;
        auto res = _main_data_tree->get(path);
        if (res) {
            return res;
        }
    }

    // Return default
    if (default_value.has_value()) {
        return Ok(default_value);
    }

    return Err<Value>("DataBag::get: key '" + key + "' not found");
}

Result<void> DataBag::set(const std::string& key, const Value& value) {
    // Check if statics has a reference for this key
    auto it = _statics.find(key);
    if (it != _statics.end()) {
        if (auto str = get_as<std::string>(it->second)) {
            if (_is_reference(*str)) {
                auto parsed = _parse_data_path_spec(*str);
                if (!parsed) {
                    return Err<void>("DataBag::set: failed to parse reference", parsed);
                }
                auto& [tree, path] = *parsed;
                return tree->set(path, value);
            }
        }
    }

    // Default: set in main data tree
    if (_main_data_tree) {
        DataPath path = _main_data_path / key;
        return _main_data_tree->set(path, value);
    }

    return Err<void>("DataBag::set: no data tree available");
}

Result<Value> DataBag::get_static(const std::string& key, const Value& default_value) {
    auto it = _statics.find(key);
    if (it != _statics.end()) {
        return Ok(it->second);
    }
    if (default_value.has_value()) {
        return Ok(default_value);
    }
    return Err<Value>("DataBag::get_static: key '" + key + "' not found");
}

Result<Dict> DataBag::get_metadata() {
    if (!_main_data_tree) {
        return Err<Dict>("DataBag::get_metadata: no main data tree");
    }
    return _main_data_tree->get_metadata(_main_data_path);
}

Result<std::vector<std::string>> DataBag::get_metadata_keys() {
    if (!_main_data_tree) {
        return Err<std::vector<std::string>>(
            "DataBag::get_metadata_keys: no main data tree");
    }
    return _main_data_tree->get_metadata_keys(_main_data_path);
}

Result<std::vector<std::string>> DataBag::get_children_names() {
    if (!_main_data_tree) {
        return Err<std::vector<std::string>>(
            "DataBag::get_children_names: no main data tree");
    }
    return _main_data_tree->get_children_names(_main_data_path);
}

Result<DataPath> DataBag::get_data_path() {
    return Ok(_main_data_path);
}

Result<std::string> DataBag::get_data_path_str() {
    return Ok(_main_data_path.to_string());
}

Result<void> DataBag::add_child(const Dict& child_spec) {
    if (!_main_data_tree) {
        return Err<void>("DataBag::add_child: no main data tree");
    }

    auto name_it = child_spec.find("name");
    if (name_it == child_spec.end()) {
        return Err<void>("DataBag::add_child: 'name' required");
    }

    auto name = get_as<std::string>(name_it->second);
    if (!name) {
        return Err<void>("DataBag::add_child: 'name' must be string");
    }

    Dict data;
    auto metadata_it = child_spec.find("metadata");
    if (metadata_it != child_spec.end()) {
        if (auto md = get_as<Dict>(metadata_it->second)) {
            data = *md;
        }
    }

    return _main_data_tree->add_child(_main_data_path, *name, data);
}

bool DataBag::_is_reference(const std::string& str) {
    return !str.empty() && (str[0] == '@' || str[0] == '$');
}

bool DataBag::_has_interpolation(const std::string& str) {
    return str.find("$") != std::string::npos || str.find("@") != std::string::npos;
}

Result<Value> DataBag::_resolve_reference(const std::string& ref_str) {
    auto parsed = _parse_data_path_spec(ref_str);
    if (!parsed) {
        return Err<Value>("DataBag::_resolve_reference: parse failed", parsed);
    }

    auto& [tree, path] = *parsed;
    return tree->get(path);
}

Result<std::pair<TreeLikePtr, DataPath>> DataBag::_parse_data_path_spec(const std::string& spec) {
    TreeLikePtr tree = _main_data_tree;
    std::string path_str = spec;

    // Handle $tree@path or $tree format
    if (!spec.empty() && spec[0] == '$') {
        size_t at_pos = spec.find('@');
        std::string tree_name;

        if (at_pos != std::string::npos) {
            tree_name = spec.substr(1, at_pos - 1);
            path_str = spec.substr(at_pos + 1);
        } else {
            tree_name = spec.substr(1);
            path_str = "/";
        }

        auto it = _data_trees.find(tree_name);
        if (it == _data_trees.end()) {
            return Err<std::pair<TreeLikePtr, DataPath>>(
                "DataBag: tree '" + tree_name + "' not found");
        }
        tree = it->second;

        // Parse the path
        DataPath path = DataPath::parse(path_str);
        return Ok(std::make_pair(tree, path));
    }

    // Handle @path (relative to main tree)
    if (!spec.empty() && spec[0] == '@') {
        path_str = spec.substr(1);
        DataPath rel_path = DataPath::parse(path_str);

        // If absolute, use as-is; if relative, join with current path
        DataPath full_path = rel_path.is_absolute() ? rel_path : (_main_data_path / rel_path);
        return Ok(std::make_pair(tree, full_path));
    }

    // Plain path (relative to current position)
    DataPath path = _main_data_path / DataPath::parse(spec);
    return Ok(std::make_pair(tree, path));
}

Result<std::string> DataBag::_interpolate(const std::string& str) {
    // Simple interpolation: replace $key and @key with values
    std::string result = str;

    std::regex ref_pattern(R"([@$][a-zA-Z_][a-zA-Z0-9_-]*)");
    std::smatch match;
    std::string::const_iterator search_start = str.cbegin();

    std::string output;
    while (std::regex_search(search_start, str.cend(), match, ref_pattern)) {
        output += std::string(search_start, search_start + match.position());

        std::string ref = match[0];
        auto resolved = _resolve_reference(ref);
        if (resolved) {
            if (auto s = get_as<std::string>(*resolved)) {
                output += *s;
            } else {
                output += ref; // Keep original if can't convert to string
            }
        } else {
            output += ref; // Keep original if resolution fails
        }

        search_start = match.suffix().first;
    }
    output += std::string(search_start, str.cend());

    return Ok(output);
}

} // namespace ymery
