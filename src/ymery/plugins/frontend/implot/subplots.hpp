#pragma once

#include "../../../frontend/composite.hpp"
#include "../../../frontend/widget_factory.hpp"
#include <imgui.h>
#include <implot.h>

namespace ymery::plugins::implot {

class Subplots : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Subplots>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Subplots::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _begin_container() override {
        std::string label = "Subplots";
        if (auto res = _data_bag->get("label"); res) {
            if (auto l = get_as<std::string>(*res)) {
                label = *l;
            }
        }

        int rows = 1, cols = 1;
        if (auto res = _data_bag->get_static("rows"); res) {
            if (auto r = get_as<int>(*res)) rows = *r;
            else if (auto r = get_as<double>(*res)) rows = static_cast<int>(*r);
        }
        if (auto res = _data_bag->get_static("cols"); res) {
            if (auto c = get_as<int>(*res)) cols = *c;
            else if (auto c = get_as<double>(*res)) cols = static_cast<int>(*c);
        }

        ImVec2 size(-1, -1);
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

        std::string imgui_id = label + "###" + _uid;
        _container_open = ImPlot::BeginSubplots(imgui_id.c_str(), rows, cols, size);
        return Ok();
    }

    Result<void> _end_container() override {
        if (_container_open) {
            ImPlot::EndSubplots();
        }
        return Ok();
    }
};

} // namespace ymery::plugins::implot
