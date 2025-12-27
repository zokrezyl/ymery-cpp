// radio-button widget plugin
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins {

class RadioButton : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<RadioButton>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("RadioButton::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        std::string label = "Radio";
        if (auto res = _data_bag->get("label"); res && res->has_value()) {
            if (auto l = get_as<std::string>(*res)) label = *l;
        }

        // Get current selected value from data
        std::string current_value = "";
        if (auto res = _data_bag->get("value"); res && res->has_value()) {
            if (auto v = get_as<std::string>(*res)) current_value = *v;
        }

        // Get this radio button's value
        std::string button_value = label;
        if (auto res = _data_bag->get_static("button-value"); res && res->has_value()) {
            if (auto v = get_as<std::string>(*res)) button_value = *v;
        }

        bool active = (current_value == button_value);

        std::string imgui_id = label + "##" + _uid;
        if (ImGui::RadioButton(imgui_id.c_str(), active)) {
            _data_bag->set("value", button_value);
        }

        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "radio-button"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::RadioButton::create(wf, d, ns, db);
}
