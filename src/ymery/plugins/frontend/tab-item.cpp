// tab-item widget plugin
#include "../../frontend/composite.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins {

class TabItem : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<TabItem>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("TabItem::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _begin_container() override {
        std::string label = "Tab";
        if (auto label_res = _data_bag->get("label"); label_res) {
            if (auto l = get_as<std::string>(*label_res)) {
                label = *l;
            }
        }
        std::string imgui_id = label + "###" + _uid;
        _container_open = ImGui::BeginTabItem(imgui_id.c_str());
        return Ok();
    }

    Result<void> _end_container() override {
        if (_container_open) {
            ImGui::EndTabItem();
        }
        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "tab-item"; }
extern "C" const char* type() { return "widget"; }
extern "C" void* create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return static_cast<void*>(new ymery::Result<ymery::WidgetPtr>(ymery::plugins::TabItem::create(wf, d, ns, db)));
}
