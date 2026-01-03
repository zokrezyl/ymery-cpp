// editor-widget-tree widget plugin - categorized widget browser with drag support
#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"
#include "../../../types.hpp"
#include <imgui.h>
#include <vector>

namespace ymery::plugins {

// Widget category with list of widget types
struct WidgetCategory {
    std::string name;
    std::vector<std::string> widgets;
};

class EditorWidgetTree : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<EditorWidgetTree>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;
        widget->_init_categories();

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("EditorWidgetTree::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        // Get optional title from config
        std::string title = "Widget Browser";
        if (auto res = _data_bag->get("title"); res && res->has_value()) {
            if (auto t = get_as<std::string>(*res)) {
                title = *t;
            }
        }

        ImGui::Text("%s", title.c_str());
        ImGui::Separator();

        // Render categories
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

        // Drag source for drag and drop
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("WIDGET_TYPE", widget_type.c_str(), widget_type.size() + 1);
            ImGui::Text("Add: %s", widget_type.c_str());
            ImGui::EndDragDropSource();
        }
    }

public:
    // Render as a menu (for popup/context menu use)
    // Returns true if a widget was selected, calls callback with widget type
    bool render_as_menu(const std::function<void(const std::string&)>& on_select) {
        bool selected = false;
        for (const auto& category : _categories) {
            if (_render_category_menu(category, on_select)) {
                selected = true;
            }
        }
        return selected;
    }

private:
    bool _render_category_menu(const WidgetCategory& category,
                               const std::function<void(const std::string&)>& on_select) {
        bool selected = false;
        if (ImGui::BeginMenu(category.name.c_str())) {
            for (const auto& widget : category.widgets) {
                if (ImGui::MenuItem(widget.c_str())) {
                    if (on_select) {
                        on_select(widget);
                    }
                    selected = true;
                }
            }
            ImGui::EndMenu();
        }
        return selected;
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "editor-widget-tree"; }
extern "C" const char* type() { return "widget"; }
extern "C" void* create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return static_cast<void*>(new ymery::Result<ymery::WidgetPtr>(ymery::plugins::EditorWidgetTree::create(wf, d, ns, db)));
}
