// spinner widget plugin - Loading spinner
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>
#include <imspinner.h>

namespace ymery::plugins {

class Spinner : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Spinner>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Spinner::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        std::string variant = "spinner";
        if (auto res = _data_bag->get_static("variant"); res) {
            if (auto v = get_as<std::string>(*res)) {
                variant = *v;
            }
        }

        float radius = 16.0f;
        if (auto res = _data_bag->get_static("radius"); res) {
            if (auto r = get_as<double>(*res)) {
                radius = static_cast<float>(*r);
            }
        }

        float thickness = 2.0f;
        if (auto res = _data_bag->get_static("thickness"); res) {
            if (auto t = get_as<double>(*res)) {
                thickness = static_cast<float>(*t);
            }
        }

        ImColor color = ImColor(ImGui::GetStyleColorVec4(ImGuiCol_Text));
        ImColor bg = ImColor(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        float speed = 2.8f;

        if (variant == "spinner") {
            // SpinnerAngTriple needs 3 radius values
            ImSpinner::SpinnerAngTriple("spinner", radius, radius * 0.75f, radius * 0.5f, thickness, color, bg, color, speed);
        } else if (variant == "dots") {
            ImSpinner::SpinnerBounceDots("dots", radius, thickness, color, speed, 3);
        } else if (variant == "ang") {
            ImSpinner::SpinnerAng("ang", radius, thickness, color, bg, speed);
        } else if (variant == "bounceball") {
            ImSpinner::SpinnerBounceBall("bounceball", radius, thickness, color, speed);
        } else if (variant == "rainbow") {
            ImSpinner::SpinnerRainbow("rainbow", radius, thickness, color, speed);
        } else if (variant == "pulsar") {
            ImSpinner::SpinnerPulsar("pulsar", radius, thickness, bg, speed);
        } else {
            ImSpinner::SpinnerAng("spinner", radius, thickness, color, bg, speed);
        }

        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "spinner"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::Spinner::create(wf, d, ns, db);
}
