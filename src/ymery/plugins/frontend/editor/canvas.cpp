// editor-canvas widget plugin - layout editor with drag-drop and context menus
// Matches ymery-editor exactly
#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"
#include "../../../types.hpp"
#include "shared_model.hpp"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <functional>

namespace ymery::plugins {

class EditorCanvas : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<EditorCanvas>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("EditorCanvas::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        auto& model = SharedLayoutModel::instance();

        if (model.empty()) {
            _render_empty_state();
        } else {
            _render_layout();
        }

        // Process pending drops
        _process_pending_drops();

        // Context popup
        if (_context_popup_open) {
            ImGui::OpenPopup("##context_popup");
            _context_popup_open = false;
        }

        if (ImGui::BeginPopup("##context_popup")) {
            auto* node = model.find_by_id(_context_node_id);
            if (node) {
                _render_context_menu(node);
            }
            ImGui::EndPopup();
        }

        return Ok();
    }

private:
    int _context_node_id = -1;
    bool _context_popup_open = false;

    struct PendingDrop {
        int target_id;
        std::string widget_type;
        bool add_as_child;
    };
    std::vector<PendingDrop> _pending_drops;

    void _render_empty_state() {
        auto& model = SharedLayoutModel::instance();

        ImGui::Text("Layout:");
        ImGui::Separator();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
        if (ImGui::Button("[undefined]")) {
            // Nothing to select
        }
        ImGui::PopStyleColor();

        // Right-click to set root
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            ImGui::OpenPopup("set_root_widget");
        }

        // Drop to set root
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("WIDGET_TYPE")) {
                std::string type(static_cast<const char*>(payload->Data));
                model.set_root(type);
                spdlog::info("Added root widget: {}", type);
            }
            ImGui::EndDragDropTarget();
        }

        // Popup for root selection
        if (ImGui::BeginPopup("set_root_widget")) {
            ImGui::Text("Select widget type:");
            ImGui::Separator();
            _render_widget_tree_menu([&model](const std::string& type) {
                model.set_root(type);
                ImGui::CloseCurrentPopup();
            });
            ImGui::EndPopup();
        }
    }

    void _render_layout() {
        auto& model = SharedLayoutModel::instance();

        ImGui::Text("Layout:");
        ImGui::Separator();

        _render_node(model.root());
    }

    void _render_node(LayoutNode* node, int depth = 0, bool same_line = false) {
        if (!node) return;
        auto& model = SharedLayoutModel::instance();

        ImGui::PushID(node->id);

        if (same_line) {
            ImGui::SameLine();
        } else if (depth > 0) {
            ImGui::Indent(20.0f);
        }

        // Display format: [type] label for containers, type: label for leaves
        std::string display;
        if (node->is_container()) {
            display = "[" + node->widget_type + "] " + node->label;
        } else {
            display = node->widget_type + ": " + node->label;
        }

        // Selection highlight
        bool is_selected = (model.selected() == node);
        if (is_selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
        }

        if (ImGui::Button(display.c_str())) {
            model.select(node);
        }

        if (is_selected) {
            ImGui::PopStyleColor();
        }

        // Drag source
        if (node->parent && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("LAYOUT_NODE", &node->id, sizeof(int));
            ImGui::Text("Move: %s", node->widget_type.c_str());
            ImGui::EndDragDropSource();
        }

        // Drop target
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("WIDGET_TYPE")) {
                std::string type(static_cast<const char*>(payload->Data));
                PendingDrop drop;
                drop.target_id = node->id;
                drop.widget_type = type;
                drop.add_as_child = node->can_have_children();
                _pending_drops.push_back(drop);
            }
            ImGui::EndDragDropTarget();
        }

        // Right-click context menu
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            _context_node_id = node->id;
            _context_popup_open = true;
        }

        // Render children
        bool first_child = true;
        for (auto& child : node->children) {
            bool child_same_line = (child->position == LayoutPosition::SameLine) && !first_child;
            _render_node(child.get(), depth + 1, child_same_line);
            first_child = false;
        }

        if (!same_line && depth > 0) {
            ImGui::Unindent(20.0f);
        }

        ImGui::PopID();
    }

    void _render_context_menu(LayoutNode* node) {
        auto& model = SharedLayoutModel::instance();

        ImGui::Text("%s", node->widget_type.c_str());
        ImGui::Separator();

        // Edit Properties (collapsible)
        if (ImGui::CollapsingHeader("Edit Properties")) {
            _render_properties_editor(node);
        }

        ImGui::Separator();

        // Change Type
        if (ImGui::BeginMenu("Change Type")) {
            _render_widget_tree_menu([&model, node](const std::string& type) {
                model.change_type(node, type);
                ImGui::CloseCurrentPopup();
            });
            ImGui::EndMenu();
        }

        ImGui::Separator();

        // Insert options
        if (node->parent) {
            // Has parent - normal insertion
            if (ImGui::BeginMenu("Insert Before (same line)")) {
                _render_widget_tree_menu([&model, node](const std::string& type) {
                    model.insert_before(node, type, LayoutPosition::SameLine);
                    ImGui::CloseCurrentPopup();
                });
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Insert After (same line)")) {
                _render_widget_tree_menu([&model, node](const std::string& type) {
                    model.insert_after(node, type, LayoutPosition::SameLine);
                    ImGui::CloseCurrentPopup();
                });
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Insert Before")) {
                _render_widget_tree_menu([&model, node](const std::string& type) {
                    model.insert_before(node, type, LayoutPosition::NewLine);
                    ImGui::CloseCurrentPopup();
                });
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Insert After")) {
                _render_widget_tree_menu([&model, node](const std::string& type) {
                    model.insert_after(node, type, LayoutPosition::NewLine);
                    ImGui::CloseCurrentPopup();
                });
                ImGui::EndMenu();
            }
        } else {
            // Root node - wrap in column first
            if (ImGui::BeginMenu("Insert Before (same line)")) {
                _render_widget_tree_menu([&model, node](const std::string& type) {
                    model.wrap_root_in_column();
                    if (model.root() && !model.root()->children.empty()) {
                        model.insert_before(model.root()->children[0].get(), type, LayoutPosition::SameLine);
                    }
                    ImGui::CloseCurrentPopup();
                });
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Insert After (same line)")) {
                _render_widget_tree_menu([&model, node](const std::string& type) {
                    model.wrap_root_in_column();
                    if (model.root() && !model.root()->children.empty()) {
                        model.insert_after(model.root()->children[0].get(), type, LayoutPosition::SameLine);
                    }
                    ImGui::CloseCurrentPopup();
                });
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Insert Before")) {
                _render_widget_tree_menu([&model, node](const std::string& type) {
                    model.wrap_root_in_column();
                    if (model.root() && !model.root()->children.empty()) {
                        model.insert_before(model.root()->children[0].get(), type, LayoutPosition::NewLine);
                    }
                    ImGui::CloseCurrentPopup();
                });
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Insert After")) {
                _render_widget_tree_menu([&model, node](const std::string& type) {
                    model.wrap_root_in_column();
                    if (model.root() && !model.root()->children.empty()) {
                        model.insert_after(model.root()->children[0].get(), type, LayoutPosition::NewLine);
                    }
                    ImGui::CloseCurrentPopup();
                });
                ImGui::EndMenu();
            }
        }

        // Add Child (containers only)
        if (node->can_have_children()) {
            ImGui::Separator();
            if (ImGui::BeginMenu("Add Child")) {
                _render_widget_tree_menu([&model, node](const std::string& type) {
                    model.add_child(node, type);
                    ImGui::CloseCurrentPopup();
                });
                ImGui::EndMenu();
            }
        }

        // Movement options (non-root only)
        if (node->parent) {
            ImGui::Separator();

            if (ImGui::MenuItem("Move Up", nullptr, false, model.can_move_up(node))) {
                model.move_up(node);
            }
            if (ImGui::MenuItem("Move Down", nullptr, false, model.can_move_down(node))) {
                model.move_down(node);
            }

            ImGui::Separator();

            bool is_same_line = (node->position == LayoutPosition::SameLine);
            bool is_first = false;
            auto& siblings = node->parent->children;
            for (size_t i = 0; i < siblings.size(); ++i) {
                if (siblings[i].get() == node) {
                    is_first = (i == 0);
                    break;
                }
            }

            if (ImGui::MenuItem("Move Left (same line)", nullptr, false, !is_same_line && !is_first)) {
                model.set_same_line(node, true);
            }
            if (ImGui::MenuItem("Move Right (new line)", nullptr, false, is_same_line)) {
                model.set_same_line(node, false);
            }
        }

        ImGui::Separator();

        // Delete
        if (ImGui::MenuItem("Delete")) {
            model.remove(node);
        }
    }

    void _render_properties_editor(LayoutNode* node) {
        static std::map<std::string, std::string> buffers;

        std::string key = std::to_string(node->id) + "_label";
        if (buffers.find(key) == buffers.end()) {
            buffers[key] = node->label;
        }

        char buf[256];
        strncpy(buf, buffers[key].c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';

        ImGui::Text("label:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        if (ImGui::InputText("##label", buf, sizeof(buf))) {
            buffers[key] = buf;
            node->label = buf;
        }
    }

    void _render_widget_tree_menu(const std::function<void(const std::string&)>& on_select) {
        static const std::vector<std::pair<std::string, std::vector<std::string>>> categories = {
            {"Containers", {"window", "row", "column", "group", "child"}},
            {"Collapsible", {"tree-node", "collapsing-header", "tab-bar", "tab-item"}},
            {"Inputs", {"button", "input-text", "input-int", "slider-int", "slider-float",
                        "checkbox", "combo", "color-edit", "toggle", "knob"}},
            {"Display", {"text", "separator", "spacing", "markdown"}},
            {"Popups", {"popup", "popup-modal", "tooltip"}},
            {"Visualization", {"implot", "implot-layer", "plot3d", "gizmo"}},
            {"Advanced", {"coolbar", "spinner"}}
        };

        for (const auto& [cat, widgets] : categories) {
            if (ImGui::BeginMenu(cat.c_str())) {
                for (const auto& w : widgets) {
                    if (ImGui::MenuItem(w.c_str())) {
                        on_select(w);
                    }
                }
                ImGui::EndMenu();
            }
        }
    }

    void _process_pending_drops() {
        auto& model = SharedLayoutModel::instance();

        for (const auto& drop : _pending_drops) {
            auto* target = model.find_by_id(drop.target_id);
            if (!target) continue;

            if (drop.add_as_child) {
                model.add_child(target, drop.widget_type);
                spdlog::info("Added {} as child of {}", drop.widget_type, target->widget_type);
            } else if (target->parent) {
                model.insert_after(target, drop.widget_type);
                spdlog::info("Inserted {} after {}", drop.widget_type, target->widget_type);
            } else {
                // Root - wrap and add
                model.wrap_root_in_column();
                if (model.root() && !model.root()->children.empty()) {
                    model.insert_after(model.root()->children[0].get(), drop.widget_type);
                }
                spdlog::info("Wrapped root and added {}", drop.widget_type);
            }
        }
        _pending_drops.clear();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "editor-canvas"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::EditorCanvas::create(wf, d, ns, db);
}
