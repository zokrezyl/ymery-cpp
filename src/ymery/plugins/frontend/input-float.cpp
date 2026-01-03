// input-float widget plugin
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins {

class InputFloat : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<InputFloat>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("InputFloat::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        std::string label = "##input";
        if (auto res = _data_bag->get("label"); res && res->has_value()) {
            if (auto l = get_as<std::string>(*res)) label = *l;
        }

        float value = 0.0f;
        if (auto res = _data_bag->get("value"); res && res->has_value()) {
            if (auto v = get_as<float>(*res)) value = *v;
            else if (auto v = get_as<double>(*res)) value = static_cast<float>(*v);
        }

        float step = 0.0f, step_fast = 0.0f;
        if (auto res = _data_bag->get_static("step"); res && res->has_value()) {
            if (auto s = get_as<float>(*res)) step = *s;
            else if (auto s = get_as<double>(*res)) step = static_cast<float>(*s);
        }
        if (auto res = _data_bag->get_static("step-fast"); res && res->has_value()) {
            if (auto s = get_as<float>(*res)) step_fast = *s;
            else if (auto s = get_as<double>(*res)) step_fast = static_cast<float>(*s);
        }

        std::string imgui_id = label + "##" + _uid;
        if (ImGui::InputFloat(imgui_id.c_str(), &value, step, step_fast)) {
            _data_bag->set("value", static_cast<double>(value));
        }

        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "input-float"; }
extern "C" const char* type() { return "widget"; }
extern "C" void* create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return static_cast<void*>(new ymery::Result<ymery::WidgetPtr>(ymery::plugins::InputFloat::create(wf, d, ns, db)));
}
