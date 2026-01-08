#pragma once
#include "../../../frontend/widget.hpp"
#include <imgui.h>
#include <im_anim.h>

namespace ymery::plugins::imanim {

class EasingCompare : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<EasingCompare>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;
        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("EasingCompare::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        iam_update_begin_frame();
        float dt = ImGui::GetIO().DeltaTime;
        if (dt <= 0.0f) dt = 1.0f / 60.0f;
        if (dt > 0.1f) dt = 0.1f;

        std::string label = "Easing Compare";
        if (auto res = _data_bag->get_static("label"); res) {
            if (auto l = get_as<std::string>(*res)) label = *l;
        }

        float target = 100.0f;
        if (auto res = _data_bag->get("target"); res) {
            if (auto t = get_as<double>(*res)) target = static_cast<float>(*t);
        }

        float duration = 3.0f;
        if (auto res = _data_bag->get_static("duration"); res) {
            if (auto d = get_as<double>(*res)) duration = static_cast<float>(*d);
        }

        ImGuiID id = ImGui::GetID(_uid.c_str());

        ImGui::Text("%s", label.c_str());

        const char* ease_names[] = {
            "Linear", "InQuad", "OutQuad", "InOutQuad",
            "InCubic", "OutCubic", "InOutCubic",
            "InQuart", "OutQuart", "InOutQuart",
            "InQuint", "OutQuint", "InOutQuint",
            "InSine", "OutSine", "InOutSine",
            "InExpo", "OutExpo", "InOutExpo",
            "InCirc", "OutCirc", "InOutCirc",
            "InBack", "OutBack", "InOutBack",
            "InElastic", "OutElastic", "InOutElastic",
            "InBounce", "OutBounce", "InOutBounce"
        };

        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        float canvas_w = ImGui::GetContentRegionAvail().x;
        float bar_h = 20.0f;
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        for (int i = 0; i <= iam_ease_in_out_bounce; i++) {
            float value = iam_tween_float(id + i + 1, 0, target, duration,
                iam_ease_preset(static_cast<iam_ease_type>(i)), iam_policy_crossfade, dt);

            ImVec2 bar_pos(canvas_pos.x, canvas_pos.y + i * (bar_h + 4));
            draw_list->AddRectFilled(bar_pos, ImVec2(bar_pos.x + canvas_w, bar_pos.y + bar_h),
                IM_COL32(40, 42, 48, 255), 4.0f);

            float bar_w = (value / 100.0f) * (canvas_w - 100);
            ImU32 bar_color = IM_COL32(100 + (i * 5) % 155, 180 - (i * 3) % 80, 255 - (i * 7) % 155, 255);
            draw_list->AddRectFilled(ImVec2(bar_pos.x + 2, bar_pos.y + 2),
                ImVec2(bar_pos.x + 2 + bar_w, bar_pos.y + bar_h - 2), bar_color, 3.0f);

            draw_list->AddText(ImVec2(bar_pos.x + canvas_w - 90, bar_pos.y + 2),
                IM_COL32(200, 200, 200, 255), ease_names[i]);
        }
        ImGui::Dummy(ImVec2(canvas_w, (iam_ease_in_out_bounce + 1) * (bar_h + 4)));

        return Ok();
    }
};

} // namespace ymery::plugins::imanim
