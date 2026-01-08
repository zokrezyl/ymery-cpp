#pragma once

#include "../../../frontend/composite.hpp"
#include "../../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins::imgui {

class Window : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Window>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Window::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _begin_container() override {
        std::string title = "Window";
        if (auto title_res = _data_bag->get_static("title"); title_res) {
            if (auto t = get_as<std::string>(*title_res)) {
                title = *t;
            }
        }

        ImGuiWindowFlags flags = 0;
        if (auto flags_res = _data_bag->get_static("flags"); flags_res) {
            if (auto f = get_as<int>(*flags_res)) {
                flags = *f;
            }
        }

        _container_open = ImGui::Begin(title.c_str(), &_is_open, flags);
        return Ok();
    }

    Result<void> _end_container() override {
        ImGui::End();
        return Ok();
    }

private:
    bool _is_open = true;
};

} // namespace ymery::plugins::imgui
