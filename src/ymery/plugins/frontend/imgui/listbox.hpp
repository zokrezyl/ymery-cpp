#pragma once

#include "../../../frontend/composite.hpp"
#include "../../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins::imgui {

class Listbox : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Listbox>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Listbox::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _begin_container() override {
        std::string label;
        if (auto res = _data_bag->get("label"); res) {
            if (auto l = get_as<std::string>(*res)) {
                label = *l;
            }
        }

        ImVec2 size(0, 0);
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
        _container_open = ImGui::BeginListBox(imgui_id.c_str(), size);
        return Ok();
    }

    Result<void> _end_container() override {
        if (_container_open) {
            ImGui::EndListBox();
        }
        return Ok();
    }
};

} // namespace ymery::plugins::imgui
