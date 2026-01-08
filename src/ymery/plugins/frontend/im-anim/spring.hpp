#pragma once
#include "../../../frontend/widget.hpp"
#include <imgui.h>
#include <im_anim.h>

namespace ymery::plugins::imanim {

class Spring : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Spring>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;
        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Spring::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        iam_update_begin_frame();
        float dt = ImGui::GetIO().DeltaTime;
        if (dt <= 0.0f) dt = 1.0f / 60.0f;
        if (dt > 0.1f) dt = 0.1f;

        std::string label = "Spring";
        if (auto res = _data_bag->get_static("label"); res) {
            if (auto l = get_as<std::string>(*res)) label = *l;
        }

        float target = 100.0f;
        if (auto res = _data_bag->get("target"); res) {
            if (auto t = get_as<double>(*res)) target = static_cast<float>(*t);
        }

        float mass = 1.0f, stiffness = 180.0f, damping = 12.0f;
        if (auto res = _data_bag->get_static("mass"); res) {
            if (auto m = get_as<double>(*res)) mass = static_cast<float>(*m);
        }
        if (auto res = _data_bag->get_static("stiffness"); res) {
            if (auto s = get_as<double>(*res)) stiffness = static_cast<float>(*s);
        }
        if (auto res = _data_bag->get_static("damping"); res) {
            if (auto d = get_as<double>(*res)) damping = static_cast<float>(*d);
        }

        ImGuiID id = ImGui::GetID(_uid.c_str());
        iam_ease_desc spring_ease = iam_ease_spring_desc(mass, stiffness, damping, 0.0f);
        float value = iam_tween_float(id, 0, target, 2.0f, spring_ease, iam_policy_crossfade, dt);

        ImGui::Text("%s (mass=%.1f, k=%.0f, c=%.0f)", label.c_str(), mass, stiffness, damping);
        ImGui::ProgressBar(value / 100.0f, ImVec2(-1, 0), "");
        ImGui::SameLine();
        ImGui::Text("%.1f", value);
        _data_bag->set("value", Value(static_cast<double>(value)));

        return Ok();
    }
};

} // namespace ymery::plugins::imanim
