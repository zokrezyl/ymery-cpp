#pragma once

#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"
#include <imgui.h>
#include <vector>

namespace ymery::plugins::imgui {

class Combo : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Combo>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Combo::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        std::string label;
        if (auto res = _data_bag->get("label"); res) {
            if (auto l = get_as<std::string>(*res)) {
                label = *l;
            }
        }

        if (auto res = _data_bag->get_static("items"); res) {
            if (auto items_list = get_as<List>(*res)) {
                _items.clear();
                for (const auto& item : *items_list) {
                    if (auto s = get_as<std::string>(item)) {
                        _items.push_back(*s);
                    }
                }
            }
        }

        if (auto res = _data_bag->get("value"); res) {
            if (auto v = get_as<int>(*res)) {
                _selected = *v;
            }
        }

        std::string items_str;
        for (const auto& item : _items) {
            items_str += item;
            items_str += '\0';
        }
        items_str += '\0';

        std::string imgui_id = label + "###" + _uid;
        if (ImGui::Combo(imgui_id.c_str(), &_selected, items_str.c_str())) {
            _data_bag->set("value", Value(_selected));
        }
        return Ok();
    }

private:
    int _selected = 0;
    std::vector<std::string> _items;
};

} // namespace ymery::plugins::imgui
