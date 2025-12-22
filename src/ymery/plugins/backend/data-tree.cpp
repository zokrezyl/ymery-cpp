// data-tree backend plugin - tree with explicit children/metadata structure
// Each node has { "children": {...}, "metadata": {...} } format
#include "../../types.hpp"
#include "../../result.hpp"
#include <map>
#include <spdlog/spdlog.h>

namespace ymery::plugins {

class DataTree : public TreeLike {
public:
    static Result<TreeLikePtr> create() {
        auto tree = std::make_shared<DataTree>();
        if (auto res = tree->init(); !res) {
            return Err<TreeLikePtr>("DataTree::create failed", res);
        }
        return tree;
    }

    Result<void> init() override {
        // Initialize root node
        _nodes["/"] = Node{};
        return Ok();
    }

    Result<std::vector<std::string>> get_children_names(const DataPath& path) override {
        auto path_str = path.to_string();
        auto it = _nodes.find(path_str);
        if (it != _nodes.end()) {
            std::vector<std::string> names;
            for (const auto& [name, _] : it->second.children) {
                names.push_back(name);
            }
            return Ok(names);
        }
        return Ok(std::vector<std::string>{});
    }

    Result<Dict> get_metadata(const DataPath& path) override {
        auto path_str = path.to_string();
        auto it = _nodes.find(path_str);
        if (it != _nodes.end()) {
            return Ok(it->second.metadata);
        }
        return Ok(Dict{});
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
        // Get a specific metadata value - last component is the key
        DataPath node_path = path.dirname();
        std::string key = path.filename();

        auto path_str = node_path.to_string();
        auto it = _nodes.find(path_str);
        if (it != _nodes.end()) {
            auto meta_it = it->second.metadata.find(key);
            if (meta_it != it->second.metadata.end()) {
                return Ok(meta_it->second);
            }
        }
        return Ok(Value{});
    }

    Result<void> set(const DataPath& path, const Value& value) override {
        // Set a specific metadata value - last component is the key
        DataPath node_path = path.dirname();
        std::string key = path.filename();

        auto path_str = node_path.to_string();
        // Ensure node exists
        if (_nodes.find(path_str) == _nodes.end()) {
            _nodes[path_str] = Node{};
        }
        _nodes[path_str].metadata[key] = value;
        return Ok();
    }

    Result<void> add_child(const DataPath& path, const std::string& name, const Dict& data) override {
        auto path_str = path.to_string();

        // Ensure parent node exists
        if (_nodes.find(path_str) == _nodes.end()) {
            _nodes[path_str] = Node{};
        }

        // Add child reference to parent
        _nodes[path_str].children[name] = true;

        // Create child node with metadata
        auto child_path = path / name;
        auto child_path_str = child_path.to_string();
        _nodes[child_path_str] = Node{};
        _nodes[child_path_str].metadata = data;

        return Ok();
    }

    Result<std::string> as_tree(const DataPath& path, int depth) override {
        std::string result;
        auto path_str = path.to_string();

        auto it = _nodes.find(path_str);
        if (it == _nodes.end()) {
            return Ok(path_str + " (not found)");
        }

        std::string indent(depth * 2, ' ');
        result += indent + path.filename();

        // Show metadata
        if (!it->second.metadata.empty()) {
            result += " {";
            bool first = true;
            for (const auto& [k, v] : it->second.metadata) {
                if (!first) result += ", ";
                result += k + ": ...";
                first = false;
            }
            result += "}";
        }
        result += "\n";

        // Recurse into children
        if (depth < 10) {
            for (const auto& [child_name, _] : it->second.children) {
                auto child_path = path / child_name;
                auto child_res = as_tree(child_path, depth + 1);
                if (child_res) {
                    result += *child_res;
                }
            }
        }

        return Ok(result);
    }

private:
    struct Node {
        std::map<std::string, bool> children;  // child names
        Dict metadata;                          // node metadata
    };

    std::map<std::string, Node> _nodes;
};

} // namespace ymery::plugins

extern "C" const char* name() { return "data-tree"; }
extern "C" const char* type() { return "tree"; }
extern "C" ymery::Result<ymery::TreeLikePtr> create() {
    return ymery::plugins::DataTree::create();
}
