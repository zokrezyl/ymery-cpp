#pragma once

#include <string>
#include <vector>
#include <functional>

namespace ymery::editor {

// Widget category with list of widget types
struct WidgetCategory {
    std::string name;
    std::vector<std::string> widgets;
};

// Callback when a widget is selected
using WidgetSelectedCallback = std::function<void(const std::string& widget_type)>;

// Widget tree browser - left pane
class WidgetTree {
public:
    WidgetTree();

    // Render the widget tree (as a window or in a region)
    void render();

    // Render as a menu (for popup/context menu use)
    // Returns true if a widget was selected
    bool render_as_menu(const WidgetSelectedCallback& on_select);

    // Set callback for when drag starts (for status display)
    void set_drag_callback(const WidgetSelectedCallback& callback);

private:
    std::vector<WidgetCategory> _categories;
    WidgetSelectedCallback _drag_callback;

    void _init_categories();
    void _render_category(const WidgetCategory& category);
    bool _render_category_menu(const WidgetCategory& category, const WidgetSelectedCallback& on_select);
    void _render_widget_item(const std::string& widget_type);
    bool _render_widget_menu_item(const std::string& widget_type, const WidgetSelectedCallback& on_select);
};

} // namespace ymery::editor
