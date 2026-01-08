#pragma once

#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins::imgui {

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
        std::string label = "Item";
        if (auto res = _data_bag->get("label"); res) {
            if (auto l = get_as<std::string>(*res)) {
                label = *l;
            }
        }

        std::string shortcut;
        const char* shortcut_ptr = nullptr;
        if (auto res = _data_bag->get_static("shortcut"); res) {
            if (auto s = get_as<std::string>(*res)) {
                shortcut = *s;
                shortcut_ptr = shortcut.c_str();
            }
        }

        bool enabled = true;
        if (auto res = _data_bag->get("enabled"); res) {
            if (auto e = get_as<bool>(*res)) {
                enabled = *e;
            }
        }

        std::string imgui_id = label + "###" + _uid;
        if (ImGui::MenuItem(imgui_id.c_str(), shortcut_ptr, false, enabled)) {
            _is_body_activated = true;
        }
        return Ok();
    }

    Result<void> _detect_and_execute_events() override {
        if (_is_body_activated) {
            _execute_event_commands("on-click");
        }
        return Ok();
    }
};

} // namespace ymery::plugins::imgui
