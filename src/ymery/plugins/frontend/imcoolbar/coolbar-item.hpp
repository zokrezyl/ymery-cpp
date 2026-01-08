#pragma once

#include "../../../frontend/composite.hpp"
#include "../../../frontend/widget_factory.hpp"
#include <imgui.h>
#include <ImCoolBar.h>

namespace ymery::plugins::imcoolbar {

class CoolBarItem : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<CoolBarItem>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("CoolBarItem::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        std::string label = "Item";
        if (auto res = _data_bag->get_static("label"); res) {
            if (auto l = get_as<std::string>(*res)) {
                label = *l;
            }
        }

        std::string imgui_id = label + "###" + _uid;
        if (ImGui::CoolBarItem()) {
            if (ImGui::Button(imgui_id.c_str())) {
                // Button clicked - could dispatch event
            }
        }

        // Handle tooltip
        if (ImGui::IsItemHovered()) {
            if (auto res = _data_bag->get_static("tooltip"); res) {
                if (auto t = get_as<std::string>(*res)) {
                    ImGui::SetTooltip("%s", t->c_str());
                }
            }
        }

        return Ok();
    }
};

} // namespace ymery::plugins::imcoolbar
