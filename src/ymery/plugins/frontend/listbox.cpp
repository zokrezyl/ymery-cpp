// listbox widget plugin
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>
#include <vector>

namespace ymery::plugins {

class Listbox : public Widget {
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
    Result<void> _pre_render_head() override {
        std::string label = "##listbox";
        if (auto res = _data_bag->get("label"); res && res->has_value()) {
            if (auto l = get_as<std::string>(*res)) label = *l;
        }

        int current_item = 0;
        if (auto res = _data_bag->get("value"); res && res->has_value()) {
            if (auto v = get_as<int>(*res)) current_item = *v;
        }

        std::vector<std::string> items;
        if (auto res = _data_bag->get_static("items"); res && res->has_value()) {
            if (auto list = get_as<List>(*res)) {
                for (const auto& item : *list) {
                    if (auto s = get_as<std::string>(item)) {
                        items.push_back(*s);
                    }
                }
            }
        }

        int height_items = -1;
        if (auto res = _data_bag->get_static("height-items"); res && res->has_value()) {
            if (auto h = get_as<int>(*res)) height_items = *h;
        }

        // Convert to C-style array for ImGui
        std::vector<const char*> items_cstr;
        for (const auto& item : items) {
            items_cstr.push_back(item.c_str());
        }

        std::string imgui_id = label + "##" + _uid;
        if (ImGui::ListBox(imgui_id.c_str(), &current_item, items_cstr.data(),
                          static_cast<int>(items_cstr.size()), height_items)) {
            _data_bag->set("value", current_item);
        }

        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "listbox"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::Listbox::create(wf, d, ns, db);
}
