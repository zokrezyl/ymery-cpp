#pragma once

#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <im_anim.h>
#include <cmath>
#include <algorithm>

namespace ymery::plugins::imanim {

static float GetSafeDeltaTime() {
    float dt = ImGui::GetIO().DeltaTime;
    if (dt <= 0.0f) dt = 1.0f / 60.0f;
    if (dt > 0.1f) dt = 0.1f;
    return dt;
}

class Anim : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Anim>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Anim::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        iam_update_begin_frame();
        float dt = GetSafeDeltaTime();

        // Get demo type
        std::string demo = "float-tween";
        if (auto res = _data_bag->get_static("demo"); res) {
            if (auto d = get_as<std::string>(*res)) {
                demo = *d;
            }
        }

        // Get label
        std::string label = "Animation";
        if (auto res = _data_bag->get_static("label"); res) {
            if (auto l = get_as<std::string>(*res)) {
                label = *l;
            }
        }

        // Get target value
        float target = 100.0f;
        if (auto res = _data_bag->get("target"); res) {
            if (auto t = get_as<double>(*res)) {
                target = static_cast<float>(*t);
            }
        }

        // Get duration
        float duration = 1.0f;
        if (auto res = _data_bag->get_static("duration"); res) {
            if (auto d = get_as<double>(*res)) {
                duration = static_cast<float>(*d);
            }
        }

        // Get easing type
        int easing = iam_ease_out_cubic;
        if (auto res = _data_bag->get_static("easing"); res) {
            if (auto e = get_as<double>(*res)) {
                easing = static_cast<int>(*e);
            }
        }

        ImGuiID id = ImGui::GetID(_uid.c_str());

        if (demo == "float-tween") {
            renderFloatTween(id, label, target, duration, easing, dt);
        }
        else if (demo == "vec2-tween") {
            renderVec2Tween(id, label, duration, easing, dt);
        }
        else if (demo == "color-tween") {
            renderColorTween(id, label, duration, easing, dt);
        }
        else if (demo == "easing-compare") {
            renderEasingCompare(id, label, target, duration, dt);
        }
        else if (demo == "oscillate") {
            renderOscillate(id, label, dt);
        }
        else if (demo == "shake") {
            renderShake(id, label, dt);
        }
        else if (demo == "spring") {
            renderSpring(id, label, target, dt);
        }

        return Ok();
    }

private:
    void renderFloatTween(ImGuiID id, const std::string& label, float target, float duration, int easing, float dt) {
        float value = iam_tween_float(id, 0, target, duration,
            iam_ease_preset(static_cast<iam_ease_type>(easing)), iam_policy_crossfade, dt);

        ImGui::Text("%s", label.c_str());
        ImGui::ProgressBar(value / 100.0f, ImVec2(-1, 0), "");
        ImGui::SameLine();
        ImGui::Text("%.1f", value);
        _data_bag->set("value", Value(static_cast<double>(value)));
    }

    void renderVec2Tween(ImGuiID id, const std::string& label, float duration, int easing, float dt) {
        float target_x = 150.0f, target_y = 80.0f;
        if (auto res = _data_bag->get("target-x"); res) {
            if (auto t = get_as<double>(*res)) target_x = static_cast<float>(*t);
        }
        if (auto res = _data_bag->get("target-y"); res) {
            if (auto t = get_as<double>(*res)) target_y = static_cast<float>(*t);
        }

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
    }

    void renderColorTween(ImGuiID id, const std::string& label, float duration, int easing, float dt) {
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

        ImVec4 value = iam_tween_color(id, 0, target_color, duration,
            iam_ease_preset(static_cast<iam_ease_type>(easing)), iam_policy_crossfade, iam_col_oklab, dt);

        ImGui::Text("%s", label.c_str());
        ImGui::ColorButton("##color", value, 0, ImVec2(100, 50));
        ImGui::SameLine();
        ImGui::Text("RGBA(%.2f, %.2f, %.2f, %.2f)", value.x, value.y, value.z, value.w);
    }

    void renderEasingCompare(ImGuiID id, const std::string& label, float target, float duration, float dt) {
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
    }

    void renderOscillate(ImGuiID id, const std::string& label, float dt) {
        int wave_type = iam_wave_sine;
        if (auto res = _data_bag->get_static("wave"); res) {
            if (auto w = get_as<double>(*res)) wave_type = static_cast<int>(*w);
        }
        float amplitude = 50.0f;
        if (auto res = _data_bag->get_static("amplitude"); res) {
            if (auto a = get_as<double>(*res)) amplitude = static_cast<float>(*a);
        }
        float frequency = 1.0f;
        if (auto res = _data_bag->get_static("frequency"); res) {
            if (auto f = get_as<double>(*res)) frequency = static_cast<float>(*f);
        }

        float value = iam_oscillate(id, amplitude, frequency, wave_type, 0.0f, dt);

        ImGui::Text("%s (Wave: %d)", label.c_str(), wave_type);
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        float canvas_w = ImGui::GetContentRegionAvail().x;
        float canvas_h = 100.0f;
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + canvas_w, canvas_pos.y + canvas_h),
            IM_COL32(40, 40, 45, 255));

        float center_y = canvas_pos.y + canvas_h / 2.0f;
        draw_list->AddLine(ImVec2(canvas_pos.x, center_y), ImVec2(canvas_pos.x + canvas_w, center_y),
            IM_COL32(60, 60, 65, 255));

        float dot_x = canvas_pos.x + canvas_w / 2.0f;
        float dot_y = center_y - value;
        draw_list->AddCircleFilled(ImVec2(dot_x, dot_y), 12.0f, IM_COL32(255, 150, 50, 255));

        ImGui::Dummy(ImVec2(canvas_w, canvas_h));
        ImGui::Text("Value: %.2f", value);
        _data_bag->set("value", Value(static_cast<double>(value)));
    }

    void renderShake(ImGuiID id, const std::string& label, float dt) {
        float intensity = 20.0f;
        if (auto res = _data_bag->get_static("intensity"); res) {
            if (auto i = get_as<double>(*res)) intensity = static_cast<float>(*i);
        }
        float frequency = 15.0f;
        if (auto res = _data_bag->get_static("frequency"); res) {
            if (auto f = get_as<double>(*res)) frequency = static_cast<float>(*f);
        }
        float decay = 1.0f;
        if (auto res = _data_bag->get_static("decay"); res) {
            if (auto d = get_as<double>(*res)) decay = static_cast<float>(*d);
        }

        if (ImGui::Button("Trigger Shake!")) {
            iam_trigger_shake(id);
        }

        ImVec2 shake = iam_shake_vec2(id, ImVec2(intensity, intensity), frequency, decay, dt);

        ImGui::Text("%s", label.c_str());
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        float canvas_w = ImGui::GetContentRegionAvail().x;
        float canvas_h = 100.0f;
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + canvas_w, canvas_pos.y + canvas_h),
            IM_COL32(40, 40, 45, 255));

        float center_x = canvas_pos.x + canvas_w / 2.0f + shake.x;
        float center_y = canvas_pos.y + canvas_h / 2.0f + shake.y;
        draw_list->AddRectFilled(ImVec2(center_x - 30, center_y - 30), ImVec2(center_x + 30, center_y + 30),
            IM_COL32(255, 100, 100, 255), 4.0f);

        ImGui::Dummy(ImVec2(canvas_w, canvas_h));
        ImGui::Text("Offset: (%.2f, %.2f)", shake.x, shake.y);
    }

    void renderSpring(ImGuiID id, const std::string& label, float target, float dt) {
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

        iam_ease_desc spring_ease = iam_ease_spring_desc(mass, stiffness, damping, 0.0f);
        float value = iam_tween_float(id, 0, target, 2.0f, spring_ease, iam_policy_crossfade, dt);

        ImGui::Text("%s (mass=%.1f, k=%.0f, c=%.0f)", label.c_str(), mass, stiffness, damping);
        ImGui::ProgressBar(value / 100.0f, ImVec2(-1, 0), "");
        ImGui::SameLine();
        ImGui::Text("%.1f", value);
        _data_bag->set("value", Value(static_cast<double>(value)));
    }
};

} // namespace ymery::plugins::imanim
