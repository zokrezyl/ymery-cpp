// slider-int widget plugin
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins {

class SliderInt : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<SliderInt>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("SliderInt::create failed", res);
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
            if (auto m = get_as<int>(*res)) {
                _min = *m;
            }
        }
        if (auto res = _data_bag->get_static("max"); res) {
            if (auto m = get_as<int>(*res)) {
                _max = *m;
            }
        }

        if (auto res = _data_bag->get("value"); res) {
            if (auto v = get_as<int>(*res)) {
                _value = *v;
            }
        }

        if (ImGui::SliderInt(label.c_str(), &_value, _min, _max)) {
            _data_bag->set("value", Value(_value));
        }
        return Ok();
    }

private:
    int _value = 0;
    int _min = 0;
    int _max = 100;
};

} // namespace ymery::plugins

extern "C" const char* name() { return "slider-int"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::SliderInt::create(wf, d, ns, db);
}
