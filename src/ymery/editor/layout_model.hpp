#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <atomic>

namespace ymery::editor {

// Represents how a widget is positioned relative to siblings
enum class LayoutPosition {
    NewLine,    // Widget on its own line
    SameLine    // Widget on same line as previous sibling
};

// Represents a single widget node in the layout tree
struct LayoutNode {
    int id;                                           // Unique ID for selection/identification
    std::string widget_type;                          // e.g., "button", "window", "row"
    std::string label;                                // Display label
    std::map<std::string, std::string> properties;   // Widget properties
    std::vector<std::unique_ptr<LayoutNode>> children;
    LayoutNode* parent = nullptr;
    LayoutPosition position = LayoutPosition::NewLine;

    LayoutNode();
    LayoutNode(const std::string& type);

    bool is_container() const;
    bool can_have_children() const;
};

using LayoutNodePtr = std::unique_ptr<LayoutNode>;

// Manages the layout being edited
class LayoutModel {
public:
    LayoutModel();

    // Check if layout is empty
    bool empty() const;

    // Get/set root
    LayoutNode* root() const;
    void set_root(const std::string& widget_type);
    void clear();

    // Selection
    LayoutNode* selected() const;
    void select(LayoutNode* node);
    void clear_selection();

    // Find node by ID
    LayoutNode* find_by_id(int id);

    // Insertion operations
    void insert_before(LayoutNode* target, const std::string& widget_type, LayoutPosition pos = LayoutPosition::NewLine);
    void insert_after(LayoutNode* target, const std::string& widget_type, LayoutPosition pos = LayoutPosition::NewLine);
    void add_child(LayoutNode* parent, const std::string& widget_type);

    // Wrap current root in a column container
    void wrap_root_in_column();

    // Movement operations
    bool can_move_up(LayoutNode* node) const;
    bool can_move_down(LayoutNode* node) const;
    void move_up(LayoutNode* node);    // Swap with previous sibling
    void move_down(LayoutNode* node);  // Swap with next sibling
    void set_same_line(LayoutNode* node, bool same_line);  // Change position
    void change_type(LayoutNode* node, const std::string& new_type);  // Change widget type
    void move_node(LayoutNode* node, LayoutNode* new_parent, int position = -1);  // Move node to new parent

    // Deletion
    void remove(LayoutNode* node);

    // Export to YAML
    std::string to_yaml() const;

private:
    LayoutNodePtr _root;
    LayoutNode* _selected = nullptr;
    static std::atomic<int> _id_counter;

    static int _next_id();
    LayoutNode* _find_by_id_recursive(LayoutNode* node, int id);
    void _to_yaml_recursive(const LayoutNode* node, std::string& out, int indent) const;
};

// List of container widget types that can have children
bool is_container_type(const std::string& type);

} // namespace ymery::editor
