// knob widget plugin - Rotary knob control
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>
#include <imgui-knobs.h>

namespace ymery::plugins {

class Knob : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Knob>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Knob::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        std::string label = "Knob";
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

        // Get knob variant
        std::string variant = "wiper";
        if (auto res = _data_bag->get_static("variant"); res) {
            if (auto v = get_as<std::string>(*res)) {
                variant = *v;
            }
        }

        ImGuiKnobVariant knob_variant = ImGuiKnobVariant_Wiper;

        if (variant == "tick") knob_variant = ImGuiKnobVariant_Tick;
        else if (variant == "dot") knob_variant = ImGuiKnobVariant_Dot;
        else if (variant == "wiper") knob_variant = ImGuiKnobVariant_Wiper;
        else if (variant == "wiper_only") knob_variant = ImGuiKnobVariant_WiperOnly;
        else if (variant == "wiper_dot") knob_variant = ImGuiKnobVariant_WiperDot;
        else if (variant == "stepped") knob_variant = ImGuiKnobVariant_Stepped;
        else if (variant == "space") knob_variant = ImGuiKnobVariant_Space;

        float size = 50.0f;
        if (auto res = _data_bag->get_static("size"); res) {
            if (auto s = get_as<double>(*res)) {
                size = static_cast<float>(*s);
            }
        }

        if (ImGuiKnobs::Knob(label.c_str(), &_value, _min, _max, 0.0f, "%.3f", knob_variant, size)) {
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

extern "C" const char* name() { return "knob"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::Knob::create(wf, d, ns, db);
}
