#pragma once
#include "../../../frontend/widget.hpp"
#include <imgui.h>
#include <im_anim.h>

namespace ymery::plugins::imanim {

class ColorTween : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<ColorTween>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;
        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("ColorTween::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        iam_update_begin_frame();
        float dt = ImGui::GetIO().DeltaTime;
        if (dt <= 0.0f) dt = 1.0f / 60.0f;
        if (dt > 0.1f) dt = 0.1f;

        std::string label = "Color Tween";
        if (auto res = _data_bag->get_static("label"); res) {
            if (auto l = get_as<std::string>(*res)) label = *l;
        }

        float duration = 2.0f;
        if (auto res = _data_bag->get_static("duration"); res) {
            if (auto d = get_as<double>(*res)) duration = static_cast<float>(*d);
        }

        int easing = iam_ease_out_cubic;
        if (auto res = _data_bag->get_static("easing"); res) {
            if (auto e = get_as<double>(*res)) easing = static_cast<int>(*e);
        }

        ImVec4 target_color(1.0f, 0.5f, 0.2f, 1.0f);
        if (auto res = _data_bag->get("target-r"); res) {
            if (auto t = get_as<double>(*res)) target_color.x = static_cast<float>(*t);
        }
        if (auto res = _data_bag->get("target-g"); res) {
            if (auto t = get_as<double>(*res)) target_color.y = static_cast<float>(*t);
        }
        if (auto res = _data_bag->get("target-b"); res) {
            if (auto t = get_as<double>(*res)) target_color.z = static_cast<float>(*t);
        }

        ImGuiID id = ImGui::GetID(_uid.c_str());
        ImVec4 value = iam_tween_color(id, 0, target_color, duration,
            iam_ease_preset(static_cast<iam_ease_type>(easing)), iam_policy_crossfade, iam_col_oklab, dt);

        ImGui::Text("%s", label.c_str());
        ImGui::ColorButton("##color", value, 0, ImVec2(100, 50));
        ImGui::SameLine();
        ImGui::Text("RGBA(%.2f, %.2f, %.2f, %.2f)", value.x, value.y, value.z, value.w);

        return Ok();
    }
};

} // namespace ymery::plugins::imanim
