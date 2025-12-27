// knob-int widget plugin - integer variant of knob
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>
#include <imgui-knobs.h>

namespace ymery::plugins {

class KnobInt : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<KnobInt>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("KnobInt::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        std::string label = "##knob";
        if (auto res = _data_bag->get("label"); res && res->has_value()) {
            if (auto l = get_as<std::string>(*res)) label = *l;
        }

        int value = 0;
        if (auto res = _data_bag->get("value"); res && res->has_value()) {
            if (auto v = get_as<int>(*res)) value = *v;
        }

        int min_val = 0, max_val = 100;
        if (auto res = _data_bag->get_static("min"); res && res->has_value()) {
            if (auto m = get_as<int>(*res)) min_val = *m;
        }
        if (auto res = _data_bag->get_static("max"); res && res->has_value()) {
            if (auto m = get_as<int>(*res)) max_val = *m;
        }

        float speed = 1.0f;
        if (auto res = _data_bag->get_static("speed"); res && res->has_value()) {
            if (auto s = get_as<float>(*res)) speed = *s;
            else if (auto s = get_as<double>(*res)) speed = static_cast<float>(*s);
        }

        ImGuiKnobVariant variant = ImGuiKnobVariant_Tick;
        if (auto res = _data_bag->get_static("variant"); res && res->has_value()) {
            if (auto v = get_as<std::string>(*res)) {
                if (*v == "tick") variant = ImGuiKnobVariant_Tick;
                else if (*v == "dot") variant = ImGuiKnobVariant_Dot;
                else if (*v == "wiper") variant = ImGuiKnobVariant_Wiper;
                else if (*v == "wiper-only") variant = ImGuiKnobVariant_WiperOnly;
                else if (*v == "wiper-dot") variant = ImGuiKnobVariant_WiperDot;
                else if (*v == "stepped") variant = ImGuiKnobVariant_Stepped;
                else if (*v == "space") variant = ImGuiKnobVariant_Space;
            }
        }

        float size = 0.0f;  // 0 = default
        if (auto res = _data_bag->get_static("size"); res && res->has_value()) {
            if (auto s = get_as<float>(*res)) size = *s;
            else if (auto s = get_as<double>(*res)) size = static_cast<float>(*s);
            else if (auto s = get_as<int>(*res)) size = static_cast<float>(*s);
        }

        std::string imgui_id = label + "##" + _uid;
        if (ImGuiKnobs::KnobInt(imgui_id.c_str(), &value, min_val, max_val, speed, nullptr, variant, size)) {
            _data_bag->set("value", value);
        }

        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "knob-int"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::KnobInt::create(wf, d, ns, db);
}
