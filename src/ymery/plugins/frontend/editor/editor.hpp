#pragma once
#include "../../../frontend/widget.hpp"
#include "../../../plugin_manager.hpp"
#include "shared_model.hpp"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <functional>

namespace ymery::plugins::editor {

struct TreeTypeInfo {
    std::string name;
    std::string description;
    bool supports_children;
};

struct ValueTypeInfo {
    std::string name;
    bool is_container;
};

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
        widget->_init_types();
        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Editor::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        auto& model = SharedLayoutModel::instance();

        if (ImGui::CollapsingHeader("Data", ImGuiTreeNodeFlags_DefaultOpen)) {
            _render_data_section();
        }

        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Layout", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (model.empty()) {
                _render_empty_layout();
            } else {
                _render_layout();
            }
        }
        return Ok();
    }

private:
    std::vector<TreeTypeInfo> _tree_types;
    std::vector<ValueTypeInfo> _value_types;
    int _name_counter = 0;
    SelectionPath _current_path;

    void _init_types() {
        _tree_types = {
            {"data-tree", "Hierarchical data storage with metadata", true},
            {"simple-data-tree", "Basic hierarchical data storage", true},
            {"kernel", "System kernel for providers", false},
            {"waveform", "Waveform generator (sine, square, triangle)", false},
            {"filesystem", "File system browser", false},
            {"log-tree", "Log message tree", false},
        };

        _value_types = {
            {"string", false}, {"int", false}, {"float", false}, {"bool", false}, {"map", true},
        };
    }

    std::string _generate_default_name(const std::string& type) {
        ++_name_counter;
        std::string prefix = "item";
        if (type == "kernel") prefix = "kernel";
        else if (type == "waveform") prefix = "waveform";
        else if (type == "filesystem") prefix = "fs";
        else if (type == "data-tree") prefix = "data";
        else if (type == "simple-data-tree") prefix = "tree";
        else if (type == "string") prefix = "str";
        else if (type == "int") prefix = "num";
        else if (type == "float") prefix = "val";
        else if (type == "bool") prefix = "flag";
        else if (type == "map") prefix = "obj";
        return prefix + "-" + std::to_string(_name_counter);
    }

    void _render_data_section() {
        auto& model = SharedLayoutModel::instance();
        auto& entries = model.data_entries();

        for (size_t i = 0; i < entries.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            _render_data_entry(entries[i], i);
            ImGui::PopID();
        }

        std::string add_id = "+ Add Data Entry###add_data_" + _uid;
        if (ImGui::SmallButton(add_id.c_str())) {
            ImGui::OpenPopup("add_data_popup");
        }

        if (ImGui::BeginPopup("add_data_popup")) {
            for (const auto& tt : _tree_types) {
                if (ImGui::MenuItem(tt.name.c_str())) {
                    std::string name = _generate_default_name(tt.name);
                    model.add_data_entry(tt.name, name);
                    auto tree_res = _widget_factory->plugin_manager()->create_tree(tt.name, _dispatcher);
                    if (tree_res) {
                        model.set_live_tree(name, *tree_res);
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", tt.description.c_str());
                    ImGui::EndTooltip();
                }
            }
            ImGui::EndPopup();
        }
    }

    void _render_data_entry(DataEntry& entry, size_t idx) {
        auto& model = SharedLayoutModel::instance();
        bool can_have_children = SharedLayoutModel::data_type_supports_children(entry.type);

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
        if (!can_have_children && entry.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;

        std::string label = entry.name + " (" + entry.type + ")###data_" + std::to_string(idx);
        bool is_open = ImGui::TreeNodeEx(label.c_str(), flags);

        if (ImGui::BeginPopupContextItem()) {
            if (can_have_children && ImGui::BeginMenu("Add child")) {
                for (const auto& vt : _value_types) {
                    if (ImGui::MenuItem(vt.name.c_str())) {
                        std::string name = _generate_default_name(vt.name);
                        model.add_child_to_data_entry(idx, vt.name, name);
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Remove")) {
                model.remove_data_entry(idx);
                ImGui::EndPopup();
                if (is_open) ImGui::TreePop();
                return;
            }
            ImGui::EndPopup();
        }

        if (is_open) {
            for (size_t i = 0; i < entry.children.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                _render_data_child(entry, entry.children[i], i);
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    }

    void _render_data_child(DataEntry& parent, DataEntry& child, size_t idx) {
        bool can_have_children = (child.type == "map");
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
        if (!can_have_children && child.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;

        std::string label = child.name + " (" + child.type + ")###data_child_" + std::to_string(idx);
        bool is_open = ImGui::TreeNodeEx(label.c_str(), flags);

        if (ImGui::BeginPopupContextItem()) {
            if (can_have_children && ImGui::BeginMenu("Add child")) {
                for (const auto& vt : _value_types) {
                    if (ImGui::MenuItem(vt.name.c_str())) {
                        std::string name = _generate_default_name(vt.name);
                        SharedLayoutModel::add_child_to_data_entry_recursive(child, vt.name, name);
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Remove")) {
                SharedLayoutModel::remove_child_from_data_entry(parent, idx);
                ImGui::EndPopup();
                if (is_open) ImGui::TreePop();
                return;
            }
            ImGui::EndPopup();
        }

        if (is_open) {
            for (size_t i = 0; i < child.children.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                _render_data_child(child, child.children[i], i);
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    }

    void _render_empty_layout() {
        auto& model = SharedLayoutModel::instance();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
        if (ImGui::Button("[undefined]")) {}
        ImGui::PopStyleColor();

        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            ImGui::OpenPopup("set_root_widget");
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("WIDGET_TYPE")) {
                std::string type(static_cast<const char*>(payload->Data));
                model.set_root(type);
            }
            ImGui::EndDragDropTarget();
        }

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
        if (depth > 0) ImGui::Indent(20.0f);

        std::string display = is_container ? "[" + type + "] " + label : type + ": " + label;
        bool is_selected = (_current_path == model.selection());
        if (is_selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
        }

        std::string button_label = display + "##" + uid;
        if (ImGui::Button(button_label.c_str())) {
            model.select(_current_path);
        }

        if (is_selected) ImGui::PopStyleColor();

        std::string popup_id = "ctx_" + uid;
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            ImGui::OpenPopup(popup_id.c_str());
        }

        if (is_container && ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("WIDGET_TYPE")) {
                std::string new_type(static_cast<const char*>(payload->Data));
                model.add_child(_current_path, new_type);
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopup(popup_id.c_str())) {
            _render_context_menu(widget, type, is_container);
            ImGui::EndPopup();
        }

        for (size_t i = 0; i < body.size(); ++i) {
            _current_path.push_back(i);
            _render_widget(body[i], depth + 1);
            _current_path.pop_back();
        }

        if (depth > 0) ImGui::Unindent(20.0f);
        ImGui::PopID();
    }

    void _render_context_menu(const Value& widget, const std::string& type, bool is_container) {
        auto& model = SharedLayoutModel::instance();

        ImGui::Text("%s", type.c_str());
        ImGui::Separator();

        if (ImGui::BeginMenu("Change Type")) {
            _render_widget_menu([this, &model](const std::string& new_type) {
                model.change_type(_current_path, new_type);
                ImGui::CloseCurrentPopup();
            });
            ImGui::EndMenu();
        }

        ImGui::Separator();

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

        if (is_container) {
            ImGui::Separator();
            if (ImGui::BeginMenu("Add Child")) {
                _render_widget_menu([this, &model](const std::string& new_type) {
                    model.add_child(_current_path, new_type, false);
                    ImGui::CloseCurrentPopup();
                });
                ImGui::EndMenu();
            }
        }

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
        if (ImGui::MenuItem("Delete")) {
            model.remove(_current_path);
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

} // namespace ymery::plugins::editor
