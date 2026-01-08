#pragma once

#include "../../../frontend/composite.hpp"
#include "../../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins::imgui {

class MenuBar : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<MenuBar>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("MenuBar::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _begin_container() override {
        _container_open = ImGui::BeginMenuBar();
        return Ok();
    }

    Result<void> _end_container() override {
        if (_container_open) {
            ImGui::EndMenuBar();
        }
        return Ok();
    }
};

} // namespace ymery::plugins::imgui
