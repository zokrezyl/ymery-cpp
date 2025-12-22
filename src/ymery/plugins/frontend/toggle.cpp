// toggle widget plugin - Toggle switch
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>
#include <imgui_toggle.h>

namespace ymery::plugins {

class Toggle : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Toggle>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Toggle::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        std::string label = "Toggle";
        if (auto res = _data_bag->get("label"); res) {
            if (auto l = get_as<std::string>(*res)) {
                label = *l;
            }
        }

        if (auto res = _data_bag->get("value"); res) {
            if (auto v = get_as<bool>(*res)) {
                _value = *v;
            }
        }

        if (ImGui::Toggle(label.c_str(), &_value)) {
            _data_bag->set("value", Value(_value));
        }

        return Ok();
    }

private:
    bool _value = false;
};

} // namespace ymery::plugins

extern "C" const char* name() { return "toggle"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::Toggle::create(wf, d, ns, db);
}
