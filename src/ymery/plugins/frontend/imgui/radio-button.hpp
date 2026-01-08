#pragma once

#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins::imgui {

class RadioButton : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<RadioButton>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("RadioButton::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        std::string label = "Option";
        if (auto res = _data_bag->get("label"); res) {
            if (auto l = get_as<std::string>(*res)) {
                label = *l;
            }
        }

        bool active = false;
        if (auto res = _data_bag->get("active"); res) {
            if (auto a = get_as<bool>(*res)) {
                active = *a;
            }
        }

        std::string imgui_id = label + "###" + _uid;
        if (ImGui::RadioButton(imgui_id.c_str(), active)) {
            _data_bag->set("active", Value(true));
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
