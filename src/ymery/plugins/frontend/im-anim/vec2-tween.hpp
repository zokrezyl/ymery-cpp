#pragma once
#include "../../../frontend/widget.hpp"
#include <imgui.h>
#include <im_anim.h>
#include <algorithm>

namespace ymery::plugins::imanim {

class Vec2Tween : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Vec2Tween>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;
        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Vec2Tween::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        iam_update_begin_frame();
        float dt = ImGui::GetIO().DeltaTime;
        if (dt <= 0.0f) dt = 1.0f / 60.0f;
        if (dt > 0.1f) dt = 0.1f;

        std::string label = "Vec2 Tween";
        if (auto res = _data_bag->get_static("label"); res) {
            if (auto l = get_as<std::string>(*res)) label = *l;
        }

        float duration = 1.5f;
        if (auto res = _data_bag->get_static("duration"); res) {
            if (auto d = get_as<double>(*res)) duration = static_cast<float>(*d);
        }

        int easing = iam_ease_out_cubic;
        if (auto res = _data_bag->get_static("easing"); res) {
            if (auto e = get_as<double>(*res)) easing = static_cast<int>(*e);
        }

        float target_x = 150.0f, target_y = 80.0f;
        if (auto res = _data_bag->get("target-x"); res) {
            if (auto t = get_as<double>(*res)) target_x = static_cast<float>(*t);
        }
        if (auto res = _data_bag->get("target-y"); res) {
            if (auto t = get_as<double>(*res)) target_y = static_cast<float>(*t);
        }

        ImGuiID id = ImGui::GetID(_uid.c_str());
        ImVec2 value = iam_tween_vec2(id, 0, ImVec2(target_x, target_y), duration,
            iam_ease_preset(static_cast<iam_ease_type>(easing)), iam_policy_crossfade, dt);

        ImGui::Text("%s", label.c_str());
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size(200, 200);
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
            IM_COL32(40, 40, 45, 255));
        draw_list->AddRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
            IM_COL32(80, 80, 85, 255));

        float draw_x = std::clamp(value.x, 0.0f, canvas_size.x - 10.0f);
        float draw_y = std::clamp(value.y, 0.0f, canvas_size.y - 10.0f);
        draw_list->AddCircleFilled(ImVec2(canvas_pos.x + draw_x + 10, canvas_pos.y + draw_y + 10), 10.0f,
            IM_COL32(100, 200, 255, 255));
        ImGui::Dummy(canvas_size);
        ImGui::Text("Position: (%.1f, %.1f)", value.x, value.y);

        return Ok();
    }
};

} // namespace ymery::plugins::imanim
