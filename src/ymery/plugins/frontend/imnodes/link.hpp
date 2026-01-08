#pragma once

#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"
#include <imgui.h>
#include <imnodes.h>

namespace ymery::plugins::imnodes {

class Link : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Link>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Link::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        int link_id = 0;
        int start_attr = 0;
        int end_attr = 0;

        if (auto res = _data_bag->get_static("id"); res) {
            if (auto v = get_as<int>(*res)) link_id = *v;
        }
        if (auto res = _data_bag->get_static("start"); res) {
            if (auto v = get_as<int>(*res)) start_attr = *v;
        }
        if (auto res = _data_bag->get_static("end"); res) {
            if (auto v = get_as<int>(*res)) end_attr = *v;
        }

        ImNodes::Link(link_id, start_attr, end_attr);
        return Ok();
    }
};

} // namespace ymery::plugins::imnodes
