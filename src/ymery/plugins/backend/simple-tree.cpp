// simple-tree backend plugin - in-memory tree data store
#include "../../types.hpp"
#include "../../result.hpp"
#include <map>

namespace ymery::plugins {

class SimpleTree : public TreeLike {
public:
    static Result<TreeLikePtr> create() {
        auto tree = std::make_shared<SimpleTree>();
        if (auto res = tree->init(); !res) {
            return Err<TreeLikePtr>("SimpleTree::create failed", res);
        }
        return tree;
    }

    Result<std::vector<std::string>> get_children_names(const DataPath& path) override {
        auto it = _children.find(path.to_string());
        if (it != _children.end()) {
            return Ok(it->second);
        }
        return Ok(std::vector<std::string>{});
    }

    Result<Dict> get_metadata(const DataPath& path) override {
        auto it = _metadata.find(path.to_string());
        if (it != _metadata.end()) {
            return Ok(it->second);
        }
        return Ok(Dict{});
    }

    Result<std::vector<std::string>> get_metadata_keys(const DataPath& path) override {
        auto it = _metadata.find(path.to_string());
        if (it != _metadata.end()) {
            std::vector<std::string> keys;
            for (const auto& [k, _] : it->second) {
                keys.push_back(k);
            }
            return Ok(keys);
        }
        return Ok(std::vector<std::string>{});
    }

    Result<Value> get(const DataPath& path) override {
        auto it = _values.find(path.to_string());
        if (it != _values.end()) {
            return Ok(it->second);
        }
        return Ok(Value{});
    }

    Result<void> set(const DataPath& path, const Value& value) override {
        _values[path.to_string()] = value;
        return Ok();
    }

    Result<void> add_child(const DataPath& path, const std::string& name, const Dict& data) override {
        auto path_str = path.to_string();
        _children[path_str].push_back(name);
        auto child_path = path / name;
        for (const auto& [k, v] : data) {
            _metadata[child_path.to_string()][k] = v;
        }
        return Ok();
    }

    Result<std::string> as_tree(const DataPath& path, int depth) override {
        return Ok(path.to_string());
    }

private:
    std::map<std::string, std::vector<std::string>> _children;
    std::map<std::string, Dict> _metadata;
    std::map<std::string, Value> _values;
};

} // namespace ymery::plugins

extern "C" const char* name() { return "simple-tree"; }
extern "C" const char* type() { return "tree"; }
extern "C" ymery::Result<ymery::TreeLikePtr> create() {
    return ymery::plugins::SimpleTree::create();
}
