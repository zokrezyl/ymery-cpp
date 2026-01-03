// button widget plugin
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include "../../plugin_export.hpp"
#include <imgui.h>

namespace ymery::plugins {

class Button : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Button>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Button::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        std::string label = "Button";
        if (auto res = _data_bag->get("label"); res) {
            if (auto l = get_as<std::string>(*res)) {
                label = *l;
            }
        }

        // Render button with unique ID
        bool clicked = ImGui::Button((label + "###" + _uid).c_str());
        if (clicked) {
            // Activate body - if body is a popup, it will open
            _is_body_activated = true;
        }

        return Ok();
    }

    Result<void> _detect_and_execute_events() override {
        if (_is_body_activated) {
            _execute_event_commands("on-click");
        }
        // Handle on-hover events (for rich tooltips)
        if (ImGui::IsItemHovered()) {
            _execute_event_commands("on-hover");
        }
        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* PLUGIN_EXPORT_NAME() { return "button"; }
extern "C" const char* PLUGIN_EXPORT_TYPE() { return "widget"; }
extern "C" void* PLUGIN_EXPORT_CREATE(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return static_cast<void*>(new ymery::Result<ymery::WidgetPtr>(ymery::plugins::Button::create(wf, d, ns, db)));
}
