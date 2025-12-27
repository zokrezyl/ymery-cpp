// drag-int widget plugin
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins {

class DragInt : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<DragInt>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("DragInt::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        std::string label = "##drag";
        if (auto res = _data_bag->get("label"); res && res->has_value()) {
            if (auto l = get_as<std::string>(*res)) label = *l;
        }

        int value = 0;
        if (auto res = _data_bag->get("value"); res && res->has_value()) {
            if (auto v = get_as<int>(*res)) value = *v;
        }

        float speed = 1.0f;
        if (auto res = _data_bag->get_static("speed"); res && res->has_value()) {
            if (auto s = get_as<float>(*res)) speed = *s;
            else if (auto s = get_as<double>(*res)) speed = static_cast<float>(*s);
        }

        int min_val = 0, max_val = 0;
        if (auto res = _data_bag->get_static("min"); res && res->has_value()) {
            if (auto m = get_as<int>(*res)) min_val = *m;
        }
        if (auto res = _data_bag->get_static("max"); res && res->has_value()) {
            if (auto m = get_as<int>(*res)) max_val = *m;
        }

        std::string imgui_id = label + "##" + _uid;
        if (ImGui::DragInt(imgui_id.c_str(), &value, speed, min_val, max_val)) {
            _data_bag->set("value", value);
        }

        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "drag-int"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::DragInt::create(wf, d, ns, db);
}
