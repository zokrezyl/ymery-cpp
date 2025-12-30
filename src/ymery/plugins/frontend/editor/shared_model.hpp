// Shared layout model between editor-canvas and editor-preview
// Uses YAML-equivalent Value/Dict structure directly
#pragma once

#include "../../../types.hpp"
#include <string>
#include <vector>
#include <memory>
#include <atomic>

namespace ymery::plugins {

// Selection path - tracks which node is selected in the tree
using SelectionPath = std::vector<size_t>;  // indices into body lists

// Data entry in the data: section
struct DataEntry {
    std::string name;       // Entry name (e.g., "demo-data", "kernel")
    std::string type;       // Tree type (e.g., "data-tree", "kernel", "waveform")
    Dict metadata;          // Metadata for the entry
    std::vector<DataEntry> children;  // Children (for data-tree types)
};

// Fast PRNG for UID generation (xorshift32)
inline uint32_t g_uid_state = 0x12345678;

inline std::string generate_uid(const std::string& widget_type) {
    // xorshift32 - fast PRNG
    uint32_t x = g_uid_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_uid_state = x;

    // Generate 10 char alphanumeric string
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    std::string uid;
    uid.reserve(widget_type.size() + 11);
    uid = widget_type + "-";
    for (int i = 0; i < 10; ++i) {
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        uid += chars[x % 36];
    }
    g_uid_state = x;
    return uid;
}

class SharedLayoutModel {
public:
    static SharedLayoutModel& instance() {
        static SharedLayoutModel inst;
        return inst;
    }

    // The YAML structure - this IS what gets rendered
    Value& root() { return _root; }
    const Value& root() const { return _root; }

    bool empty() const {
        // Empty if root is not a Dict or has no widget key
        auto dict = get_as<Dict>(_root);
        return !dict || dict->empty();
    }

    // Version number - increments on each modification
    uint64_t version() const { return _version; }

    // Selection tracking
    const SelectionPath& selection() const { return _selection; }
    void select(const SelectionPath& path) { _selection = path; }
    void clear_selection() { _selection.clear(); }

    // Get the selected node (returns pointer to the Value, or nullptr)
    Value* get_selected() {
        return _navigate(_root, _selection);
    }

    // Set root widget: creates {widget_type: {uid: "type-xxx", label: "widget_type"}}
    void set_root(const std::string& widget_type) {
        Dict props;
        props["uid"] = generate_uid(widget_type);
        props["label"] = widget_type;
        Dict root;
        root[widget_type] = Value(props);
        _root = Value(root);
        _selection.clear();
        _bump_version();
    }

    void clear() {
        _root = Value();
        _selection.clear();
        _bump_version();
    }

    // Add child to root widget's body list
    void add_child_to_root(const std::string& widget_type, bool same_line = false) {
        auto root_dict = get_as<Dict>(_root);
        if (!root_dict || root_dict->empty()) return;

        // Get root widget type and props
        std::string root_type = root_dict->begin()->first;
        auto root_props = get_as<Dict>(root_dict->begin()->second);

        Dict props;
        if (root_props) {
            props = *root_props;
        }

        // Get or create body list
        List body;
        auto body_it = props.find("body");
        if (body_it != props.end()) {
            if (auto existing = get_as<List>(body_it->second)) {
                body = *existing;
            }
        }

        // If same_line, insert a same-line widget first
        if (same_line) {
            Dict sl_props;
            sl_props["uid"] = generate_uid("same-line");
            Dict sl_widget;
            sl_widget["same-line"] = Value(sl_props);
            body.push_back(Value(sl_widget));
        }

        // Create new widget: {widget_type: {uid: "...", label: "widget_type"}}
        Dict new_props;
        new_props["uid"] = generate_uid(widget_type);
        new_props["label"] = widget_type;
        Dict new_widget;
        new_widget[widget_type] = Value(new_props);
        body.push_back(Value(new_widget));

        // Rebuild root
        props["body"] = Value(body);
        Dict new_root;
        new_root[root_type] = Value(props);
        _root = Value(new_root);
        _bump_version();
    }

    // Add child at a specific path (rebuilds tree)
    void add_child(const SelectionPath& parent_path, const std::string& widget_type, bool same_line = false) {
        if (parent_path.empty()) {
            add_child_to_root(widget_type, same_line);
            return;
        }
        // For nested paths, rebuild the whole tree
        _root = _add_child_recursive(_root, parent_path, 0, widget_type, same_line);
        _bump_version();
    }

    // Insert sibling before the node at path
    void insert_before(const SelectionPath& path, const std::string& widget_type, bool same_line = false) {
        if (path.empty()) {
            // Root node - wrap in column first
            wrap_root_in_column();
            SelectionPath child_path = {0};
            _root = _insert_sibling_recursive(_root, child_path, 0, widget_type, false, same_line);
            _bump_version();
            return;
        }
        _root = _insert_sibling_recursive(_root, path, 0, widget_type, false, same_line);
        _bump_version();
    }

    // Insert sibling after the node at path
    void insert_after(const SelectionPath& path, const std::string& widget_type, bool same_line = false) {
        if (path.empty()) {
            // Root node - wrap in column first
            wrap_root_in_column();
            SelectionPath child_path = {0};
            _root = _insert_sibling_recursive(_root, child_path, 0, widget_type, true, same_line);
            _bump_version();
            return;
        }
        _root = _insert_sibling_recursive(_root, path, 0, widget_type, true, same_line);
        _bump_version();
    }

    // Wrap root in a column container
    void wrap_root_in_column() {
        if (empty()) return;

        // Current root becomes a child
        Value old_root = _root;

        // Create column with old root as child
        List body;
        body.push_back(old_root);

        Dict props;
        props["uid"] = generate_uid("column");
        props["label"] = std::string("column");
        props["body"] = Value(body);

        Dict new_root;
        new_root["column"] = Value(props);
        _root = Value(new_root);
        _bump_version();
    }

    // Remove node at path
    void remove(const SelectionPath& path) {
        if (path.empty()) {
            clear();  // Remove root = clear all
            return;
        }
        _root = _remove_recursive(_root, path, 0);
        _selection.clear();
        _bump_version();
    }

    // Change widget type at path
    void change_type(const SelectionPath& path, const std::string& new_type) {
        _root = _change_type_recursive(_root, path, 0, new_type);
        _bump_version();
    }

    // Set label at path
    void set_label_at(const SelectionPath& path, const std::string& label) {
        _root = _set_label_recursive(_root, path, 0, label);
        _bump_version();
    }

    // Set data-path at path
    void set_data_path_at(const SelectionPath& path, const std::string& data_path) {
        _root = _set_data_path_recursive(_root, path, 0, data_path);
        _bump_version();
    }

    // Move node up (swap with previous sibling)
    bool can_move_up(const SelectionPath& path) const {
        if (path.empty()) return false;
        return path.back() > 0;
    }

    void move_up(const SelectionPath& path) {
        if (!can_move_up(path)) return;
        _root = _swap_siblings_recursive(_root, path, 0, -1);
        // Update selection
        SelectionPath new_sel = path;
        new_sel.back()--;
        _selection = new_sel;
        _bump_version();
    }

    // Move node down (swap with next sibling)
    bool can_move_down(const SelectionPath& path) const {
        if (path.empty()) return false;
        // Need to check sibling count - use helper
        return _can_move_down_check(_root, path, 0);
    }

    void move_down(const SelectionPath& path) {
        if (!can_move_down(path)) return;
        _root = _swap_siblings_recursive(_root, path, 0, 1);
        // Update selection
        SelectionPath new_sel = path;
        new_sel.back()++;
        _selection = new_sel;
        _bump_version();
    }

    // Get widget type from a widget Value
    static std::string get_widget_type(const Value& widget) {
        auto dict = get_as<Dict>(widget);
        if (!dict || dict->empty()) return "";
        return dict->begin()->first;
    }

    // Get props from a widget Value
    static Dict* get_props(Value& widget) {
        auto dict = get_as<Dict>(widget);
        if (!dict || dict->empty()) return nullptr;
        auto& props_val = dict->begin()->second;
        return const_cast<Dict*>(get_as<Dict>(props_val).operator->());
    }

    // Get label from widget
    static std::string get_label(const Value& widget) {
        auto dict = get_as<Dict>(widget);
        if (!dict || dict->empty()) return "";
        auto props = get_as<Dict>(dict->begin()->second);
        if (!props) return "";
        auto label_it = props->find("label");
        if (label_it == props->end()) return "";
        auto label = get_as<std::string>(label_it->second);
        return label ? *label : "";
    }

    // Get uid from widget
    static std::string get_uid(const Value& widget) {
        auto dict = get_as<Dict>(widget);
        if (!dict || dict->empty()) return "";
        auto props = get_as<Dict>(dict->begin()->second);
        if (!props) return "";
        auto uid_it = props->find("uid");
        if (uid_it == props->end()) return "";
        auto uid = get_as<std::string>(uid_it->second);
        return uid ? *uid : "";
    }

    // Get data-path from widget
    static std::string get_data_path(const Value& widget) {
        auto dict = get_as<Dict>(widget);
        if (!dict || dict->empty()) return "";
        auto props = get_as<Dict>(dict->begin()->second);
        if (!props) return "";
        auto dp_it = props->find("data-path");
        if (dp_it == props->end()) return "";
        auto dp = get_as<std::string>(dp_it->second);
        return dp ? *dp : "";
    }

    // Set label on widget
    static void set_label(Value& widget, const std::string& label) {
        auto dict = get_as<Dict>(widget);
        if (!dict || dict->empty()) return;
        auto& [type, props_val] = *dict->begin();
        auto props = get_as<Dict>(props_val);
        if (!props) {
            Dict new_props;
            new_props["label"] = label;
            (*dict)[type] = Value(new_props);
        } else {
            (*props)["label"] = label;
            (*dict)[type] = Value(*props);
        }
    }

    // Get body list from widget (or empty)
    static List get_body(const Value& widget) {
        auto dict = get_as<Dict>(widget);
        if (!dict || dict->empty()) return {};
        auto props = get_as<Dict>(dict->begin()->second);
        if (!props) return {};
        auto body_it = props->find("body");
        if (body_it == props->end()) return {};
        auto body = get_as<List>(body_it->second);
        return body ? *body : List{};
    }

    // Check if widget type is a container
    static bool is_container(const std::string& type) {
        static const std::vector<std::string> containers = {
            "window", "row", "column", "group", "child",
            "tab-bar", "tab-item", "tree-node", "collapsing-header",
            "popup", "popup-modal", "tooltip", "implot", "implot-group",
            "coolbar", "dockable-window", "docking-main-window", "docking-split"
        };
        return std::find(containers.begin(), containers.end(), type) != containers.end();
    }

    // ========== Data Entries Management ==========

    std::vector<DataEntry>& data_entries() { return _data_entries; }
    const std::vector<DataEntry>& data_entries() const { return _data_entries; }

    void add_data_entry(const std::string& type, const std::string& name) {
        DataEntry entry;
        entry.name = name;
        entry.type = type;
        _data_entries.push_back(entry);
        _bump_version();
    }

    void remove_data_entry(size_t idx) {
        if (idx < _data_entries.size()) {
            // Also remove live tree if exists
            if (idx < _data_entries.size()) {
                _live_trees.erase(_data_entries[idx].name);
            }
            _data_entries.erase(_data_entries.begin() + idx);
            _bump_version();
        }
    }

    // ========== Live Tree Management ==========

    void set_live_tree(const std::string& name, TreeLikePtr tree) {
        _live_trees[name] = tree;
        _bump_version();
    }

    TreeLikePtr get_live_tree(const std::string& name) const {
        auto it = _live_trees.find(name);
        return it != _live_trees.end() ? it->second : nullptr;
    }

    const std::map<std::string, TreeLikePtr>& live_trees() const {
        return _live_trees;
    }

    std::vector<std::string> live_tree_names() const {
        std::vector<std::string> names;
        for (const auto& [name, tree] : _live_trees) {
            names.push_back(name);
        }
        return names;
    }

    Result<std::vector<std::string>> get_tree_children(const std::string& name, const DataPath& path) const {
        auto tree = get_live_tree(name);
        if (!tree) {
            return Err<std::vector<std::string>>("Tree '" + name + "' not found");
        }
        return tree->get_children_names(path);
    }

    void add_child_to_data_entry(size_t entry_idx, const std::string& type, const std::string& name) {
        if (entry_idx < _data_entries.size()) {
            DataEntry child;
            child.name = name;
            child.type = type;
            _data_entries[entry_idx].children.push_back(child);
            _bump_version();
        }
    }

    // Recursively add child to nested entry
    static void add_child_to_data_entry_recursive(DataEntry& parent, const std::string& type, const std::string& name) {
        DataEntry child;
        child.name = name;
        child.type = type;
        parent.children.push_back(child);
    }

    static void remove_child_from_data_entry(DataEntry& parent, size_t idx) {
        if (idx < parent.children.size()) {
            parent.children.erase(parent.children.begin() + idx);
        }
    }

    // Check if data entry type supports children
    static bool data_type_supports_children(const std::string& type) {
        return type == "data-tree" || type == "simple-data-tree" || type == "map";
    }

private:
    SharedLayoutModel() = default;

    void _bump_version() { ++_version; }

    Value _root;
    SelectionPath _selection;
    std::vector<DataEntry> _data_entries;
    std::map<std::string, TreeLikePtr> _live_trees;
    uint64_t _version = 0;

    // Navigate to a node by path
    Value* _navigate(Value& node, const SelectionPath& path) {
        if (path.empty()) return &node;

        Value* current = &node;
        for (size_t idx : path) {
            List body = get_body(*current);
            if (idx >= body.size()) return nullptr;
            // Need to navigate into the actual tree, not copies
            // This is tricky with std::any...
            // For now, return nullptr - we'll fix navigation later
        }
        return current;
    }

    // Recursively rebuild tree with new child added at path
    Value _add_child_recursive(const Value& node, const SelectionPath& path, size_t depth, const std::string& widget_type, bool same_line = false) {
        auto dict = get_as<Dict>(node);
        if (!dict || dict->empty()) return node;

        std::string type = dict->begin()->first;
        auto props = get_as<Dict>(dict->begin()->second);
        Dict new_props = props ? *props : Dict{};

        // Get current body
        List body;
        auto body_it = new_props.find("body");
        if (body_it != new_props.end()) {
            if (auto existing = get_as<List>(body_it->second)) {
                body = *existing;
            }
        }

        if (depth >= path.size()) {
            // We're at the target - add child here
            // If same_line, insert a same-line widget first
            if (same_line) {
                Dict sl_props;
                sl_props["uid"] = generate_uid("same-line");
                Dict sl_widget;
                sl_widget["same-line"] = Value(sl_props);
                body.push_back(Value(sl_widget));
            }
            Dict child_props;
            child_props["uid"] = generate_uid(widget_type);
            child_props["label"] = widget_type;
            Dict child_widget;
            child_widget[widget_type] = Value(child_props);
            body.push_back(Value(child_widget));
        } else {
            // Recurse into the child at path[depth]
            size_t idx = path[depth];
            if (idx < body.size()) {
                body[idx] = _add_child_recursive(body[idx], path, depth + 1, widget_type, same_line);
            }
        }

        new_props["body"] = Value(body);
        Dict new_node;
        new_node[type] = Value(new_props);
        return Value(new_node);
    }

    // Insert sibling at path (before or after)
    Value _insert_sibling_recursive(const Value& node, const SelectionPath& path, size_t depth, const std::string& widget_type, bool after, bool same_line = false) {
        auto dict = get_as<Dict>(node);
        if (!dict || dict->empty()) return node;

        std::string type = dict->begin()->first;
        auto props = get_as<Dict>(dict->begin()->second);
        Dict new_props = props ? *props : Dict{};

        List body;
        auto body_it = new_props.find("body");
        if (body_it != new_props.end()) {
            if (auto existing = get_as<List>(body_it->second)) {
                body = *existing;
            }
        }

        if (depth == path.size() - 1) {
            // Insert here
            size_t idx = path[depth];
            if (idx <= body.size()) {
                size_t insert_pos = after ? idx + 1 : idx;
                if (insert_pos > body.size()) insert_pos = body.size();

                // If same_line, insert a same-line widget first
                if (same_line) {
                    Dict sl_props;
                    sl_props["uid"] = generate_uid("same-line");
                    Dict sl_widget;
                    sl_widget["same-line"] = Value(sl_props);
                    body.insert(body.begin() + insert_pos, Value(sl_widget));
                    insert_pos++;  // Move insert position after same-line
                }

                Dict child_props;
                child_props["uid"] = generate_uid(widget_type);
                child_props["label"] = widget_type;
                Dict child_widget;
                child_widget[widget_type] = Value(child_props);
                body.insert(body.begin() + insert_pos, Value(child_widget));
            }
        } else if (depth < path.size() - 1) {
            size_t idx = path[depth];
            if (idx < body.size()) {
                body[idx] = _insert_sibling_recursive(body[idx], path, depth + 1, widget_type, after, same_line);
            }
        }

        new_props["body"] = Value(body);
        Dict new_node;
        new_node[type] = Value(new_props);
        return Value(new_node);
    }

    // Remove node at path
    Value _remove_recursive(const Value& node, const SelectionPath& path, size_t depth) {
        auto dict = get_as<Dict>(node);
        if (!dict || dict->empty()) return node;

        std::string type = dict->begin()->first;
        auto props = get_as<Dict>(dict->begin()->second);
        Dict new_props = props ? *props : Dict{};

        List body;
        auto body_it = new_props.find("body");
        if (body_it != new_props.end()) {
            if (auto existing = get_as<List>(body_it->second)) {
                body = *existing;
            }
        }

        if (depth == path.size() - 1) {
            // Remove here
            size_t idx = path[depth];
            if (idx < body.size()) {
                body.erase(body.begin() + idx);
            }
        } else if (depth < path.size() - 1) {
            size_t idx = path[depth];
            if (idx < body.size()) {
                body[idx] = _remove_recursive(body[idx], path, depth + 1);
            }
        }

        new_props["body"] = Value(body);
        Dict new_node;
        new_node[type] = Value(new_props);
        return Value(new_node);
    }

    // Change type at path
    Value _change_type_recursive(const Value& node, const SelectionPath& path, size_t depth, const std::string& new_type) {
        auto dict = get_as<Dict>(node);
        if (!dict || dict->empty()) return node;

        std::string type = dict->begin()->first;
        auto props = get_as<Dict>(dict->begin()->second);
        Dict new_props = props ? *props : Dict{};

        if (path.empty() || depth >= path.size()) {
            // Change this node's type
            Dict new_node;
            new_node[new_type] = Value(new_props);
            return Value(new_node);
        }

        List body;
        auto body_it = new_props.find("body");
        if (body_it != new_props.end()) {
            if (auto existing = get_as<List>(body_it->second)) {
                body = *existing;
            }
        }

        size_t idx = path[depth];
        if (idx < body.size()) {
            body[idx] = _change_type_recursive(body[idx], path, depth + 1, new_type);
        }

        new_props["body"] = Value(body);
        Dict new_node;
        new_node[type] = Value(new_props);
        return Value(new_node);
    }

    // Set label at path
    Value _set_label_recursive(const Value& node, const SelectionPath& path, size_t depth, const std::string& label) {
        auto dict = get_as<Dict>(node);
        if (!dict || dict->empty()) return node;

        std::string type = dict->begin()->first;
        auto props = get_as<Dict>(dict->begin()->second);
        Dict new_props = props ? *props : Dict{};

        if (path.empty() || depth >= path.size()) {
            // Set label on this node
            new_props["label"] = label;
            Dict new_node;
            new_node[type] = Value(new_props);
            return Value(new_node);
        }

        List body;
        auto body_it = new_props.find("body");
        if (body_it != new_props.end()) {
            if (auto existing = get_as<List>(body_it->second)) {
                body = *existing;
            }
        }

        size_t idx = path[depth];
        if (idx < body.size()) {
            body[idx] = _set_label_recursive(body[idx], path, depth + 1, label);
        }

        new_props["body"] = Value(body);
        Dict new_node;
        new_node[type] = Value(new_props);
        return Value(new_node);
    }

    // Set data-path at path
    Value _set_data_path_recursive(const Value& node, const SelectionPath& path, size_t depth, const std::string& data_path) {
        auto dict = get_as<Dict>(node);
        if (!dict || dict->empty()) return node;

        std::string type = dict->begin()->first;
        auto props = get_as<Dict>(dict->begin()->second);
        Dict new_props = props ? *props : Dict{};

        if (path.empty() || depth >= path.size()) {
            // Set data-path on this node
            if (data_path.empty()) {
                new_props.erase("data-path");
            } else {
                new_props["data-path"] = data_path;
            }
            Dict new_node;
            new_node[type] = Value(new_props);
            return Value(new_node);
        }

        List body;
        auto body_it = new_props.find("body");
        if (body_it != new_props.end()) {
            if (auto existing = get_as<List>(body_it->second)) {
                body = *existing;
            }
        }

        size_t idx = path[depth];
        if (idx < body.size()) {
            body[idx] = _set_data_path_recursive(body[idx], path, depth + 1, data_path);
        }

        new_props["body"] = Value(body);
        Dict new_node;
        new_node[type] = Value(new_props);
        return Value(new_node);
    }

    // Swap siblings (direction: -1 = up, +1 = down)
    Value _swap_siblings_recursive(const Value& node, const SelectionPath& path, size_t depth, int direction) {
        auto dict = get_as<Dict>(node);
        if (!dict || dict->empty()) return node;

        std::string type = dict->begin()->first;
        auto props = get_as<Dict>(dict->begin()->second);
        Dict new_props = props ? *props : Dict{};

        List body;
        auto body_it = new_props.find("body");
        if (body_it != new_props.end()) {
            if (auto existing = get_as<List>(body_it->second)) {
                body = *existing;
            }
        }

        if (depth == path.size() - 1) {
            // Swap here
            size_t idx = path[depth];
            size_t other_idx = idx + direction;
            if (idx < body.size() && other_idx < body.size()) {
                std::swap(body[idx], body[other_idx]);
            }
        } else if (depth < path.size() - 1) {
            size_t idx = path[depth];
            if (idx < body.size()) {
                body[idx] = _swap_siblings_recursive(body[idx], path, depth + 1, direction);
            }
        }

        new_props["body"] = Value(body);
        Dict new_node;
        new_node[type] = Value(new_props);
        return Value(new_node);
    }

    // Check if can move down (has next sibling)
    bool _can_move_down_check(const Value& node, const SelectionPath& path, size_t depth) const {
        auto dict = get_as<Dict>(node);
        if (!dict || dict->empty()) return false;

        auto props = get_as<Dict>(dict->begin()->second);
        if (!props) return false;

        auto body_it = props->find("body");
        if (body_it == props->end()) return false;

        auto body = get_as<List>(body_it->second);
        if (!body) return false;

        if (depth == path.size() - 1) {
            size_t idx = path[depth];
            return idx + 1 < body->size();
        } else if (depth < path.size() - 1) {
            size_t idx = path[depth];
            if (idx < body->size()) {
                return _can_move_down_check((*body)[idx], path, depth + 1);
            }
        }
        return false;
    }
};

} // namespace ymery::plugins
