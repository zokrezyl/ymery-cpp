#pragma once
#include "../../../frontend/widget.hpp"
#include "../../../data_bag.hpp"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <vector>
#include <map>

namespace ymery::plugins::editor {

struct DataEditorTreeTypeInfo {
    std::string name;
    std::string description;
    bool supports_children;
};

struct DataEditorValueTypeInfo {
    std::string name;
    std::string default_value;
    bool is_container;
};

struct DataEditorEntry {
    std::string name;
    std::string type;
    Dict metadata;
    std::vector<DataEditorEntry> children;
};

class DataEditor : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<DataEditor>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;
        widget->_init_types();
        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("DataEditor::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        ImGui::Text("Data Editor");
        ImGui::Separator();
        ImGui::Spacing();

        for (size_t i = 0; i < _data_entries.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            _render_entry(_data_entries[i], i);
            ImGui::PopID();
        }

        ImGui::Spacing();
        std::string add_btn_id = "+ Add Data Entry###add_root_" + _uid;
        if (ImGui::Button(add_btn_id.c_str())) {
            ImGui::OpenPopup("add_root_popup");
        }

        if (ImGui::BeginPopup("add_root_popup")) {
            for (const auto& tt : _tree_types) {
                if (ImGui::MenuItem(tt.name.c_str())) {
                    std::string name = _generate_default_name(tt.name);
                    _add_root_entry(tt.name, name);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", tt.description.c_str());
                    ImGui::EndTooltip();
                }
            }
            ImGui::EndPopup();
        }

        if (!_data_entries.empty()) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Generated YAML:");
            std::string yaml = _generate_yaml();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.6f, 1.0f));
            ImGui::TextWrapped("%s", yaml.c_str());
            ImGui::PopStyleColor();
        }
        return Ok();
    }

private:
    std::vector<DataEditorTreeTypeInfo> _tree_types;
    std::vector<DataEditorValueTypeInfo> _value_types;
    std::vector<DataEditorEntry> _data_entries;
    int _name_counter = 0;

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
            {"string", "", false}, {"int", "0", false}, {"float", "0.0", false},
            {"bool", "false", false}, {"map", "", true},
        };
    }

    std::string _generate_default_name(const std::string& type) {
        ++_name_counter;
        std::string prefix = "data";
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

    void _add_root_entry(const std::string& type, const std::string& name) {
        DataEditorEntry entry;
        entry.name = name;
        entry.type = type;
        _data_entries.push_back(entry);

        if (_data_bag) {
            Dict child_spec;
            child_spec["name"] = name;
            Dict metadata;
            metadata["type"] = type;
            child_spec["metadata"] = metadata;
            _data_bag->add_child(child_spec);
        }
    }

    bool _type_supports_children(const std::string& type) {
        for (const auto& tt : _tree_types) {
            if (tt.name == type) return tt.supports_children;
        }
        return type == "map";
    }

    void _render_entry(DataEditorEntry& entry, size_t idx) {
        bool can_have_children = _type_supports_children(entry.type);
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
        if (!can_have_children && entry.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;

        std::string label = entry.name + " (" + entry.type + ")###entry_" + std::to_string(idx);
        bool is_open = ImGui::TreeNodeEx(label.c_str(), flags);

        if (ImGui::BeginPopupContextItem()) {
            if (can_have_children && ImGui::BeginMenu("Add child")) {
                for (const auto& vt : _value_types) {
                    if (ImGui::MenuItem(vt.name.c_str())) {
                        std::string name = _generate_default_name(vt.name);
                        _add_child_to_entry(entry, vt.name, name);
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Remove")) {
                _data_entries.erase(_data_entries.begin() + idx);
                ImGui::EndPopup();
                if (is_open) ImGui::TreePop();
                return;
            }
            ImGui::EndPopup();
        }

        if (is_open) {
            for (size_t i = 0; i < entry.children.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                _render_child(entry, entry.children[i], i);
                ImGui::PopID();
            }

            if (can_have_children) {
                std::string add_id = "+ add###add_child_" + std::to_string(idx);
                if (ImGui::SmallButton(add_id.c_str())) ImGui::OpenPopup("add_child_popup");
                if (ImGui::BeginPopup("add_child_popup")) {
                    for (const auto& vt : _value_types) {
                        if (ImGui::MenuItem(vt.name.c_str())) {
                            std::string name = _generate_default_name(vt.name);
                            _add_child_to_entry(entry, vt.name, name);
                        }
                    }
                    ImGui::EndPopup();
                }
            }
            ImGui::TreePop();
        }
    }

    void _render_child(DataEditorEntry& parent, DataEditorEntry& child, size_t idx) {
        bool can_have_children = (child.type == "map");
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
        if (!can_have_children && child.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;

        std::string label = child.name + " (" + child.type + ")###child_" + std::to_string(idx);
        bool is_open = ImGui::TreeNodeEx(label.c_str(), flags);

        if (ImGui::BeginPopupContextItem()) {
            if (can_have_children && ImGui::BeginMenu("Add child")) {
                for (const auto& vt : _value_types) {
                    if (ImGui::MenuItem(vt.name.c_str())) {
                        std::string name = _generate_default_name(vt.name);
                        _add_child_to_entry(child, vt.name, name);
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Remove")) {
                parent.children.erase(parent.children.begin() + idx);
                ImGui::EndPopup();
                if (is_open) ImGui::TreePop();
                return;
            }
            ImGui::EndPopup();
        }

        if (is_open) {
            for (size_t i = 0; i < child.children.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                _render_child(child, child.children[i], i);
                ImGui::PopID();
            }

            if (can_have_children) {
                std::string add_id = "+ add###add_nested_" + std::to_string(idx);
                if (ImGui::SmallButton(add_id.c_str())) ImGui::OpenPopup("add_nested_popup");
                if (ImGui::BeginPopup("add_nested_popup")) {
                    for (const auto& vt : _value_types) {
                        if (ImGui::MenuItem(vt.name.c_str())) {
                            std::string name = _generate_default_name(vt.name);
                            _add_child_to_entry(child, vt.name, name);
                        }
                    }
                    ImGui::EndPopup();
                }
            }
            ImGui::TreePop();
        }
    }

    void _add_child_to_entry(DataEditorEntry& parent, const std::string& type, const std::string& name) {
        DataEditorEntry child;
        child.name = name;
        child.type = type;
        parent.children.push_back(child);
    }

    std::string _generate_yaml() {
        if (_data_entries.empty()) return "";
        std::string yaml = "data:\n";
        for (const auto& entry : _data_entries) {
            yaml += "  " + entry.name + ":\n";
            yaml += "    type: " + entry.type + "\n";
            if (!entry.children.empty()) {
                yaml += "    arg:\n      children:\n";
                _generate_yaml_children(yaml, entry.children, 8);
            }
        }
        return yaml;
    }

    void _generate_yaml_children(std::string& yaml, const std::vector<DataEditorEntry>& children, int indent) {
        std::string pad(indent, ' ');
        for (const auto& child : children) {
            yaml += pad + child.name + ":\n";
            if (!child.children.empty()) {
                yaml += pad + "  children:\n";
                _generate_yaml_children(yaml, child.children, indent + 4);
            } else {
                yaml += pad + "  metadata:\n";
                yaml += pad + "    type: " + child.type + "\n";
            }
        }
    }
};

} // namespace ymery::plugins::editor
