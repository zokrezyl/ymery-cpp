#pragma once

#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins::imgui {

class DragInt : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<DragInt>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("DragInt::create failed", res);
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
            if (auto v = get_as<int>(*res)) {
                _value = *v;
            }
        }

        float speed = 1.0f;
        if (auto res = _data_bag->get_static("speed"); res) {
            if (auto s = get_as<double>(*res)) {
                speed = static_cast<float>(*s);
            }
        }

        int min = 0, max = 0;
        if (auto res = _data_bag->get_static("min"); res) {
            if (auto m = get_as<int>(*res)) {
                min = *m;
            }
        }
        if (auto res = _data_bag->get_static("max"); res) {
            if (auto m = get_as<int>(*res)) {
                max = *m;
            }
        }

        std::string imgui_id = label + "###" + _uid;
        if (ImGui::DragInt(imgui_id.c_str(), &_value, speed, min, max)) {
            _data_bag->set("value", Value(_value));
        }
        return Ok();
    }

private:
    int _value = 0;
};

} // namespace ymery::plugins::imgui
