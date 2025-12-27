// drag-float widget plugin
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins {

class DragFloat : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<DragFloat>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("DragFloat::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        std::string label = "##drag";
        if (auto res = _data_bag->get("label"); res && res->has_value()) {
            if (auto l = get_as<std::string>(*res)) label = *l;
        }

        float value = 0.0f;
        if (auto res = _data_bag->get("value"); res && res->has_value()) {
            if (auto v = get_as<float>(*res)) value = *v;
            else if (auto v = get_as<double>(*res)) value = static_cast<float>(*v);
        }

        float speed = 0.1f;
        if (auto res = _data_bag->get_static("speed"); res && res->has_value()) {
            if (auto s = get_as<float>(*res)) speed = *s;
            else if (auto s = get_as<double>(*res)) speed = static_cast<float>(*s);
        }

        float min_val = 0.0f, max_val = 0.0f;
        if (auto res = _data_bag->get_static("min"); res && res->has_value()) {
            if (auto m = get_as<float>(*res)) min_val = *m;
            else if (auto m = get_as<double>(*res)) min_val = static_cast<float>(*m);
        }
        if (auto res = _data_bag->get_static("max"); res && res->has_value()) {
            if (auto m = get_as<float>(*res)) max_val = *m;
            else if (auto m = get_as<double>(*res)) max_val = static_cast<float>(*m);
        }

        std::string imgui_id = label + "##" + _uid;
        if (ImGui::DragFloat(imgui_id.c_str(), &value, speed, min_val, max_val)) {
            _data_bag->set("value", static_cast<double>(value));
        }

        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "drag-float"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::DragFloat::create(wf, d, ns, db);
}
