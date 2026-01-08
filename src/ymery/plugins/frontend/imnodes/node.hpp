#pragma once

#include "../../../frontend/composite.hpp"
#include "../../../frontend/widget_factory.hpp"
#include <imgui.h>
#include <imnodes.h>
#include <functional>

namespace ymery::plugins::imnodes {

class Node : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Node>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Node::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _begin_container() override {
        // Generate node ID from uid
        std::hash<std::string> hasher;
        _node_id = static_cast<int>(hasher(_uid) & 0x7FFFFFFF);

        // Get title
        std::string title = "Node";
        if (auto res = _data_bag->get("title"); res) {
            if (auto t = get_as<std::string>(*res)) {
                title = *t;
            }
        }

        // Set position if specified
        if (!_positioned) {
            float x = 100.0f, y = 100.0f;
            if (auto res = _data_bag->get_static("x"); res) {
                if (auto v = get_as<double>(*res)) x = static_cast<float>(*v);
            }
            if (auto res = _data_bag->get_static("y"); res) {
                if (auto v = get_as<double>(*res)) y = static_cast<float>(*v);
            }
            ImNodes::SetNodeScreenSpacePos(_node_id, ImVec2(x, y));
            _positioned = true;
        }

        ImNodes::BeginNode(_node_id);
        ImGui::Text("%s", title.c_str());
        ImGui::Spacing();

        _container_open = true;
        return Ok();
    }

    Result<void> _end_container() override {
        if (_container_open) {
            ImNodes::EndNode();
        }
        return Ok();
    }

private:
    int _node_id = 0;
    bool _positioned = false;
};

} // namespace ymery::plugins::imnodes
