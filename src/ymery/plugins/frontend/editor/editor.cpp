// editor widget plugin - layout editor with drag-drop and context menus
// Builds YAML structure directly
#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"
#include "../../../types.hpp"
#include "shared_model.hpp"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <functional>

namespace ymery::plugins {

class Editor : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Editor>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Editor::create failed", res);
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

        return Ok();
    }

private:
    SelectionPath _current_path;  // Path being rendered

    void _render_empty_state() {
        auto& model = SharedLayoutModel::instance();

        ImGui::Text("Layout:");
        ImGui::Separator();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
        if (ImGui::Button("[undefined]")) {
            // Nothing
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
            _render_widget_menu([&model](const std::string& type) {
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

        _current_path.clear();
        _render_widget(model.root(), 0);
    }

    void _render_widget(const Value& widget, int depth) {
        auto& model = SharedLayoutModel::instance();

        std::string type = SharedLayoutModel::get_widget_type(widget);
        std::string label = SharedLayoutModel::get_label(widget);
        std::string uid = SharedLayoutModel::get_uid(widget);
        if (label.empty()) label = type;
        if (uid.empty()) uid = "no-uid";

        bool is_container = SharedLayoutModel::is_container(type);
        List body = SharedLayoutModel::get_body(widget);

        ImGui::PushID(uid.c_str());

        if (depth > 0) {
            ImGui::Indent(20.0f);
        }

        // Display format
        std::string display;
        if (is_container) {
            display = "[" + type + "] " + label;
        } else {
            display = type + ": " + label;
        }

        // Selection highlight
        bool is_selected = (_current_path == model.selection());
        if (is_selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
        }

        // Use ##uid for unique button ID
        std::string button_label = display + "##" + uid;
        if (ImGui::Button(button_label.c_str())) {
            model.select(_current_path);
        }

        if (is_selected) {
            ImGui::PopStyleColor();
        }

        // Right-click context menu
        std::string popup_id = "ctx_" + uid;
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            ImGui::OpenPopup(popup_id.c_str());
        }

        if (ImGui::BeginPopup(popup_id.c_str())) {
            _render_context_menu(widget, type, is_container);
            ImGui::EndPopup();
        }

        // Drop target for adding children
        if (is_container && ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("WIDGET_TYPE")) {
                std::string new_type(static_cast<const char*>(payload->Data));
                model.add_child(_current_path, new_type);
                spdlog::info("Added {} as child of {}", new_type, type);
            }
            ImGui::EndDragDropTarget();
        }

        // Render children
        for (size_t i = 0; i < body.size(); ++i) {
            _current_path.push_back(i);
            _render_widget(body[i], depth + 1);
            _current_path.pop_back();
        }

        if (depth > 0) {
            ImGui::Unindent(20.0f);
        }

        ImGui::PopID();
    }

    void _render_context_menu(const Value& widget, const std::string& type, bool is_container) {
        auto& model = SharedLayoutModel::instance();

        ImGui::Text("%s", type.c_str());
        ImGui::Separator();

        // Edit Properties
        if (ImGui::CollapsingHeader("Edit Properties")) {
            _render_properties_editor(widget);
        }

        ImGui::Separator();

        // Change Type
        if (ImGui::BeginMenu("Change Type")) {
            _render_widget_menu([this, &model](const std::string& new_type) {
                model.change_type(_current_path, new_type);
                ImGui::CloseCurrentPopup();
            });
            ImGui::EndMenu();
        }

        ImGui::Separator();

        // Insert options - same line variants first
        if (ImGui::BeginMenu("Insert Before (same line)")) {
            _render_widget_menu([this, &model](const std::string& new_type) {
                model.insert_before(_current_path, new_type, true);
                ImGui::CloseCurrentPopup();
            });
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Insert After (same line)")) {
            _render_widget_menu([this, &model](const std::string& new_type) {
                model.insert_after(_current_path, new_type, true);
                ImGui::CloseCurrentPopup();
            });
            ImGui::EndMenu();
        }

        ImGui::Separator();

        // Insert options - new line variants
        if (ImGui::BeginMenu("Insert Before")) {
            _render_widget_menu([this, &model](const std::string& new_type) {
                model.insert_before(_current_path, new_type, false);
                ImGui::CloseCurrentPopup();
            });
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Insert After")) {
            _render_widget_menu([this, &model](const std::string& new_type) {
                model.insert_after(_current_path, new_type, false);
                ImGui::CloseCurrentPopup();
            });
            ImGui::EndMenu();
        }

        // Add Child (containers only)
        if (is_container) {
            ImGui::Separator();
            if (ImGui::BeginMenu("Add Child (same line)")) {
                _render_widget_menu([this, &model](const std::string& new_type) {
                    model.add_child(_current_path, new_type, true);
                    ImGui::CloseCurrentPopup();
                });
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Add Child")) {
                _render_widget_menu([this, &model](const std::string& new_type) {
                    model.add_child(_current_path, new_type, false);
                    ImGui::CloseCurrentPopup();
                });
                ImGui::EndMenu();
            }
        }

        // Movement options (non-root only)
        if (!_current_path.empty()) {
            ImGui::Separator();

            if (ImGui::MenuItem("Move Up", nullptr, false, model.can_move_up(_current_path))) {
                model.move_up(_current_path);
            }
            if (ImGui::MenuItem("Move Down", nullptr, false, model.can_move_down(_current_path))) {
                model.move_down(_current_path);
            }
        }

        ImGui::Separator();

        // Delete
        if (ImGui::MenuItem("Delete")) {
            model.remove(_current_path);
        }
    }

    void _render_properties_editor(const Value& widget) {
        auto& model = SharedLayoutModel::instance();
        std::string label = SharedLayoutModel::get_label(widget);

        static std::map<std::string, std::string> buffers;
        std::string key = "label_" + std::to_string(_current_path.size());
        for (size_t i : _current_path) key += "_" + std::to_string(i);

        if (buffers.find(key) == buffers.end()) {
            buffers[key] = label;
        }

        char buf[256];
        strncpy(buf, buffers[key].c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';

        ImGui::Text("label:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        if (ImGui::InputText("##label", buf, sizeof(buf))) {
            buffers[key] = buf;
            model.set_label_at(_current_path, buf);
        }
    }

    void _render_widget_menu(const std::function<void(const std::string&)>& on_select) {
        static const std::vector<std::pair<std::string, std::vector<std::string>>> categories = {
            {"Containers", {"window", "row", "column", "group", "child"}},
            {"Collapsible", {"tree-node", "collapsing-header", "tab-bar", "tab-item"}},
            {"Inputs", {"button", "input-text", "input-int", "slider-int", "slider-float",
                        "checkbox", "combo", "color-edit", "toggle", "knob"}},
            {"Display", {"text", "separator", "spacing", "markdown"}},
            {"Popups", {"popup", "popup-modal", "tooltip"}},
            {"Visualization", {"implot", "implot-layer", "implot-group", "plot3d", "gizmo"}},
            {"Advanced", {"coolbar", "spinner", "spinners-demo"}}
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
};

} // namespace ymery::plugins

extern "C" const char* name() { return "editor"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::Editor::create(wf, d, ns, db);
}
