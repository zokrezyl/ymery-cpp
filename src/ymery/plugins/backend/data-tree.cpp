// data-tree backend plugin - tree with explicit children/metadata structure
// Each node has { "children": {...}, "metadata": {...} } format
// Wraps YAML::Node for native YAML serialization support
#include "../../types.hpp"
#include "../../result.hpp"
#include <yaml-cpp/yaml.h>
#include <spdlog/spdlog.h>

namespace ymery::plugins {

class DataTree : public TreeLike {
public:
    // Create empty tree
    static Result<TreeLikePtr> create() {
        auto tree = std::make_shared<DataTree>();
        if (auto res = tree->init(); !res) {
            return Err<TreeLikePtr>("DataTree::create failed", res);
        }
        return tree;
    }

    // Create from existing YAML::Node
    static Result<TreeLikePtr> create(YAML::Node node) {
        auto tree = std::make_shared<DataTree>();
        tree->_root = node;
        if (auto res = tree->init(); !res) {
            return Err<TreeLikePtr>("DataTree::create failed", res);
        }
        return tree;
    }

    Result<void> init() override {
        // Ensure root is a valid node with structure
        if (!_root.IsDefined() || _root.IsNull()) {
            _root = YAML::Node(YAML::NodeType::Map);
        }
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
            auto it = _nested_trees.find(path.to_string());
            if (it != _nested_trees.end()) {
                return it->second->get_children_names(remaining);
            }
        }

        // Get children from "children" key
        if (!node.IsMap()) {
            return Ok(std::vector<std::string>{});
        }

        auto children = node["children"];
        if (!children.IsDefined() || !children.IsMap()) {
            return Ok(std::vector<std::string>{});
        }

        std::vector<std::string> names;
        for (auto it = children.begin(); it != children.end(); ++it) {
            names.push_back(it->first.as<std::string>());
        }
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
            auto it = _nested_trees.find(path.to_string());
            if (it != _nested_trees.end()) {
                return it->second->get_metadata(remaining);
            }
        }

        // Get metadata from "metadata" key
        if (!node.IsMap()) {
            return Ok(Dict{});
        }

        auto metadata = node["metadata"];
        if (!metadata.IsDefined() || !metadata.IsMap()) {
            return Ok(Dict{});
        }

        return Ok(_yaml_to_dict(metadata));
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
            return Err<Value>("DataTree::get: empty key");
        }

        auto nav_res = _navigate(node_path);
        if (!nav_res) {
            return Ok(Value{});
        }

        auto& [node, remaining] = *nav_res;

        // Check if delegating to nested TreeLike
        if (!remaining.is_root()) {
            auto it = _nested_trees.find(node_path.to_string());
            if (it != _nested_trees.end()) {
                return it->second->get(remaining / key);
            }
        }

        // Get from metadata
        if (!node.IsMap()) {
            return Ok(Value{});
        }

        auto metadata = node["metadata"];
        if (!metadata.IsDefined() || !metadata.IsMap()) {
            return Ok(Value{});
        }

        auto val = metadata[key];
        if (!val.IsDefined()) {
            return Ok(Value{});
        }

        return Ok(_yaml_to_value(val));
    }

    Result<void> set(const DataPath& path, const Value& value) override {
        // Last component is the metadata key
        DataPath node_path = path.dirname();
        std::string key = path.filename();

        if (key.empty()) {
            return Err<void>("DataTree::set: empty key");
        }

        auto nav_res = _navigate(node_path);
        if (!nav_res) {
            // Create path if it doesn't exist
            _ensure_path(node_path);
            nav_res = _navigate(node_path);
            if (!nav_res) {
                return Err<void>("DataTree::set: failed to create path");
            }
        }

        auto& [node, remaining] = *nav_res;

        // Check if delegating to nested TreeLike
        if (!remaining.is_root()) {
            auto it = _nested_trees.find(node_path.to_string());
            if (it != _nested_trees.end()) {
                return it->second->set(remaining / key, value);
            }
        }

        // Ensure metadata exists
        if (!node["metadata"].IsDefined()) {
            node["metadata"] = YAML::Node(YAML::NodeType::Map);
        }

        // Set value
        node["metadata"][key] = _value_to_yaml(value);
        return Ok();
    }

    Result<void> add_child(const DataPath& path, const std::string& name, const Dict& data) override {
        auto nav_res = _navigate(path);
        if (!nav_res) {
            // Create path if it doesn't exist
            _ensure_path(path);
            nav_res = _navigate(path);
            if (!nav_res) {
                return Err<void>("DataTree::add_child: failed to create path");
            }
        }

        auto& [node, remaining] = *nav_res;

        // Check if delegating to nested TreeLike
        if (!remaining.is_root()) {
            auto it = _nested_trees.find(path.to_string());
            if (it != _nested_trees.end()) {
                return it->second->add_child(remaining, name, data);
            }
        }

        // Ensure children exists
        if (!node["children"].IsDefined()) {
            node["children"] = YAML::Node(YAML::NodeType::Map);
        }

        // Create child node with metadata
        YAML::Node child(YAML::NodeType::Map);
        child["metadata"] = _dict_to_yaml(data);
        node["children"][name] = child;

        return Ok();
    }

    Result<std::string> as_tree(const DataPath& path, int depth) override {
        return Ok(YAML::Dump(_root));
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

            // Navigate through children
            if (!current.IsMap()) {
                return Err<std::pair<YAML::Node, DataPath>>(
                    "DataTree::_navigate: node is not a map at '" + part + "'");
            }

            auto children = current["children"];
            if (!children.IsDefined() || !children.IsMap()) {
                return Err<std::pair<YAML::Node, DataPath>>(
                    "DataTree::_navigate: no children at '" + part + "'");
            }

            auto child = children[part];
            if (!child.IsDefined()) {
                return Err<std::pair<YAML::Node, DataPath>>(
                    "DataTree::_navigate: child '" + part + "' not found");
            }

            current = child;
        }

        return Ok(std::make_pair(current, DataPath::root()));
    }

    // Ensure path exists (create intermediate nodes)
    void _ensure_path(const DataPath& path) {
        if (path.is_root() || path.as_list().empty()) {
            return;
        }

        YAML::Node current = _root;
        for (const auto& part : path.as_list()) {
            if (!current["children"].IsDefined()) {
                current["children"] = YAML::Node(YAML::NodeType::Map);
            }
            if (!current["children"][part].IsDefined()) {
                current["children"][part] = YAML::Node(YAML::NodeType::Map);
            }
            current = current["children"][part];
        }
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

extern "C" const char* name() { return "data-tree"; }
extern "C" const char* type() { return "tree"; }
extern "C" ymery::Result<ymery::TreeLikePtr> create() {
    return ymery::plugins::DataTree::create();
}
