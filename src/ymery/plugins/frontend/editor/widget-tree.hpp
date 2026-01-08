#pragma once
#include "../../../frontend/widget.hpp"
#include <imgui.h>
#include <vector>
#include <functional>

namespace ymery::plugins::editor {

struct WidgetCategory {
    std::string name;
    std::vector<std::string> widgets;
};

class WidgetTree : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<WidgetTree>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;
        widget->_init_categories();
        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("WidgetTree::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        std::string title = "Widget Browser";
        if (auto res = _data_bag->get("title"); res && res->has_value()) {
            if (auto t = get_as<std::string>(*res)) title = *t;
        }

        ImGui::Text("%s", title.c_str());
        ImGui::Separator();

        for (const auto& category : _categories) {
            _render_category(category);
        }
        return Ok();
    }

private:
    std::vector<WidgetCategory> _categories;

    void _init_categories() {
        _categories = {
            {"Containers", {"window", "row", "column", "group", "child"}},
            {"Collapsible", {"tree-node", "collapsing-header", "tab-bar", "tab-item"}},
            {"Inputs", {"button", "input-text", "input-int", "slider-int", "slider-float",
                        "checkbox", "combo", "color-edit", "toggle", "knob"}},
            {"Display", {"text", "separator", "spacing", "markdown"}},
            {"Popups", {"popup", "popup-modal", "tooltip"}},
            {"Visualization", {"implot", "implot-layer", "plot3d", "gizmo"}},
            {"Advanced", {"coolbar", "spinner"}}
        };
    }

    void _render_category(const WidgetCategory& category) {
        if (ImGui::TreeNode(category.name.c_str())) {
            for (const auto& widget : category.widgets) {
                _render_widget_item(widget);
            }
            ImGui::TreePop();
        }
    }

    void _render_widget_item(const std::string& widget_type) {
        ImGui::Selectable(widget_type.c_str());
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("WIDGET_TYPE", widget_type.c_str(), widget_type.size() + 1);
            ImGui::Text("Add: %s", widget_type.c_str());
            ImGui::EndDragDropSource();
        }
    }
};

} // namespace ymery::plugins::editor
