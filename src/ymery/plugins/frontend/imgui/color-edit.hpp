#pragma once

#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins::imgui {

class ColorEdit : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<ColorEdit>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("ColorEdit::create failed", res);
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

        if (auto res = _data_bag->get("r"); res) {
            if (auto v = get_as<double>(*res)) {
                _color[0] = static_cast<float>(*v);
            }
        }
        if (auto res = _data_bag->get("g"); res) {
            if (auto v = get_as<double>(*res)) {
                _color[1] = static_cast<float>(*v);
            }
        }
        if (auto res = _data_bag->get("b"); res) {
            if (auto v = get_as<double>(*res)) {
                _color[2] = static_cast<float>(*v);
            }
        }
        if (auto res = _data_bag->get("a"); res) {
            if (auto v = get_as<double>(*res)) {
                _color[3] = static_cast<float>(*v);
            }
        }

        std::string imgui_id = label + "###" + _uid;
        if (ImGui::ColorEdit4(imgui_id.c_str(), _color)) {
            _data_bag->set("r", Value(static_cast<double>(_color[0])));
            _data_bag->set("g", Value(static_cast<double>(_color[1])));
            _data_bag->set("b", Value(static_cast<double>(_color[2])));
            _data_bag->set("a", Value(static_cast<double>(_color[3])));
        }
        return Ok();
    }

private:
    float _color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
};

} // namespace ymery::plugins::imgui
