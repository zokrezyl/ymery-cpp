#include "widget_tree.hpp"
#include <imgui.h>

namespace ymery::editor {

WidgetTree::WidgetTree() {
    _init_categories();
}

void WidgetTree::_init_categories() {
    _categories = {
        {"Containers", {"window", "row", "column", "group", "child"}},
        {"Collapsible", {"tree-node", "collapsing-header", "tab-bar", "tab-item"}},
        {"Inputs", {"button", "input-text", "input-int", "slider-int", "slider-float",
                    "checkbox", "combo", "color-edit", "toggle", "knob"}},
        {"Display", {"text", "separator", "spacing", "markdown"}},
        {"Popups", {"popup", "popup-modal", "tooltip"}},
        {"Visualization", {"implot", "implot-layer", "plot3d", "gizmo"}},
        {"Advanced", {"node-editor", "node", "node-pin", "coolbar", "spinner"}}
    };
}

void WidgetTree::render() {
    for (const auto& category : _categories) {
        _render_category(category);
    }
}

void WidgetTree::_render_category(const WidgetCategory& category) {
    if (ImGui::TreeNode(category.name.c_str())) {
        for (const auto& widget : category.widgets) {
            _render_widget_item(widget);
        }
        ImGui::TreePop();
    }
}

void WidgetTree::_render_widget_item(const std::string& widget_type) {
    ImGui::Selectable(widget_type.c_str());

    // Drag source for drag and drop
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        ImGui::SetDragDropPayload("WIDGET_TYPE", widget_type.c_str(), widget_type.size() + 1);
        ImGui::Text("Add: %s", widget_type.c_str());

        if (_drag_callback) {
            _drag_callback(widget_type);
        }

        ImGui::EndDragDropSource();
    }
}

bool WidgetTree::render_as_menu(const WidgetSelectedCallback& on_select) {
    bool selected = false;
    for (const auto& category : _categories) {
        if (_render_category_menu(category, on_select)) {
            selected = true;
        }
    }
    return selected;
}

bool WidgetTree::_render_category_menu(const WidgetCategory& category, const WidgetSelectedCallback& on_select) {
    bool selected = false;
    if (ImGui::BeginMenu(category.name.c_str())) {
        for (const auto& widget : category.widgets) {
            if (_render_widget_menu_item(widget, on_select)) {
                selected = true;
            }
        }
        ImGui::EndMenu();
    }
    return selected;
}

bool WidgetTree::_render_widget_menu_item(const std::string& widget_type, const WidgetSelectedCallback& on_select) {
    if (ImGui::MenuItem(widget_type.c_str())) {
        if (on_select) {
            on_select(widget_type);
        }
        return true;
    }
    return false;
}

void WidgetTree::set_drag_callback(const WidgetSelectedCallback& callback) {
    _drag_callback = callback;
}

} // namespace ymery::editor
