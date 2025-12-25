// editor-preview widget plugin - live preview of the layout being edited
// Renders actual widgets from the shared model, matching ymery-editor
#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"
#include "../../../types.hpp"
#include "shared_model.hpp"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <map>
#include <cstring>

namespace ymery::plugins {

class EditorPreview : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<EditorPreview>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("EditorPreview::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        auto& model = SharedLayoutModel::instance();

        if (model.empty()) {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            ImVec2 text_size = ImGui::CalcTextSize("No widgets in layout");
            ImGui::SetCursorPos(ImVec2(
                (avail.x - text_size.x) / 2,
                (avail.y - text_size.y) / 2
            ));
            ImGui::TextDisabled("No widgets in layout");
            return Ok();
        }

        _render_node(model.root());

        return Ok();
    }

private:
    // State for interactive widgets
    static std::map<int, bool> _checkbox_states;
    static std::map<int, std::string> _input_buffers;
    static std::map<int, int> _int_values;
    static std::map<int, float> _float_values;
    static std::map<int, ImVec4> _colors;

    void _render_node(LayoutNode* node) {
        if (!node) return;

        ImGui::PushID(node->id);

        // Handle same-line positioning
        if (node->position == LayoutPosition::SameLine && node->parent) {
            auto& siblings = node->parent->children;
            for (size_t i = 0; i < siblings.size(); ++i) {
                if (siblings[i].get() == node && i > 0) {
                    ImGui::SameLine();
                    break;
                }
            }
        }

        const std::string& type = node->widget_type;
        const std::string& label = node->label;
        bool has_children = !node->children.empty();

        // Render widget based on type
        if (type == "window") {
            if (ImGui::Begin(label.c_str())) {
                for (auto& child : node->children) {
                    _render_node(child.get());
                }
            }
            ImGui::End();
        }
        else if (type == "button") {
            ImGui::Button(label.c_str());
        }
        else if (type == "text") {
            ImGui::Text("%s", label.c_str());
        }
        else if (type == "separator") {
            ImGui::Separator();
        }
        else if (type == "spacing") {
            ImGui::Spacing();
        }
        else if (type == "checkbox") {
            ImGui::Checkbox(label.c_str(), &_checkbox_states[node->id]);
        }
        else if (type == "input-text") {
            if (_input_buffers.find(node->id) == _input_buffers.end()) {
                _input_buffers[node->id] = "";
            }
            char buf[256];
            strncpy(buf, _input_buffers[node->id].c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText(label.c_str(), buf, sizeof(buf))) {
                _input_buffers[node->id] = buf;
            }
        }
        else if (type == "input-int") {
            ImGui::InputInt(label.c_str(), &_int_values[node->id]);
        }
        else if (type == "slider-int") {
            ImGui::SliderInt(label.c_str(), &_int_values[node->id], 0, 100);
        }
        else if (type == "slider-float") {
            ImGui::SliderFloat(label.c_str(), &_float_values[node->id], 0.0f, 1.0f);
        }
        else if (type == "combo") {
            static std::map<int, int> combo_values;
            const char* items[] = { "Option 1", "Option 2", "Option 3" };
            ImGui::Combo(label.c_str(), &combo_values[node->id], items, 3);
        }
        else if (type == "color-edit") {
            if (_colors.find(node->id) == _colors.end()) {
                _colors[node->id] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            }
            ImGui::ColorEdit4(label.c_str(), &_colors[node->id].x);
        }
        else if (type == "row" || type == "group") {
            ImGui::BeginGroup();
            bool first = true;
            for (auto& child : node->children) {
                if (!first) ImGui::SameLine();
                _render_node(child.get());
                first = false;
            }
            ImGui::EndGroup();
        }
        else if (type == "column") {
            ImGui::BeginGroup();
            for (auto& child : node->children) {
                _render_node(child.get());
            }
            ImGui::EndGroup();
        }
        else if (type == "child") {
            ImVec2 size(0, 100);
            if (ImGui::BeginChild(label.c_str(), size, true)) {
                for (auto& child : node->children) {
                    _render_node(child.get());
                }
            }
            ImGui::EndChild();
        }
        else if (type == "tree-node") {
            if (ImGui::TreeNode(label.c_str())) {
                for (auto& child : node->children) {
                    _render_node(child.get());
                }
                ImGui::TreePop();
            }
        }
        else if (type == "collapsing-header") {
            if (ImGui::CollapsingHeader(label.c_str())) {
                for (auto& child : node->children) {
                    _render_node(child.get());
                }
            }
        }
        else if (type == "tab-bar") {
            if (ImGui::BeginTabBar(label.c_str())) {
                for (auto& child : node->children) {
                    _render_node(child.get());
                }
                ImGui::EndTabBar();
            }
        }
        else if (type == "tab-item") {
            if (ImGui::BeginTabItem(label.c_str())) {
                for (auto& child : node->children) {
                    _render_node(child.get());
                }
                ImGui::EndTabItem();
            }
        }
        else if (type == "tooltip") {
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%s", label.c_str());
                ImGui::EndTooltip();
            }
        }
        else if (type == "popup" || type == "popup-modal") {
            if (ImGui::Button(("Open " + label).c_str())) {
                ImGui::OpenPopup(label.c_str());
            }
            bool open = true;
            if (type == "popup-modal") {
                if (ImGui::BeginPopupModal(label.c_str(), &open)) {
                    for (auto& child : node->children) {
                        _render_node(child.get());
                    }
                    if (ImGui::Button("Close")) {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
            } else {
                if (ImGui::BeginPopup(label.c_str())) {
                    for (auto& child : node->children) {
                        _render_node(child.get());
                    }
                    ImGui::EndPopup();
                }
            }
        }
        else if (type == "toggle") {
            ImGui::Checkbox(label.c_str(), &_checkbox_states[node->id]);
        }
        else if (type == "knob") {
            ImGui::SliderFloat(label.c_str(), &_float_values[node->id], 0.0f, 1.0f);
        }
        else if (type == "markdown") {
            ImGui::TextWrapped("%s", label.c_str());
        }
        else {
            // Unknown type - show placeholder with children
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "[%s: %s]", type.c_str(), label.c_str());
            if (has_children) {
                ImGui::Indent();
                for (auto& child : node->children) {
                    _render_node(child.get());
                }
                ImGui::Unindent();
            }
        }

        ImGui::PopID();
    }
};

// Static member definitions
std::map<int, bool> EditorPreview::_checkbox_states;
std::map<int, std::string> EditorPreview::_input_buffers;
std::map<int, int> EditorPreview::_int_values;
std::map<int, float> EditorPreview::_float_values;
std::map<int, ImVec4> EditorPreview::_colors;

} // namespace ymery::plugins

extern "C" const char* name() { return "editor-preview"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::EditorPreview::create(wf, d, ns, db);
}
