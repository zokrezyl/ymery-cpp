#include "editor_canvas.hpp"
#include <imgui.h>
#include <spdlog/spdlog.h>

namespace ymery::editor {

EditorCanvas::EditorCanvas(LayoutModel& model, WidgetTree& widget_tree)
    : _model(model), _widget_tree(widget_tree) {}

void EditorCanvas::render() {
    if (_model.empty()) {
        _render_empty_state();
    } else {
        _render_layout();
    }
}

void EditorCanvas::_render_empty_state() {
    // Center the "Add Widget" button
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 button_size(150, 50);

    ImGui::SetCursorPos(ImVec2(
        (avail.x - button_size.x) / 2,
        (avail.y - button_size.y) / 2
    ));

    if (ImGui::Button("+ Add Root Widget", button_size)) {
        ImGui::OpenPopup("add_root_widget");
    }

    // Also accept drops on empty canvas
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("WIDGET_TYPE")) {
            std::string type(static_cast<const char*>(payload->Data));
            _model.set_root(type);
            spdlog::info("Added root widget: {}", type);
        }
        ImGui::EndDragDropTarget();
    }

    // Popup with widget tree
    if (ImGui::BeginPopup("add_root_widget")) {
        ImGui::Text("Select widget type:");
        ImGui::Separator();

        _widget_tree.render_as_menu([this](const std::string& type) {
            _model.set_root(type);
            ImGui::CloseCurrentPopup();
        });

        ImGui::EndPopup();
    }
}

void EditorCanvas::_render_layout() {
    // Render the layout tree
    ImGui::Text("Layout:");
    ImGui::Separator();

    _render_node(_model.root());

    // Handle pending insertion from context menu
    if (_pending.action != PendingInsertion::Action::None) {
        ImGui::OpenPopup("insert_widget_popup");
    }

    // Insertion popup
    if (ImGui::BeginPopup("insert_widget_popup")) {
        ImGui::Text("Select widget to insert:");
        ImGui::Separator();

        _widget_tree.render_as_menu([this](const std::string& type) {
            _execute_pending_insertion(type);
            ImGui::CloseCurrentPopup();
        });

        ImGui::EndPopup();
    } else {
        // Reset pending if popup closed without selection
        _pending.action = PendingInsertion::Action::None;
        _pending.target = nullptr;
    }
}

void EditorCanvas::_render_node(LayoutNode* node, int depth) {
    if (!node) return;

    ImGui::PushID(node->id);

    // Indentation
    if (depth > 0) {
        ImGui::Indent(20.0f);
    }

    // Build display string
    std::string display;
    if (node->is_container()) {
        display = "[" + node->widget_type + "] " + node->label;
    } else {
        display = node->widget_type + ": " + node->label;
    }

    // Selection highlight
    bool is_selected = (_model.selected() == node);
    if (is_selected) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
    }

    // Render as button for clickability
    if (ImGui::Button(display.c_str(), ImVec2(-1, 0))) {
        _model.select(node);
    }

    if (is_selected) {
        ImGui::PopStyleColor();
    }

    // Drag and drop target
    _handle_drop(node);

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        _render_context_menu(node);
        ImGui::EndPopup();
    }

    // Render children
    for (auto& child : node->children) {
        _render_node(child.get(), depth + 1);
    }

    if (depth > 0) {
        ImGui::Unindent(20.0f);
    }

    ImGui::PopID();
}

void EditorCanvas::_handle_drop(LayoutNode* node) {
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("WIDGET_TYPE")) {
            std::string type(static_cast<const char*>(payload->Data));

            // If dropping on a container, add as child
            // Otherwise, insert after
            if (node->can_have_children()) {
                _model.add_child(node, type);
                spdlog::info("Added {} as child of {}", type, node->widget_type);
            } else {
                _model.insert_after(node, type);
                spdlog::info("Inserted {} after {}", type, node->widget_type);
            }
        }
        ImGui::EndDragDropTarget();
    }
}

void EditorCanvas::_render_context_menu(LayoutNode* node) {
    ImGui::Text("%s", node->widget_type.c_str());
    ImGui::Separator();

    // Insert options (only if node has a parent, i.e., not root)
    if (node->parent) {
        _render_insertion_submenu("Insert Before (same line)", node, PendingInsertion::Action::InsertBeforeSameLine);
        _render_insertion_submenu("Insert After (same line)", node, PendingInsertion::Action::InsertAfterSameLine);
        ImGui::Separator();
        _render_insertion_submenu("Insert Before", node, PendingInsertion::Action::InsertBefore);
        _render_insertion_submenu("Insert After", node, PendingInsertion::Action::InsertAfter);
    }

    // Add child (only for containers)
    if (node->can_have_children()) {
        ImGui::Separator();
        _render_insertion_submenu("Add Child", node, PendingInsertion::Action::AddChild);
    }

    ImGui::Separator();

    // Edit label
    static char label_buf[256] = "";
    if (ImGui::BeginMenu("Edit Label")) {
        strncpy(label_buf, node->label.c_str(), sizeof(label_buf) - 1);
        if (ImGui::InputText("##label", label_buf, sizeof(label_buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
            node->label = label_buf;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndMenu();
    }

    // Delete
    if (ImGui::MenuItem("Delete", nullptr, false, node->parent != nullptr)) {
        _model.remove(node);
    }
}

void EditorCanvas::_render_insertion_submenu(const char* label, LayoutNode* target, PendingInsertion::Action action) {
    if (ImGui::BeginMenu(label)) {
        _widget_tree.render_as_menu([this, target, action](const std::string& type) {
            _pending.target = target;
            _pending.action = action;
            _execute_pending_insertion(type);
            ImGui::CloseCurrentPopup();
        });
        ImGui::EndMenu();
    }
}

void EditorCanvas::_execute_pending_insertion(const std::string& widget_type) {
    if (!_pending.target && _pending.action != PendingInsertion::Action::AddChild) {
        // Adding to empty model
        _model.set_root(widget_type);
        _pending.action = PendingInsertion::Action::None;
        return;
    }

    switch (_pending.action) {
        case PendingInsertion::Action::InsertBefore:
            _model.insert_before(_pending.target, widget_type, LayoutPosition::NewLine);
            break;
        case PendingInsertion::Action::InsertAfter:
            _model.insert_after(_pending.target, widget_type, LayoutPosition::NewLine);
            break;
        case PendingInsertion::Action::InsertBeforeSameLine:
            _model.insert_before(_pending.target, widget_type, LayoutPosition::SameLine);
            break;
        case PendingInsertion::Action::InsertAfterSameLine:
            _model.insert_after(_pending.target, widget_type, LayoutPosition::SameLine);
            break;
        case PendingInsertion::Action::AddChild:
            _model.add_child(_pending.target, widget_type);
            break;
        default:
            break;
    }

    _pending.action = PendingInsertion::Action::None;
    _pending.target = nullptr;
}

} // namespace ymery::editor
