// imgui plugin - core ImGui widgets
#include "../../../plugin.hpp"
#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"

// Widget headers
#include "button.hpp"
#include "text.hpp"
#include "checkbox.hpp"
#include "window.hpp"
#include "group.hpp"
#include "input-text.hpp"
#include "slider-int.hpp"
#include "combo.hpp"
#include "separator.hpp"
#include "spacing.hpp"
#include "same-line.hpp"
#include "child.hpp"
#include "collapsing-header.hpp"
#include "tab-bar.hpp"
#include "tab-item.hpp"
#include "tree-node.hpp"
#include "popup.hpp"
#include "slider-float.hpp"
#include "color-edit.hpp"
#include "progress-bar.hpp"
#include "menu-bar.hpp"
#include "menu.hpp"
#include "menu-item.hpp"
#include "tooltip.hpp"
#include "radio-button.hpp"
#include "listbox.hpp"
#include "input-int.hpp"
#include "input-float.hpp"
#include "drag-int.hpp"
#include "drag-float.hpp"
#include "selectable.hpp"
#include "column.hpp"
#include "row.hpp"
#include "indent.hpp"
#include "popup-modal.hpp"
#include "color-button.hpp"
#include "bullet-text.hpp"
#include "separator-text.hpp"
#include "main-menu-bar.hpp"
#include "table.hpp"
#include "table-row.hpp"
#include "table-column.hpp"
#include "next-column.hpp"
#include "imgui-main-window.hpp"
#include "hello-imgui-main-window.hpp"
#include "hello-imgui-menu.hpp"
#include "hello-imgui-app-menu-items.hpp"
#include "docking-main-window.hpp"
#include "docking-split.hpp"
#include "dockable-window.hpp"

#include <map>
#include <functional>

namespace ymery::plugins {

class ImguiPlugin : public Plugin {
public:
    const char* name() const override { return "imgui"; }

    std::vector<std::string> widgets() const override {
        return {
            "button", "text", "checkbox", "window", "group",
            "input-text", "input-int", "input-float",
            "slider-int", "slider-float", "drag-int", "drag-float",
            "combo", "listbox", "selectable", "radio-button",
            "separator", "spacing", "same-line",
            "child", "collapsing-header", "tab-bar", "tab-item", "tree-node",
            "popup", "popup-modal", "tooltip", "menu-bar", "menu", "menu-item",
            "color-edit", "color-button", "progress-bar",
            "column", "next-column", "row", "indent", "bullet-text", "separator-text",
            "main-menu-bar", "table", "table-row", "table-column",
            "imgui-main-window", "hello-imgui-main-window",
            "hello-imgui-menu", "hello-imgui-app-menu-items",
            "docking-main-window", "docking-split", "dockable-window"
        };
    }

    Result<WidgetPtr> createWidget(
        const std::string& widget_name,
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) override {
        if (widget_name == "button") {
            return imgui::Button::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "text") {
            return imgui::Text::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "checkbox") {
            return imgui::Checkbox::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "window") {
            return imgui::Window::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "group") {
            return imgui::Group::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "input-text") {
            return imgui::InputText::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "slider-int") {
            return imgui::SliderInt::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "combo") {
            return imgui::Combo::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "separator") {
            return imgui::Separator::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "spacing") {
            return imgui::Spacing::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "same-line") {
            return imgui::SameLine::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "child") {
            return imgui::Child::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "collapsing-header") {
            return imgui::CollapsingHeader::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "tab-bar") {
            return imgui::TabBar::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "tab-item") {
            return imgui::TabItem::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "tree-node") {
            return imgui::TreeNode::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "popup") {
            return imgui::Popup::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "slider-float") {
            return imgui::SliderFloat::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "color-edit") {
            return imgui::ColorEdit::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "progress-bar") {
            return imgui::ProgressBar::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "menu-bar") {
            return imgui::MenuBar::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "menu") {
            return imgui::Menu::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "menu-item") {
            return imgui::MenuItem::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "tooltip") {
            return imgui::Tooltip::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "radio-button") {
            return imgui::RadioButton::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "listbox") {
            return imgui::Listbox::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "input-int") {
            return imgui::InputInt::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "input-float") {
            return imgui::InputFloat::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "drag-int") {
            return imgui::DragInt::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "drag-float") {
            return imgui::DragFloat::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "selectable") {
            return imgui::Selectable::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "column") {
            return imgui::Column::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "next-column") {
            return imgui::NextColumn::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "row") {
            return imgui::Row::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "indent") {
            return imgui::Indent::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "popup-modal") {
            return imgui::PopupModal::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "color-button") {
            return imgui::ColorButton::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "bullet-text") {
            return imgui::BulletText::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "separator-text") {
            return imgui::SeparatorText::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "main-menu-bar") {
            return imgui::MainMenuBar::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "table") {
            return imgui::Table::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "table-row") {
            return imgui::TableRow::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "table-column") {
            return imgui::TableColumn::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "imgui-main-window") {
            return imgui::ImguiMainWindow::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "hello-imgui-main-window") {
            return imgui::HelloImguiMainWindow::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "hello-imgui-menu") {
            return imgui::HelloImguiMenu::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "hello-imgui-app-menu-items") {
            return imgui::HelloImguiAppMenuItems::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "docking-main-window") {
            return imgui::DockingMainWindow::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "docking-split") {
            return imgui::DockingSplit::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "dockable-window") {
            return imgui::DockableWindow::create(widget_factory, dispatcher, ns, data_bag);
        }
        return Err<WidgetPtr>("Unknown widget: " + widget_name);
    }
};

} // namespace ymery::plugins

extern "C" void* create() {
    return static_cast<void*>(new ymery::plugins::ImguiPlugin());
}
