// slider-float widget plugin
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins {

class SliderFloat : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<SliderFloat>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("SliderFloat::create failed", res);
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

        if (auto res = _data_bag->get_static("min"); res) {
            if (auto m = get_as<double>(*res)) {
                _min = static_cast<float>(*m);
            }
        }
        if (auto res = _data_bag->get_static("max"); res) {
            if (auto m = get_as<double>(*res)) {
                _max = static_cast<float>(*m);
            }
        }

        if (auto res = _data_bag->get("value"); res) {
            if (auto v = get_as<double>(*res)) {
                _value = static_cast<float>(*v);
            }
        }

        if (ImGui::SliderFloat(label.c_str(), &_value, _min, _max)) {
            _data_bag->set("value", Value(static_cast<double>(_value)));
        }
        return Ok();
    }

private:
    float _value = 0.0f;
    float _min = 0.0f;
    float _max = 1.0f;
};

} // namespace ymery::plugins

extern "C" const char* name() { return "slider-float"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::SliderFloat::create(wf, d, ns, db);
}
