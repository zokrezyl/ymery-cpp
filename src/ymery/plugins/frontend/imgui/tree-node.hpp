#pragma once

#include "../../../frontend/composite.hpp"
#include "../../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins::imgui {

class TreeNode : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<TreeNode>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("TreeNode::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _begin_container() override {
        std::string label = "Node";
        if (auto label_res = _data_bag->get("label"); label_res) {
            if (auto l = get_as<std::string>(*label_res)) {
                label = *l;
            }
        }
        std::string imgui_id = label + "###" + _uid;
        _container_open = ImGui::TreeNode(imgui_id.c_str());
        return Ok();
    }

    Result<void> _end_container() override {
        if (_container_open) {
            ImGui::TreePop();
        }
        return Ok();
    }
};

} // namespace ymery::plugins::imgui
