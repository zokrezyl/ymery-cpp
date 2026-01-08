#pragma once

#include "../../../frontend/composite.hpp"
#include "../../../frontend/widget_factory.hpp"
#include <imgui.h>
#include <imnodes.h>
#include <functional>

namespace ymery::plugins::imnodes {

class OutputAttribute : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<OutputAttribute>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("OutputAttribute::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _begin_container() override {
        std::hash<std::string> hasher;
        _attr_id = static_cast<int>(hasher(_uid) & 0x7FFFFFFF);

        std::string label = "Out";
        if (auto res = _data_bag->get("label"); res) {
            if (auto l = get_as<std::string>(*res)) {
                label = *l;
            }
        }

        ImNodes::BeginOutputAttribute(_attr_id);
        ImGui::Text("%s", label.c_str());
        _container_open = true;
        return Ok();
    }

    Result<void> _end_container() override {
        if (_container_open) {
            ImNodes::EndOutputAttribute();
        }
        return Ok();
    }

private:
    int _attr_id = 0;
};

} // namespace ymery::plugins::imnodes
