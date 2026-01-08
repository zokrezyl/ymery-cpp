#pragma once
// node-pin widget plugin - Input/output pin in node editor
#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"
#include <imgui_node_editor.h>
#include <imgui.h>
#include <functional>

namespace ed = ax::NodeEditor;

namespace ymery::plugins {

class NodePin : public Widget {
public:
    NodePin() = default;

    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<NodePin>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("NodePin::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        std::string label = "Pin";
        if (auto res = _data_bag->get("label"); res) {
            if (auto l = get_as<std::string>(*res)) {
                label = *l;
            }
        }

        // Get pin type (input or output)
        ed::PinKind kind = ed::PinKind::Input;
        if (auto res = _data_bag->get("type"); res) {
            if (auto t = get_as<std::string>(*res)) {
                if (*t == "output") {
                    kind = ed::PinKind::Output;
                }
            }
        }

        // Create unique pin ID from uid hash (like Python: hash(f"pin_{self.uid}"))
        std::hash<std::string> hasher;
        ed::PinId pin_id(hasher("pin_" + _uid) & 0x7FFFFFFF);

        // Render pin
        ed::BeginPin(pin_id, kind);
        ImGui::Text("%s", label.c_str());
        ed::EndPin();

        return Ok();
    }
};

} // namespace ymery::plugins
