// Shared layout model between editor-canvas and editor-preview
// Matches ymery-editor's LayoutModel exactly
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <atomic>

namespace ymery::plugins {

// Container widget types
inline const std::vector<std::string> CONTAINER_TYPES = {
    "window", "row", "column", "group", "child",
    "tab-bar", "tab-item", "tree-node", "collapsing-header",
    "popup", "popup-modal", "tooltip", "implot", "implot-group",
    "coolbar"
};

inline bool is_container_type(const std::string& type) {
    return std::find(CONTAINER_TYPES.begin(), CONTAINER_TYPES.end(), type) != CONTAINER_TYPES.end();
}

enum class LayoutPosition {
    NewLine,
    SameLine
};

struct LayoutNode {
    int id;
    std::string widget_type;
    std::string label;
    std::map<std::string, std::string> properties;
    std::vector<std::unique_ptr<LayoutNode>> children;
    LayoutNode* parent = nullptr;
    LayoutPosition position = LayoutPosition::NewLine;

    LayoutNode(int id_, const std::string& type)
        : id(id_), widget_type(type), label(type) {}

    bool is_container() const { return is_container_type(widget_type); }
    bool can_have_children() const { return is_container(); }
};

class SharedLayoutModel {
public:
    static SharedLayoutModel& instance() {
        static SharedLayoutModel inst;
        return inst;
    }

    bool empty() const { return !_root; }
    LayoutNode* root() const { return _root.get(); }
    LayoutNode* selected() const { return _selected; }

    void set_root(const std::string& widget_type) {
        _root = std::make_unique<LayoutNode>(_next_id(), widget_type);
        _selected = _root.get();
    }

    void clear() {
        _root.reset();
        _selected = nullptr;
    }

    void select(LayoutNode* node) { _selected = node; }

    LayoutNode* find_by_id(int id) {
        return _find_recursive(_root.get(), id);
    }

    void add_child(LayoutNode* parent, const std::string& widget_type) {
        if (!parent) return;
        auto child = std::make_unique<LayoutNode>(_next_id(), widget_type);
        child->parent = parent;
        parent->children.push_back(std::move(child));
    }

    void insert_after(LayoutNode* target, const std::string& widget_type, LayoutPosition pos = LayoutPosition::NewLine) {
        if (!target || !target->parent) return;
        auto* parent = target->parent;
        auto child = std::make_unique<LayoutNode>(_next_id(), widget_type);
        child->parent = parent;
        child->position = pos;

        for (size_t i = 0; i < parent->children.size(); ++i) {
            if (parent->children[i].get() == target) {
                parent->children.insert(parent->children.begin() + i + 1, std::move(child));
                break;
            }
        }
    }

    void insert_before(LayoutNode* target, const std::string& widget_type, LayoutPosition pos = LayoutPosition::NewLine) {
        if (!target || !target->parent) return;
        auto* parent = target->parent;
        auto child = std::make_unique<LayoutNode>(_next_id(), widget_type);
        child->parent = parent;
        child->position = pos;

        for (size_t i = 0; i < parent->children.size(); ++i) {
            if (parent->children[i].get() == target) {
                parent->children.insert(parent->children.begin() + i, std::move(child));
                break;
            }
        }
    }

    void change_type(LayoutNode* node, const std::string& new_type) {
        if (node) {
            node->widget_type = new_type;
            // Keep label unless it matches old type
            if (node->label == node->widget_type || node->label.empty()) {
                node->label = new_type;
            }
        }
    }

    void remove(LayoutNode* node) {
        if (!node) return;
        if (!node->parent) {
            // Root node - clear everything
            if (_selected == node) _selected = nullptr;
            _root.reset();
            return;
        }
        auto* parent = node->parent;
        for (auto it = parent->children.begin(); it != parent->children.end(); ++it) {
            if (it->get() == node) {
                if (_selected == node) _selected = nullptr;
                parent->children.erase(it);
                break;
            }
        }
    }

    void wrap_root_in_column() {
        if (!_root) return;
        auto column = std::make_unique<LayoutNode>(_next_id(), "column");
        _root->parent = column.get();
        column->children.push_back(std::move(_root));
        _root = std::move(column);
    }

    bool can_move_up(LayoutNode* node) const {
        if (!node || !node->parent) return false;
        auto& siblings = node->parent->children;
        for (size_t i = 0; i < siblings.size(); ++i) {
            if (siblings[i].get() == node) return i > 0;
        }
        return false;
    }

    bool can_move_down(LayoutNode* node) const {
        if (!node || !node->parent) return false;
        auto& siblings = node->parent->children;
        for (size_t i = 0; i < siblings.size(); ++i) {
            if (siblings[i].get() == node) return i < siblings.size() - 1;
        }
        return false;
    }

    void move_up(LayoutNode* node) {
        if (!can_move_up(node)) return;
        auto& siblings = node->parent->children;
        for (size_t i = 1; i < siblings.size(); ++i) {
            if (siblings[i].get() == node) {
                std::swap(siblings[i], siblings[i-1]);
                break;
            }
        }
    }

    void move_down(LayoutNode* node) {
        if (!can_move_down(node)) return;
        auto& siblings = node->parent->children;
        for (size_t i = 0; i < siblings.size() - 1; ++i) {
            if (siblings[i].get() == node) {
                std::swap(siblings[i], siblings[i+1]);
                break;
            }
        }
    }

    void set_same_line(LayoutNode* node, bool same_line) {
        if (node) {
            node->position = same_line ? LayoutPosition::SameLine : LayoutPosition::NewLine;
        }
    }

private:
    SharedLayoutModel() = default;

    std::unique_ptr<LayoutNode> _root;
    LayoutNode* _selected = nullptr;
    std::atomic<int> _id_counter{0};

    int _next_id() { return ++_id_counter; }

    LayoutNode* _find_recursive(LayoutNode* node, int id) {
        if (!node) return nullptr;
        if (node->id == id) return node;
        for (auto& child : node->children) {
            if (auto* found = _find_recursive(child.get(), id)) return found;
        }
        return nullptr;
    }
};

} // namespace ymery::plugins
