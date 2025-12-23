#include "layout_model.hpp"
#include <algorithm>
#include <sstream>

namespace ymery::editor {

// Container widget types
static const std::vector<std::string> CONTAINER_TYPES = {
    "window", "row", "column", "group", "child",
    "tab-bar", "tab-item", "tree-node", "collapsing-header",
    "popup", "popup-modal", "tooltip", "implot", "implot-group",
    "coolbar"
};

bool is_container_type(const std::string& type) {
    return std::find(CONTAINER_TYPES.begin(), CONTAINER_TYPES.end(), type) != CONTAINER_TYPES.end();
}

// LayoutNode implementation

std::atomic<int> LayoutModel::_id_counter{0};

LayoutNode::LayoutNode() : id(0) {}

LayoutNode::LayoutNode(const std::string& type)
    : widget_type(type), label(type) {}

bool LayoutNode::is_container() const {
    return is_container_type(widget_type);
}

bool LayoutNode::can_have_children() const {
    return is_container();
}

// LayoutModel implementation

LayoutModel::LayoutModel() = default;

bool LayoutModel::empty() const {
    return !_root;
}

LayoutNode* LayoutModel::root() const {
    return _root.get();
}

void LayoutModel::set_root(const std::string& widget_type) {
    _root = std::make_unique<LayoutNode>(widget_type);
    _root->id = _next_id();
    _selected = _root.get();
}

void LayoutModel::clear() {
    _root.reset();
    _selected = nullptr;
}

LayoutNode* LayoutModel::selected() const {
    return _selected;
}

void LayoutModel::select(LayoutNode* node) {
    _selected = node;
}

void LayoutModel::clear_selection() {
    _selected = nullptr;
}

LayoutNode* LayoutModel::find_by_id(int id) {
    if (!_root) return nullptr;
    return _find_by_id_recursive(_root.get(), id);
}

LayoutNode* LayoutModel::_find_by_id_recursive(LayoutNode* node, int id) {
    if (node->id == id) return node;
    for (auto& child : node->children) {
        if (auto found = _find_by_id_recursive(child.get(), id)) {
            return found;
        }
    }
    return nullptr;
}

int LayoutModel::_next_id() {
    return ++_id_counter;
}

void LayoutModel::insert_before(LayoutNode* target, const std::string& widget_type, LayoutPosition pos) {
    if (!target || !target->parent) return;

    auto new_node = std::make_unique<LayoutNode>(widget_type);
    new_node->id = _next_id();
    new_node->position = pos;
    new_node->parent = target->parent;

    auto& siblings = target->parent->children;
    for (auto it = siblings.begin(); it != siblings.end(); ++it) {
        if (it->get() == target) {
            siblings.insert(it, std::move(new_node));
            break;
        }
    }
}

void LayoutModel::insert_after(LayoutNode* target, const std::string& widget_type, LayoutPosition pos) {
    if (!target || !target->parent) return;

    auto new_node = std::make_unique<LayoutNode>(widget_type);
    new_node->id = _next_id();
    new_node->position = pos;
    new_node->parent = target->parent;

    auto& siblings = target->parent->children;
    for (auto it = siblings.begin(); it != siblings.end(); ++it) {
        if (it->get() == target) {
            ++it;  // Move past target
            siblings.insert(it, std::move(new_node));
            break;
        }
    }
}

void LayoutModel::add_child(LayoutNode* parent, const std::string& widget_type) {
    if (!parent) {
        // No parent means add to root
        if (!_root) {
            set_root(widget_type);
        } else if (_root->can_have_children()) {
            add_child(_root.get(), widget_type);
        }
        return;
    }

    if (!parent->can_have_children()) return;

    auto new_node = std::make_unique<LayoutNode>(widget_type);
    new_node->id = _next_id();
    new_node->parent = parent;
    parent->children.push_back(std::move(new_node));
}

void LayoutModel::wrap_root_in_column() {
    if (!_root) return;

    // Create new column container
    auto column = std::make_unique<LayoutNode>("column");
    column->id = _next_id();
    column->label = "column";

    // Move current root to be child of column
    _root->parent = column.get();
    column->children.push_back(std::move(_root));

    // Column becomes new root
    _root = std::move(column);

    // Update selection to new root if old root was selected
    if (_selected && _selected->parent == _root.get()) {
        // Keep selection on the original widget
    } else if (_selected == nullptr) {
        _selected = _root.get();
    }
}

bool LayoutModel::can_move_up(LayoutNode* node) const {
    if (!node || !node->parent) return false;

    auto& siblings = node->parent->children;
    for (size_t i = 0; i < siblings.size(); ++i) {
        if (siblings[i].get() == node) {
            return i > 0;  // Can move up if not first child
        }
    }
    return false;
}

bool LayoutModel::can_move_down(LayoutNode* node) const {
    if (!node || !node->parent) return false;

    auto& siblings = node->parent->children;
    for (size_t i = 0; i < siblings.size(); ++i) {
        if (siblings[i].get() == node) {
            return i < siblings.size() - 1;  // Can move down if not last child
        }
    }
    return false;
}

void LayoutModel::move_up(LayoutNode* node) {
    if (!can_move_up(node)) return;

    auto& siblings = node->parent->children;
    for (size_t i = 1; i < siblings.size(); ++i) {
        if (siblings[i].get() == node) {
            // Swap with previous sibling
            std::swap(siblings[i], siblings[i - 1]);
            break;
        }
    }
}

void LayoutModel::move_down(LayoutNode* node) {
    if (!can_move_down(node)) return;

    auto& siblings = node->parent->children;
    for (size_t i = 0; i < siblings.size() - 1; ++i) {
        if (siblings[i].get() == node) {
            // Swap with next sibling
            std::swap(siblings[i], siblings[i + 1]);
            break;
        }
    }
}

void LayoutModel::set_same_line(LayoutNode* node, bool same_line) {
    if (!node) return;
    node->position = same_line ? LayoutPosition::SameLine : LayoutPosition::NewLine;
}

void LayoutModel::change_type(LayoutNode* node, const std::string& new_type) {
    if (!node) return;
    node->widget_type = new_type;
    // Update label if it matches the old type
    if (node->label == node->widget_type || node->label.empty()) {
        node->label = new_type;
    }
}

void LayoutModel::move_node(LayoutNode* node, LayoutNode* new_parent, int position) {
    if (!node || !new_parent || !node->parent) return;
    if (node == new_parent) return;  // Can't move to self
    if (!new_parent->can_have_children()) return;

    // Check if new_parent is a descendant of node (would create cycle)
    for (auto* p = new_parent; p; p = p->parent) {
        if (p == node) return;  // Can't move to own descendant
    }

    // Remove from old parent
    auto* old_parent = node->parent;
    std::unique_ptr<LayoutNode> node_ptr;

    auto& old_siblings = old_parent->children;
    for (auto it = old_siblings.begin(); it != old_siblings.end(); ++it) {
        if (it->get() == node) {
            node_ptr = std::move(*it);
            old_siblings.erase(it);
            break;
        }
    }

    if (!node_ptr) return;

    // Add to new parent
    node_ptr->parent = new_parent;
    if (position < 0 || position >= static_cast<int>(new_parent->children.size())) {
        new_parent->children.push_back(std::move(node_ptr));
    } else {
        new_parent->children.insert(new_parent->children.begin() + position, std::move(node_ptr));
    }
}

void LayoutModel::remove(LayoutNode* node) {
    if (!node) return;

    // Can't remove root this way, use clear() instead
    if (node == _root.get()) {
        clear();
        return;
    }

    if (!node->parent) return;

    // Clear selection if removing selected node
    if (_selected == node) {
        _selected = node->parent;
    }

    auto& siblings = node->parent->children;
    for (auto it = siblings.begin(); it != siblings.end(); ++it) {
        if (it->get() == node) {
            siblings.erase(it);
            break;
        }
    }
}

std::string LayoutModel::to_yaml() const {
    if (!_root) return "";

    std::string yaml;
    yaml += "widgets:\n";

    // First, define widget types used
    yaml += "  # Widget type definitions\n";

    // Then the main widget
    yaml += "\n  main-widget:\n";
    _to_yaml_recursive(_root.get(), yaml, 4);

    yaml += "\napp:\n";
    yaml += "  widget: app.main-widget\n";

    return yaml;
}

void LayoutModel::_to_yaml_recursive(const LayoutNode* node, std::string& out, int indent) const {
    std::string ind(indent, ' ');

    out += ind + "type: " + node->widget_type + "\n";

    if (!node->label.empty() && node->label != node->widget_type) {
        out += ind + "label: \"" + node->label + "\"\n";
    }

    // Output properties
    for (const auto& [key, value] : node->properties) {
        out += ind + key + ": \"" + value + "\"\n";
    }

    // Output children as body
    if (!node->children.empty()) {
        out += ind + "body:\n";
        for (const auto& child : node->children) {
            // Handle same-line positioning
            if (child->position == LayoutPosition::SameLine) {
                out += ind + "  # same-line\n";
            }
            out += ind + "  - " + child->widget_type + ":\n";
            _to_yaml_recursive(child.get(), out, indent + 6);
        }
    }
}

} // namespace ymery::editor
