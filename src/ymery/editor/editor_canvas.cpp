#include "editor_canvas.hpp"
#include <imgui.h>
#include <ytrace/ytrace.hpp>
#include <cstring>

namespace ymery::editor {

EditorCanvas::EditorCanvas(LayoutModel& model, WidgetTree& widget_tree, ymery::PluginManagerPtr plugin_manager)
    : _model(model), _widget_tree(widget_tree), _plugin_manager(plugin_manager) {}

void EditorCanvas::render() {
    if (_model.empty()) {
        _render_empty_state();
    } else {
        _render_layout();
    }

    // Process any pending drops after rendering is complete
    // This avoids iterator invalidation during tree traversal
    _process_pending_drops();

    // Context popup (replaces menu - stays open)
    if (_context_popup_open) {
        ImGui::OpenPopup("##context_popup");
        _context_popup_open = false;
    }

    if (ImGui::BeginPopup("##context_popup")) {
        auto* node = _model.find_by_id(_context_popup_node_id);
        if (node) {
            _render_context_menu(node);
        }
        ImGui::EndPopup();
    }
}

void EditorCanvas::_render_empty_state() {
    // Show "undefined" placeholder as root of the tree
    ImGui::Text("Layout:");
    ImGui::Separator();

    // Render placeholder as a button like normal nodes
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
    if (ImGui::Button("[undefined]")) {
        // Click selects (nothing to select yet)
    }
    ImGui::PopStyleColor();

    // Right-click to change type (set root)
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup("set_root_widget");
    }

    // Also accept drops
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("WIDGET_TYPE")) {
            std::string type(static_cast<const char*>(payload->Data));
            _model.set_root(type);
            ydebug("Added root widget: {}", type);
        }
        ImGui::EndDragDropTarget();
    }

    // Popup with widget tree
    if (ImGui::BeginPopup("set_root_widget")) {
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

void EditorCanvas::_render_node(LayoutNode* node, int depth, bool same_line) {
    if (!node) return;

    ImGui::PushID(node->id);

    // Handle same-line positioning
    if (same_line) {
        ImGui::SameLine();
    } else if (depth > 0) {
        // Only indent if not same line
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

    // Render as button - auto-size, not full width
    if (ImGui::Button(display.c_str())) {
        _model.select(node);
    }

    if (is_selected) {
        ImGui::PopStyleColor();
    }

    // Drag source for reordering (only non-root nodes)
    if (node->parent && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        ImGui::SetDragDropPayload("LAYOUT_NODE", &node->id, sizeof(int));
        ImGui::Text("Move: %s", node->widget_type.c_str());
        ImGui::EndDragDropSource();
    }

    // Drag and drop target
    _handle_drop(node);

    // Right-click opens context popup
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        _context_popup_node_id = node->id;
        _context_popup_open = true;
    }

    // Render children
    bool first_child = true;
    for (auto& child : node->children) {
        bool child_same_line = (child->position == LayoutPosition::SameLine) && !first_child;
        _render_node(child.get(), depth + 1, child_same_line);
        first_child = false;
    }

    // Unindent only if we indented (not same line)
    if (!same_line && depth > 0) {
        ImGui::Unindent(20.0f);
    }

    ImGui::PopID();
}

void EditorCanvas::_handle_drop(LayoutNode* node) {
    if (ImGui::BeginDragDropTarget()) {
        // Handle new widget drops from widget tree
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("WIDGET_TYPE")) {
            std::string type(static_cast<const char*>(payload->Data));

            // Queue the drop action - don't execute immediately to avoid iterator invalidation
            PendingDrop drop;
            drop.target_id = node->id;
            drop.widget_type = type;
            drop.add_as_child = node->can_have_children();
            _pending_drops.push_back(drop);
        }

        // Handle node moves (reordering)
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("LAYOUT_NODE")) {
            int dragged_id = *static_cast<const int*>(payload->Data);

            // Queue the move action
            PendingMove move;
            move.node_id = dragged_id;
            move.target_id = node->id;
            move.into_container = node->can_have_children();
            _pending_moves.push_back(move);
        }

        ImGui::EndDragDropTarget();
    }
}

void EditorCanvas::_process_pending_drops() {
    // Process new widget drops
    for (const auto& drop : _pending_drops) {
        auto* target = _model.find_by_id(drop.target_id);
        if (!target) continue;

        if (drop.add_as_child) {
            _model.add_child(target, drop.widget_type);
            ydebug("Added {} as child of {}", drop.widget_type, target->widget_type);
        } else {
            // If target has no parent (it's root), wrap in column first
            if (!target->parent) {
                _wrap_root_and_add(drop.widget_type, false);
                ydebug("Wrapped root and added {}", drop.widget_type);
            } else {
                _model.insert_after(target, drop.widget_type);
                ydebug("Inserted {} after {}", drop.widget_type, target->widget_type);
            }
        }
    }
    _pending_drops.clear();

    // Process node moves (reordering)
    for (const auto& move : _pending_moves) {
        auto* node = _model.find_by_id(move.node_id);
        auto* target = _model.find_by_id(move.target_id);
        if (!node || !target || node == target) continue;

        if (move.into_container) {
            // Move into container as last child
            _model.move_node(node, target);
            ydebug("Moved {} into {}", node->widget_type, target->widget_type);
        } else if (target->parent) {
            // Move to same parent, insert after target
            auto* parent = target->parent;
            // Find target's position
            int pos = 0;
            for (size_t i = 0; i < parent->children.size(); ++i) {
                if (parent->children[i].get() == target) {
                    pos = static_cast<int>(i) + 1;
                    break;
                }
            }
            _model.move_node(node, parent, pos);
            ydebug("Moved {} after {}", node->widget_type, target->widget_type);
        }
    }
    _pending_moves.clear();
}

void EditorCanvas::_render_context_menu(LayoutNode* node) {
    ImGui::Text("%s", node->widget_type.c_str());
    ImGui::Separator();

    // Edit Properties - collapsible section in same popup
    if (ImGui::CollapsingHeader("Edit Properties")) {
        _render_properties_menu(node);
    }

    ImGui::Separator();

    // Change widget type
    _render_insertion_submenu("Change Type", node, PendingInsertion::Action::ChangeType);

    ImGui::Separator();

    // Insert options
    if (node->parent) {
        // Node has parent - normal insertion among siblings
        _render_insertion_submenu("Insert Before (same line)", node, PendingInsertion::Action::InsertBeforeSameLine);
        _render_insertion_submenu("Insert After (same line)", node, PendingInsertion::Action::InsertAfterSameLine);
        ImGui::Separator();
        _render_insertion_submenu("Insert Before", node, PendingInsertion::Action::InsertBefore);
        _render_insertion_submenu("Insert After", node, PendingInsertion::Action::InsertAfter);
    } else {
        // Root node - offer to wrap in column and insert
        _render_insertion_submenu("Insert Before (same line)", node, PendingInsertion::Action::WrapInsertBeforeSameLine);
        _render_insertion_submenu("Insert After (same line)", node, PendingInsertion::Action::WrapInsertAfterSameLine);
        ImGui::Separator();
        _render_insertion_submenu("Insert Before", node, PendingInsertion::Action::WrapInsertBefore);
        _render_insertion_submenu("Insert After", node, PendingInsertion::Action::WrapInsertAfter);
    }

    // Add child (only for containers)
    if (node->can_have_children()) {
        ImGui::Separator();
        _render_insertion_submenu("Add Child", node, PendingInsertion::Action::AddChild);
    }

    // Movement options (only for non-root nodes)
    if (node->parent) {
        ImGui::Separator();

        // Move up/down (reorder among siblings)
        if (ImGui::MenuItem("Move Up", nullptr, false, _model.can_move_up(node))) {
            _model.move_up(node);
        }
        if (ImGui::MenuItem("Move Down", nullptr, false, _model.can_move_down(node))) {
            _model.move_down(node);
        }

        ImGui::Separator();

        // Move left/right (same-line positioning)
        bool is_same_line = (node->position == LayoutPosition::SameLine);
        bool is_first_child = false;

        // Check if this is the first child (can't be same-line if first)
        auto& siblings = node->parent->children;
        for (size_t i = 0; i < siblings.size(); ++i) {
            if (siblings[i].get() == node) {
                is_first_child = (i == 0);
                break;
            }
        }

        if (ImGui::MenuItem("Move Left (same line)", nullptr, false, !is_same_line && !is_first_child)) {
            _model.set_same_line(node, true);
        }
        if (ImGui::MenuItem("Move Right (new line)", nullptr, false, is_same_line)) {
            _model.set_same_line(node, false);
        }
    }

    ImGui::Separator();

    // Delete
    if (ImGui::MenuItem("Delete", nullptr, false, node->parent != nullptr)) {
        _model.remove(node);
    }
}

void EditorCanvas::_render_properties_menu(LayoutNode* node) {
    // Static buffers for input fields (indexed by property name)
    static std::map<std::string, std::string> prop_buffers;

    // Get metadata from plugin manager
    auto meta = _get_widget_meta(node->widget_type);

    // Always show label first
    {
        std::string key = std::to_string(node->id) + "_label";
        if (prop_buffers.find(key) == prop_buffers.end()) {
            prop_buffers[key] = node->label;
        }

        char buf[256];
        strncpy(buf, prop_buffers[key].c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';

        ImGui::Text("label:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        if (ImGui::InputText("##label", buf, sizeof(buf))) {
            prop_buffers[key] = buf;
            node->label = buf;
        }
    }

    // Get properties list from meta
    auto props_it = meta.find("properties");
    if (props_it != meta.end()) {
        if (auto props_list = ymery::get_as<ymery::List>(props_it->second)) {
            ImGui::Separator();
            ImGui::Text("Properties:");

            for (const auto& prop_val : *props_list) {
                if (auto prop_dict = ymery::get_as<ymery::Dict>(prop_val)) {
                    std::string prop_name;
                    std::string prop_desc;

                    if (auto name_it = prop_dict->find("name"); name_it != prop_dict->end()) {
                        if (auto n = ymery::get_as<std::string>(name_it->second)) {
                            prop_name = *n;
                        }
                    }
                    if (auto desc_it = prop_dict->find("description"); desc_it != prop_dict->end()) {
                        if (auto d = ymery::get_as<std::string>(desc_it->second)) {
                            prop_desc = *d;
                        }
                    }

                    if (prop_name.empty() || prop_name == "label") continue;

                    std::string key = std::to_string(node->id) + "_" + prop_name;

                    if (prop_buffers.find(key) == prop_buffers.end()) {
                        auto it = node->properties.find(prop_name);
                        prop_buffers[key] = (it != node->properties.end()) ? it->second : "";
                    }

                    char buf[256];
                    strncpy(buf, prop_buffers[key].c_str(), sizeof(buf) - 1);
                    buf[sizeof(buf) - 1] = '\0';

                    ImGui::Text("  %s:", prop_name.c_str());
                    if (!prop_desc.empty() && ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", prop_desc.c_str());
                    }
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(120);

                    std::string input_id = "##" + prop_name;
                    if (ImGui::InputText(input_id.c_str(), buf, sizeof(buf))) {
                        prop_buffers[key] = buf;
                        if (strlen(buf) > 0) {
                            node->properties[prop_name] = buf;
                        } else {
                            node->properties.erase(prop_name);
                        }
                    }
                }
            }
        }
    }

    // Get events list from meta
    auto events_it = meta.find("events");
    if (events_it != meta.end()) {
        if (auto events_list = ymery::get_as<ymery::List>(events_it->second)) {
            if (!events_list->empty()) {
                ImGui::Separator();
                ImGui::Text("Event Handlers:");

                for (const auto& event_val : *events_list) {
                    if (auto event_name = ymery::get_as<std::string>(event_val)) {
                        std::string key = std::to_string(node->id) + "_event_" + *event_name;

                        if (prop_buffers.find(key) == prop_buffers.end()) {
                            auto it = node->properties.find(*event_name);
                            prop_buffers[key] = (it != node->properties.end()) ? it->second : "";
                        }

                        char buf[256];
                        strncpy(buf, prop_buffers[key].c_str(), sizeof(buf) - 1);
                        buf[sizeof(buf) - 1] = '\0';

                        ImGui::Text("  %s:", event_name->c_str());
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(120);

                        std::string input_id = "##event_" + *event_name;
                        if (ImGui::InputText(input_id.c_str(), buf, sizeof(buf))) {
                            prop_buffers[key] = buf;
                            if (strlen(buf) > 0) {
                                node->properties[*event_name] = buf;
                            } else {
                                node->properties.erase(*event_name);
                            }
                        }
                    }
                }
            }
        }
    }
}

ymery::Dict EditorCanvas::_get_widget_meta(const std::string& widget_type) {
    if (!_plugin_manager) {
        return ymery::Dict{};
    }

    // Get metadata from /widget/<type>/meta
    ymery::DataPath path = ymery::DataPath::parse("/widget/" + widget_type + "/meta");
    auto res = _plugin_manager->get_metadata(path);
    if (res) {
        return *res;
    }
    return ymery::Dict{};
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
        // Wrap root in column first, then insert
        case PendingInsertion::Action::WrapInsertBefore:
            _wrap_root_and_add(widget_type, true);
            break;
        case PendingInsertion::Action::WrapInsertAfter:
            _wrap_root_and_add(widget_type, false);
            break;
        case PendingInsertion::Action::WrapInsertBeforeSameLine:
            _model.wrap_root_in_column();
            // After wrapping, the old root is now first child of column
            if (_model.root() && !_model.root()->children.empty()) {
                _model.insert_before(_model.root()->children[0].get(), widget_type, LayoutPosition::SameLine);
            }
            break;
        case PendingInsertion::Action::WrapInsertAfterSameLine:
            _model.wrap_root_in_column();
            if (_model.root() && !_model.root()->children.empty()) {
                _model.insert_after(_model.root()->children[0].get(), widget_type, LayoutPosition::SameLine);
            }
            break;
        case PendingInsertion::Action::ChangeType:
            _model.change_type(_pending.target, widget_type);
            break;
        default:
            break;
    }

    _pending.action = PendingInsertion::Action::None;
    _pending.target = nullptr;
}

void EditorCanvas::_wrap_root_and_add(const std::string& widget_type, bool before) {
    _model.wrap_root_in_column();

    // After wrapping, the old root is now first child of column
    if (_model.root() && !_model.root()->children.empty()) {
        auto* old_root = _model.root()->children[0].get();
        if (before) {
            _model.insert_before(old_root, widget_type, LayoutPosition::NewLine);
        } else {
            _model.insert_after(old_root, widget_type, LayoutPosition::NewLine);
        }
    }
}

} // namespace ymery::editor
