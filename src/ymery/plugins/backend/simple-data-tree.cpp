// simple-data-tree backend plugin - wrapper around any data structure
// Maps any YAML structure to TreeLike interface:
// - Dict keys become children
// - List indices become children ("0", "1", ...)
// - Primitives are leaves
// - Delegates to nested TreeLike objects
#include "../../types.hpp"
#include "../../result.hpp"
#include <yaml-cpp/yaml.h>
#include <spdlog/spdlog.h>

// Forward declarations for plugin create signature
namespace ymery { class Dispatcher; class PluginManager; }

namespace ymery::plugins {

class SimpleDataTree : public TreeLike {
public:
    // Create empty tree (with empty map as root)
    static Result<TreeLikePtr> create() {
        auto tree = std::make_shared<SimpleDataTree>();
        tree->_root = YAML::Node(YAML::NodeType::Map);  // Initialize as empty map
        if (auto res = tree->init(); !res) {
            return Err<TreeLikePtr>("SimpleDataTree::create failed", res);
        }
        return tree;
    }

    // Create from existing YAML::Node
    static Result<TreeLikePtr> create(YAML::Node node) {
        auto tree = std::make_shared<SimpleDataTree>();
        tree->_root = node;
        if (auto res = tree->init(); !res) {
            return Err<TreeLikePtr>("SimpleDataTree::create failed", res);
        }
        return tree;
    }

    Result<void> init() override {
        return Ok();
    }

    Result<std::vector<std::string>> get_children_names(const DataPath& path) override {
        auto nav_res = _navigate(path);
        if (!nav_res) {
            return Ok(std::vector<std::string>{});
        }

        auto& [node, remaining] = *nav_res;

        // Check if delegating to nested TreeLike
        if (!remaining.is_root()) {
            auto nested_path = _get_path_before_remaining(path, remaining);
            auto it = _nested_trees.find(nested_path.to_string());
            if (it != _nested_trees.end()) {
                return it->second->get_children_names(remaining);
            }
        }

        std::vector<std::string> names;

        // Dict - keys are children
        if (node.IsMap()) {
            for (auto it = node.begin(); it != node.end(); ++it) {
                names.push_back(it->first.as<std::string>());
            }
            return Ok(names);
        }

        // List - indices are children
        if (node.IsSequence()) {
            for (size_t i = 0; i < node.size(); ++i) {
                names.push_back(std::to_string(i));
            }
            return Ok(names);
        }

        // Primitive - no children
        return Ok(names);
    }

    Result<Dict> get_metadata(const DataPath& path) override {
        auto nav_res = _navigate(path);
        if (!nav_res) {
            return Ok(Dict{});
        }

        auto& [node, remaining] = *nav_res;

        // Check if delegating to nested TreeLike
        if (!remaining.is_root()) {
            auto nested_path = _get_path_before_remaining(path, remaining);
            auto it = _nested_trees.find(nested_path.to_string());
            if (it != _nested_trees.end()) {
                return it->second->get_metadata(remaining);
            }
        }

        // Get the key name (last component of path)
        std::string key_name = path.filename();

        Dict metadata;

        // Dict or list - label is the key name
        if (node.IsMap() || node.IsSequence()) {
            metadata["label"] = Value(key_name);
            return Ok(metadata);
        }

        // Primitive - label is "key: value" or just "value" for root
        std::string value_str = node.IsDefined() ? node.as<std::string>("") : "";
        if (!key_name.empty()) {
            metadata["label"] = Value(key_name + ": " + value_str);
        } else {
            metadata["label"] = Value(value_str);
        }

        return Ok(metadata);
    }

    Result<std::vector<std::string>> get_metadata_keys(const DataPath& path) override {
        auto res = get_metadata(path);
        if (!res) {
            return Err<std::vector<std::string>>("get_metadata failed", res);
        }
        std::vector<std::string> keys;
        for (const auto& [k, _] : *res) {
            keys.push_back(k);
        }
        return Ok(keys);
    }

    Result<Value> get(const DataPath& path) override {
        // Last component is the metadata key
        DataPath node_path = path.dirname();
        std::string key = path.filename();

        if (key.empty()) {
            return Err<Value>("SimpleDataTree::get: empty key");
        }

        auto nav_res = _navigate(node_path);
        if (!nav_res) {
            return Ok(Value{});
        }

        auto& [node, remaining] = *nav_res;

        // Check if delegating to nested TreeLike
        if (!remaining.is_root()) {
            auto nested_path = _get_path_before_remaining(node_path, remaining);
            auto it = _nested_trees.find(nested_path.to_string());
            if (it != _nested_trees.end()) {
                return it->second->get(remaining / key);
            }
        }

        // Get metadata for this node and return the key
        auto meta_res = get_metadata(node_path);
        if (!meta_res) {
            return Ok(Value{});
        }

        auto it = (*meta_res).find(key);
        if (it != (*meta_res).end()) {
            return Ok(it->second);
        }

        return Ok(Value{});
    }

    Result<void> set(const DataPath& path, const Value& value) override {
        // SimpleDataTree is read-only in Python, but we can support modification
        DataPath node_path = path.dirname();
        std::string key = path.filename();

        if (key.empty()) {
            return Err<void>("SimpleDataTree::set: empty key");
        }

        auto nav_res = _navigate(node_path);
        if (!nav_res) {
            return Err<void>("SimpleDataTree::set: path not found");
        }

        auto& [node, remaining] = *nav_res;

        // Check if delegating to nested TreeLike
        if (!remaining.is_root()) {
            auto nested_path = _get_path_before_remaining(node_path, remaining);
            auto it = _nested_trees.find(nested_path.to_string());
            if (it != _nested_trees.end()) {
                return it->second->set(remaining / key, value);
            }
        }

        // For SimpleDataTree, setting "label" on a primitive changes the value
        if (key == "label" && node.IsScalar()) {
            if (auto s = get_as<std::string>(value)) {
                // Parse "key: value" format if present
                auto colon_pos = s->find(": ");
                if (colon_pos != std::string::npos) {
                    node = s->substr(colon_pos + 2);
                } else {
                    node = *s;
                }
                return Ok();
            }
        }

        // For maps, we can set values directly
        if (node.IsMap()) {
            node[key] = _value_to_yaml(value);
            return Ok();
        }

        return Err<void>("SimpleDataTree::set: cannot set on this node type");
    }

    Result<void> add_child(const DataPath& path, const std::string& name, const Dict& data) override {
        auto nav_res = _navigate(path);
        if (!nav_res) {
            return Err<void>("SimpleDataTree::add_child: path not found");
        }

        auto& [node, remaining] = *nav_res;

        // Check if delegating to nested TreeLike
        if (!remaining.is_root()) {
            auto nested_path = _get_path_before_remaining(path, remaining);
            auto it = _nested_trees.find(nested_path.to_string());
            if (it != _nested_trees.end()) {
                return it->second->add_child(remaining, name, data);
            }
        }

        // For maps, add as new key
        if (node.IsMap()) {
            // If data has "label", use that as the value for primitives
            auto label_it = data.find("label");
            if (label_it != data.end()) {
                node[name] = _value_to_yaml(label_it->second);
            } else {
                // Create a map with the data
                node[name] = _dict_to_yaml(data);
            }
            return Ok();
        }

        // For sequences, push to end (name is ignored)
        if (node.IsSequence()) {
            auto label_it = data.find("label");
            if (label_it != data.end()) {
                node.push_back(_value_to_yaml(label_it->second));
            } else {
                node.push_back(_dict_to_yaml(data));
            }
            return Ok();
        }

        return Err<void>("SimpleDataTree::add_child: cannot add child to this node type");
    }

    Result<std::string> as_tree(const DataPath& path, int depth) override {
        auto nav_res = _navigate(path);
        if (!nav_res) {
            return Ok(path.to_string() + " (not found)");
        }
        return Ok(YAML::Dump((*nav_res).first));
    }

    // Register a nested TreeLike at a path
    void register_nested(const DataPath& path, TreeLikePtr tree) {
        _nested_trees[path.to_string()] = tree;
    }

    // Get the underlying YAML::Node (for serialization)
    YAML::Node& root() { return _root; }
    const YAML::Node& root() const { return _root; }

private:
    // Navigate to a node, returns (node, remaining_path)
    // remaining_path is non-root if we hit a nested TreeLike boundary
    Result<std::pair<YAML::Node, DataPath>> _navigate(const DataPath& path) {
        if (path.is_root() || path.as_list().empty()) {
            return Ok(std::make_pair(_root, DataPath::root()));
        }

        YAML::Node current = _root;
        const auto& parts = path.as_list();

        for (size_t i = 0; i < parts.size(); ++i) {
            const auto& part = parts[i];

            // Check for nested TreeLike at current path
            DataPath current_path = DataPath(std::vector<std::string>(parts.begin(), parts.begin() + i + 1));
            auto nested_it = _nested_trees.find(current_path.to_string());
            if (nested_it != _nested_trees.end()) {
                // Return remaining path for delegation
                DataPath remaining(std::vector<std::string>(parts.begin() + i + 1, parts.end()));
                return Ok(std::make_pair(current, remaining));
            }

            // Navigate based on node type
            if (current.IsMap()) {
                auto child = current[part];
                if (!child.IsDefined()) {
                    return Err<std::pair<YAML::Node, DataPath>>(
                        "SimpleDataTree::_navigate: key '" + part + "' not found");
                }
                current = child;
            } else if (current.IsSequence()) {
                try {
                    size_t index = std::stoul(part);
                    if (index >= current.size()) {
                        return Err<std::pair<YAML::Node, DataPath>>(
                            "SimpleDataTree::_navigate: index " + part + " out of range");
                    }
                    current = current[index];
                } catch (...) {
                    return Err<std::pair<YAML::Node, DataPath>>(
                        "SimpleDataTree::_navigate: '" + part + "' is not a valid index");
                }
            } else {
                // Primitive - cannot navigate further
                return Err<std::pair<YAML::Node, DataPath>>(
                    "SimpleDataTree::_navigate: cannot navigate through primitive at '" + part + "'");
            }
        }

        return Ok(std::make_pair(current, DataPath::root()));
    }

    // Get the path that leads to a nested TreeLike (path minus remaining)
    DataPath _get_path_before_remaining(const DataPath& full_path, const DataPath& remaining) {
        const auto& full_parts = full_path.as_list();
        const auto& rem_parts = remaining.as_list();

        if (full_parts.size() <= rem_parts.size()) {
            return DataPath::root();
        }

        size_t prefix_len = full_parts.size() - rem_parts.size();
        return DataPath(std::vector<std::string>(full_parts.begin(), full_parts.begin() + prefix_len));
    }

    // YAML <-> Value conversion helpers
    static Dict _yaml_to_dict(const YAML::Node& node) {
        Dict result;
        if (!node.IsMap()) return result;

        for (auto it = node.begin(); it != node.end(); ++it) {
            std::string key = it->first.as<std::string>();
            result[key] = _yaml_to_value(it->second);
        }
        return result;
    }

    static Value _yaml_to_value(const YAML::Node& node) {
        if (node.IsNull() || !node.IsDefined()) {
            return Value{};
        }
        if (node.IsScalar()) {
            std::string str = node.as<std::string>();
            // Try bool
            if (str == "true" || str == "True" || str == "TRUE") return Value(true);
            if (str == "false" || str == "False" || str == "FALSE") return Value(false);
            // Try int
            try {
                size_t pos;
                int i = std::stoi(str, &pos);
                if (pos == str.size()) return Value(i);
            } catch (...) {}
            // Try double
            try {
                size_t pos;
                double d = std::stod(str, &pos);
                if (pos == str.size()) return Value(d);
            } catch (...) {}
            // String
            return Value(str);
        }
        if (node.IsSequence()) {
            List list;
            for (const auto& item : node) {
                list.push_back(_yaml_to_value(item));
            }
            return Value(list);
        }
        if (node.IsMap()) {
            return Value(_yaml_to_dict(node));
        }
        return Value{};
    }

    static YAML::Node _dict_to_yaml(const Dict& dict) {
        YAML::Node node(YAML::NodeType::Map);
        for (const auto& [k, v] : dict) {
            node[k] = _value_to_yaml(v);
        }
        return node;
    }

    static YAML::Node _value_to_yaml(const Value& value) {
        if (!value.has_value()) {
            return YAML::Node();
        }
        if (auto s = get_as<std::string>(value)) {
            return YAML::Node(*s);
        }
        if (auto i = get_as<int>(value)) {
            return YAML::Node(*i);
        }
        if (auto d = get_as<double>(value)) {
            return YAML::Node(*d);
        }
        if (auto b = get_as<bool>(value)) {
            return YAML::Node(*b);
        }
        if (auto list = get_as<List>(value)) {
            YAML::Node node(YAML::NodeType::Sequence);
            for (const auto& item : *list) {
                node.push_back(_value_to_yaml(item));
            }
            return node;
        }
        if (auto dict = get_as<Dict>(value)) {
            return _dict_to_yaml(*dict);
        }
        return YAML::Node();
    }

    YAML::Node _root;
    std::map<std::string, TreeLikePtr> _nested_trees;
};

} // namespace ymery::plugins

extern "C" const char* name() { return "simple-data-tree"; }
extern "C" const char* type() { return "tree"; }
extern "C" ymery::Result<ymery::TreeLikePtr> create(
    std::shared_ptr<ymery::Dispatcher> /*dispatcher*/,
    std::shared_ptr<ymery::PluginManager> /*plugin_manager*/
) {
    return ymery::plugins::SimpleDataTree::create();
}
