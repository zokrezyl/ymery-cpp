#pragma once

#include "layout_model.hpp"
#include "widget_tree.hpp"
#include "../plugin_manager.hpp"

namespace ymery::editor {

// Editor canvas - right pane where layout is built
class EditorCanvas {
public:
    EditorCanvas(LayoutModel& model, WidgetTree& widget_tree, ymery::PluginManagerPtr plugin_manager);

    // Render the canvas
    void render();

private:
    LayoutModel& _model;
    WidgetTree& _widget_tree;
    ymery::PluginManagerPtr _plugin_manager;

    // Pending insertion action (from context menu)
    struct PendingInsertion {
        LayoutNode* target = nullptr;
        enum class Action {
            None,
            InsertBefore, InsertAfter,
            InsertBeforeSameLine, InsertAfterSameLine,
            AddChild,
            // For root nodes - wrap in column first
            WrapInsertBefore, WrapInsertAfter,
            WrapInsertBeforeSameLine, WrapInsertAfterSameLine
        } action = Action::None;
    };
    PendingInsertion _pending;

    // Pending drop action (from drag & drop) - deferred to avoid iterator invalidation
    struct PendingDrop {
        int target_id = -1;  // Use ID instead of pointer to avoid dangling reference
        std::string widget_type;
        bool add_as_child = false;  // true = add child, false = insert after
    };
    std::vector<PendingDrop> _pending_drops;

    // Render methods
    void _render_empty_state();
    void _render_layout();
    void _render_node(LayoutNode* node, int depth = 0, bool same_line = false);
    void _render_context_menu(LayoutNode* node);
    void _render_properties_menu(LayoutNode* node);
    void _render_insertion_submenu(const char* label, LayoutNode* target, PendingInsertion::Action action);

    // Handle drop on a node (queues action for later)
    void _handle_drop(LayoutNode* target);

    // Process all pending drops (called after rendering)
    void _process_pending_drops();

    // Execute pending insertion
    void _execute_pending_insertion(const std::string& widget_type);

    // Wrap root in a column and add a new widget
    void _wrap_root_and_add(const std::string& widget_type, bool before);

    // Get widget metadata from plugin manager
    ymery::Dict _get_widget_meta(const std::string& widget_type);
};

} // namespace ymery::editor
