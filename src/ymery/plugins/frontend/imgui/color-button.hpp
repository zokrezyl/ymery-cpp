#pragma once
#include "../../../frontend/widget.hpp"
#include <imgui.h>

namespace ymery::plugins::imgui {

class ColorButton : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<ColorButton>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;
        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("ColorButton::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        std::string label = "##color";
        if (auto res = _data_bag->get("label"); res && res->has_value()) {
            if (auto l = get_as<std::string>(*res)) label = *l;
        }

        float col[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        if (auto res = _data_bag->get("color"); res && res->has_value()) {
            if (auto list = get_as<List>(*res)) {
                for (size_t i = 0; i < 4 && i < list->size(); ++i) {
                    if (auto v = get_as<float>((*list)[i])) col[i] = *v;
                    else if (auto v = get_as<double>((*list)[i])) col[i] = static_cast<float>(*v);
                }
            }
        }

        ImVec2 size(0, 0);
        if (auto res = _data_bag->get_static("size"); res && res->has_value()) {
            if (auto list = get_as<List>(*res); list && list->size() >= 2) {
                if (auto w = get_as<float>((*list)[0])) size.x = *w;
                else if (auto w = get_as<double>((*list)[0])) size.x = static_cast<float>(*w);
                if (auto h = get_as<float>((*list)[1])) size.y = *h;
                else if (auto h = get_as<double>((*list)[1])) size.y = static_cast<float>(*h);
            }
        }

        std::string imgui_id = label + "##" + _uid;
        ImVec4 color(col[0], col[1], col[2], col[3]);
        if (ImGui::ColorButton(imgui_id.c_str(), color, ImGuiColorEditFlags_None, size)) {
            _execute_event_commands("on-click");
        }
        return Ok();
    }
};

} // namespace ymery::plugins::imgui
