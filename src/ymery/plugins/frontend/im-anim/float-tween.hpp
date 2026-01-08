#pragma once
#include "../../../frontend/widget.hpp"
#include <imgui.h>
#include <im_anim.h>

namespace ymery::plugins::imanim {

static float GetSafeDeltaTime() {
    float dt = ImGui::GetIO().DeltaTime;
    if (dt <= 0.0f) dt = 1.0f / 60.0f;
    if (dt > 0.1f) dt = 0.1f;
    return dt;
}

class FloatTween : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<FloatTween>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;
        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("FloatTween::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        iam_update_begin_frame();
        float dt = GetSafeDeltaTime();

        std::string label = "Float Tween";
        if (auto res = _data_bag->get_static("label"); res) {
            if (auto l = get_as<std::string>(*res)) label = *l;
        }

        float target = 100.0f;
        if (auto res = _data_bag->get("target"); res) {
            if (auto t = get_as<double>(*res)) target = static_cast<float>(*t);
        }

        float duration = 1.0f;
        if (auto res = _data_bag->get_static("duration"); res) {
            if (auto d = get_as<double>(*res)) duration = static_cast<float>(*d);
        }

        int easing = iam_ease_out_cubic;
        if (auto res = _data_bag->get_static("easing"); res) {
            if (auto e = get_as<double>(*res)) easing = static_cast<int>(*e);
        }

        ImGuiID id = ImGui::GetID(_uid.c_str());
        float value = iam_tween_float(id, 0, target, duration,
            iam_ease_preset(static_cast<iam_ease_type>(easing)), iam_policy_crossfade, dt);

        ImGui::Text("%s", label.c_str());
        ImGui::ProgressBar(value / 100.0f, ImVec2(-1, 0), "");
        ImGui::SameLine();
        ImGui::Text("%.1f", value);
        _data_bag->set("value", Value(static_cast<double>(value)));

        return Ok();
    }
};

} // namespace ymery::plugins::imanim
