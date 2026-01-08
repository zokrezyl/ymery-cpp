#pragma once

#include "../../../frontend/composite.hpp"
#include "../../../frontend/widget_factory.hpp"
#include <imgui.h>
#include <imnodes.h>

namespace ymery::plugins::imnodes {

class Editor : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Editor>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Editor::create failed", res);
        }
        return widget;
    }

    Result<void> init() override {
        if (auto res = Widget::init(); !res) {
            return res;
        }
        if (!ImNodes::GetCurrentContext()) {
            ImNodes::CreateContext();
        }
        return Ok();
    }

protected:
    Result<void> _begin_container() override {
        ImNodes::BeginNodeEditor();
        _container_open = true;
        return Ok();
    }

    Result<void> _end_container() override {
        if (_container_open) {
            ImNodes::EndNodeEditor();

            // Check for link creation
            int start_attr, end_attr;
            if (ImNodes::IsLinkCreated(&start_attr, &end_attr)) {
                _execute_event_commands("on-link-created");
            }

            // Check for link destruction
            int link_id;
            if (ImNodes::IsLinkDestroyed(&link_id)) {
                _execute_event_commands("on-link-destroyed");
            }
        }
        return Ok();
    }
};

} // namespace ymery::plugins::imnodes
