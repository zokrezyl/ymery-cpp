#pragma once

#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins::imgui {

class ProgressBar : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<ProgressBar>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("ProgressBar::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        float fraction = 0.0f;
        if (auto res = _data_bag->get("fraction"); res) {
            if (auto v = get_as<double>(*res)) {
                fraction = static_cast<float>(*v);
            }
        }

        ImVec2 size(-FLT_MIN, 0);
        if (auto res = _data_bag->get_static("width"); res) {
            if (auto w = get_as<double>(*res)) {
                size.x = static_cast<float>(*w);
            }
        }
        if (auto res = _data_bag->get_static("height"); res) {
            if (auto h = get_as<double>(*res)) {
                size.y = static_cast<float>(*h);
            }
        }

        const char* overlay = nullptr;
        std::string overlay_str;
        if (auto res = _data_bag->get("overlay"); res) {
            if (auto o = get_as<std::string>(*res)) {
                overlay_str = *o;
                overlay = overlay_str.c_str();
            }
        }

        ImGui::ProgressBar(fraction, size, overlay);
        return Ok();
    }
};

} // namespace ymery::plugins::imgui
