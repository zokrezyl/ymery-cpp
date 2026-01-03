// checkbox widget plugin
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include "../../plugin_export.hpp"
#include <imgui.h>

namespace ymery::plugins {

class Checkbox : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Checkbox>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Checkbox::create failed", res);
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

        if (auto res = _data_bag->get("value"); res) {
            if (auto v = get_as<bool>(*res)) {
                _checked = *v;
            }
        }

        std::string imgui_id = label + "###" + _uid;
        if (ImGui::Checkbox(imgui_id.c_str(), &_checked)) {
            _data_bag->set("value", Value(_checked));
        }
        return Ok();
    }

private:
    bool _checked = false;
};

} // namespace ymery::plugins

extern "C" const char* PLUGIN_EXPORT_NAME() { return "checkbox"; }
extern "C" const char* PLUGIN_EXPORT_TYPE() { return "widget"; }
extern "C" void* PLUGIN_EXPORT_CREATE(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return static_cast<void*>(new ymery::Result<ymery::WidgetPtr>(ymery::plugins::Checkbox::create(wf, d, ns, db)));
}
