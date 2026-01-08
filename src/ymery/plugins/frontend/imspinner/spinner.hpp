#pragma once

#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"
#include <imgui.h>
#include <imspinner.h>

namespace ymery::plugins::imspinner {

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
        std::string variant = "ang";
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

        float speed = 2.8f;
        if (auto res = _data_bag->get_static("speed"); res) {
            if (auto s = get_as<double>(*res)) {
                speed = static_cast<float>(*s);
            }
        }

        ImColor color = ImColor(ImGui::GetStyleColorVec4(ImGuiCol_Text));
        ImColor bg = ImColor(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

        std::string id = "##spinner" + _uid;

        if (variant == "ang") {
            ImSpinner::SpinnerAng(id.c_str(), radius, thickness, color, bg, speed);
        } else if (variant == "ang-triple") {
            ImSpinner::SpinnerAngTriple(id.c_str(), radius, radius * 0.75f, radius * 0.5f, thickness, color, bg, color, speed);
        } else if (variant == "dots") {
            ImSpinner::SpinnerBounceDots(id.c_str(), radius, thickness, color, speed, 3);
        } else if (variant == "bounce-ball") {
            ImSpinner::SpinnerBounceBall(id.c_str(), radius, thickness, color, speed);
        } else if (variant == "rainbow") {
            ImSpinner::SpinnerRainbow(id.c_str(), radius, thickness, color, speed);
        } else if (variant == "pulsar") {
            ImSpinner::SpinnerPulsar(id.c_str(), radius, thickness, bg, speed);
        } else if (variant == "twin-pulsar") {
            ImSpinner::SpinnerTwinPulsar(id.c_str(), radius, thickness, color, speed);
        } else if (variant == "clock") {
            ImSpinner::SpinnerClock(id.c_str(), radius, thickness, color, bg, speed);
        } else if (variant == "loading-ring") {
            ImSpinner::SpinnerLoadingRing(id.c_str(), radius, thickness, color, bg, speed);
        } else if (variant == "fade-dots") {
            ImSpinner::SpinnerFadeDots(id.c_str(), radius, thickness, color, speed);
        } else if (variant == "scale-dots") {
            ImSpinner::SpinnerScaleDots(id.c_str(), radius, thickness, color, speed);
        } else if (variant == "wave-dots") {
            ImSpinner::SpinnerWaveDots(id.c_str(), radius, thickness, color, speed);
        } else if (variant == "moving-dots") {
            ImSpinner::SpinnerMovingDots(id.c_str(), radius, thickness, color, speed);
        } else if (variant == "rotate-dots") {
            ImSpinner::SpinnerRotateDots(id.c_str(), radius, thickness, color, speed);
        } else if (variant == "twin-ang") {
            ImSpinner::SpinnerTwinAng(id.c_str(), radius, radius * 0.6f, thickness, color, bg, speed);
        } else if (variant == "filling") {
            ImSpinner::SpinnerFilling(id.c_str(), radius, thickness, color, bg, speed);
        } else if (variant == "topup") {
            ImSpinner::SpinnerTopup(id.c_str(), radius, radius * 0.6f, bg, color, color, speed);
        } else if (variant == "circular-lines") {
            ImSpinner::SpinnerCircularLines(id.c_str(), radius, color, speed);
        } else if (variant == "orion-dots") {
            ImSpinner::SpinnerOrionDots(id.c_str(), radius, thickness, color, speed);
        } else if (variant == "galaxy-dots") {
            ImSpinner::SpinnerGalaxyDots(id.c_str(), radius, thickness, color, speed);
        } else {
            // Default to SpinnerAng
            ImSpinner::SpinnerAng(id.c_str(), radius, thickness, color, bg, speed);
        }

        return Ok();
    }
};

} // namespace ymery::plugins::imspinner
