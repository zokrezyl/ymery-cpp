#pragma once

#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins::imgui {

class DragFloat : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<DragFloat>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("DragFloat::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        std::string label;
        if (auto res = _data_bag->get("label"); res) {
            if (auto l = get_as<std::string>(*res)) {
                label = *l;
            }
        }

        if (auto res = _data_bag->get("value"); res) {
            if (auto v = get_as<double>(*res)) {
                _value = static_cast<float>(*v);
            }
        }

        float speed = 1.0f;
        if (auto res = _data_bag->get_static("speed"); res) {
            if (auto s = get_as<double>(*res)) {
                speed = static_cast<float>(*s);
            }
        }

        float min = 0.0f, max = 0.0f;
        if (auto res = _data_bag->get_static("min"); res) {
            if (auto m = get_as<double>(*res)) {
                min = static_cast<float>(*m);
            }
        }
        if (auto res = _data_bag->get_static("max"); res) {
            if (auto m = get_as<double>(*res)) {
                max = static_cast<float>(*m);
            }
        }

        std::string imgui_id = label + "###" + _uid;
        if (ImGui::DragFloat(imgui_id.c_str(), &_value, speed, min, max)) {
            _data_bag->set("value", Value(static_cast<double>(_value)));
        }
        return Ok();
    }

private:
    float _value = 0.0f;
};

} // namespace ymery::plugins::imgui
