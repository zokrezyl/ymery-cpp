// menu-item widget plugin - clickable menu entry
#include "../../frontend/widget.hpp"
#include "../../types.hpp"
#include <imgui.h>

namespace ymery::plugins {

class MenuItem : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<MenuItem>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("MenuItem::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        std::string label = "NO-LABEL";
        if (auto res = _data_bag->get("label"); res && res->has_value()) {
            if (auto s = get_as<std::string>(*res)) {
                label = *s;
            }
        }

        std::string shortcut = "";
        if (auto res = _data_bag->get("shortcut"); res && res->has_value()) {
            if (auto s = get_as<std::string>(*res)) {
                shortcut = *s;
            }
        }

        bool selection = false;
        if (auto res = _data_bag->get("selection"); res && res->has_value()) {
            if (auto b = get_as<bool>(*res)) {
                selection = *b;
            }
        }

        bool enabled = true;
        if (auto res = _data_bag->get("enabled"); res && res->has_value()) {
            if (auto b = get_as<bool>(*res)) {
                enabled = *b;
            }
        }

        bool clicked = ImGui::MenuItem(label.c_str(), shortcut.c_str(), selection, enabled);
        _is_body_activated = clicked;

        // TODO: dispatch click action if configured
        if (clicked) {
            // Check for action parameter and dispatch
        }

        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "menu-item"; }
extern "C" const char* type() { return "widget"; }
extern "C" void* create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return static_cast<void*>(new ymery::Result<ymery::WidgetPtr>(ymery::plugins::MenuItem::create(wf, d, ns, db)));
}
