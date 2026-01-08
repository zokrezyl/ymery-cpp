#pragma once

#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins::imgui {

class Separator : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Separator>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Separator::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        ImGui::Separator();
        return Ok();
    }
};

} // namespace ymery::plugins::imgui
