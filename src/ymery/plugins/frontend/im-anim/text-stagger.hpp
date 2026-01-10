#pragma once
#include "../../../frontend/widget.hpp"
#include <imgui.h>
#include <im_anim.h>

namespace ymery::plugins::imanim {

class TextStagger : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<TextStagger>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;
        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("TextStagger::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        iam_update_begin_frame();
        float dt = ImGui::GetIO().DeltaTime;
        if (dt <= 0.0f) dt = 1.0f / 60.0f;
        if (dt > 0.1f) dt = 0.1f;

        std::string label = "Text Stagger";
        if (auto res = _data_bag->get_static("label"); res) {
            if (auto l = get_as<std::string>(*res)) label = *l;
        }

        // Text to animate
        std::string text = "Hello Animation!";
        if (auto res = _data_bag->get_static("text"); res) {
            if (auto t = get_as<std::string>(*res)) text = *t;
        }

        // Effect type
        int effect = iam_text_fx_fade;
        if (auto res = _data_bag->get_static("effect"); res) {
            if (auto e = get_as<double>(*res)) effect = static_cast<int>(*e);
        }

        // Delay between characters
        float char_delay = 0.05f;
        if (auto res = _data_bag->get_static("char-delay"); res) {
            if (auto d = get_as<double>(*res)) char_delay = static_cast<float>(*d);
        }

        // Duration per character
        float char_duration = 0.3f;
        if (auto res = _data_bag->get_static("char-duration"); res) {
            if (auto d = get_as<double>(*res)) char_duration = static_cast<float>(*d);
        }

        // Effect intensity
        float intensity = 20.0f;
        if (auto res = _data_bag->get_static("intensity"); res) {
            if (auto i = get_as<double>(*res)) intensity = static_cast<float>(*i);
        }

        // Easing
        int easing = iam_ease_out_cubic;
        if (auto res = _data_bag->get_static("easing"); res) {
            if (auto e = get_as<double>(*res)) easing = static_cast<int>(*e);
        }

        // Color
        ImU32 color = IM_COL32(255, 255, 255, 255);
        if (auto res = _data_bag->get_static("color-r"); res) {
            float r = 1.0f, g = 1.0f, b = 1.0f;
            if (auto c = get_as<double>(*res)) r = static_cast<float>(*c);
            if (auto res2 = _data_bag->get_static("color-g"); res2) {
                if (auto c = get_as<double>(*res2)) g = static_cast<float>(*c);
            }
            if (auto res3 = _data_bag->get_static("color-b"); res3) {
                if (auto c = get_as<double>(*res3)) b = static_cast<float>(*c);
            }
            color = IM_COL32(static_cast<int>(r * 255), static_cast<int>(g * 255), static_cast<int>(b * 255), 255);
        }

        // Font scale
        float font_scale = 1.5f;
        if (auto res = _data_bag->get_static("font-scale"); res) {
            if (auto s = get_as<double>(*res)) font_scale = static_cast<float>(*s);
        }

        // Letter spacing
        float letter_spacing = 0.0f;
        if (auto res = _data_bag->get_static("letter-spacing"); res) {
            if (auto s = get_as<double>(*res)) letter_spacing = static_cast<float>(*s);
        }

        ImGuiID id = ImGui::GetID(_uid.c_str());

        // Calculate total duration
        float total_duration = iam_text_stagger_duration(text.c_str(), iam_text_stagger_opts());

        // Animate progress from 0 to 1
        float progress = iam_tween_float(id, 0, 1.0f, total_duration + 0.5f,
            iam_ease_preset(iam_ease_linear), iam_policy_crossfade, dt);

        const char* effect_names[] = {
            "None", "Fade", "Scale", "Slide Up", "Slide Down",
            "Slide Left", "Slide Right", "Rotate", "Bounce", "Wave", "Typewriter"
        };
        ImGui::Text("%s (%s)", label.c_str(), effect_names[effect % 11]);

        // Get canvas position
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        float canvas_height = 80.0f;

        // Setup stagger options
        iam_text_stagger_opts opts;
        opts.pos = ImVec2(canvas_pos.x + 10, canvas_pos.y + canvas_height / 2.0f);
        opts.effect = effect;
        opts.char_delay = char_delay;
        opts.char_duration = char_duration;
        opts.effect_intensity = intensity;
        opts.ease = iam_ease_preset(static_cast<iam_ease_type>(easing));
        opts.color = color;
        opts.font = nullptr;
        opts.font_scale = font_scale;
        opts.letter_spacing = letter_spacing;

        // Draw background
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        float canvas_width = ImGui::GetContentRegionAvail().x;
        draw_list->AddRectFilled(canvas_pos,
            ImVec2(canvas_pos.x + canvas_width, canvas_pos.y + canvas_height),
            IM_COL32(25, 30, 40, 255));

        // Render animated text
        iam_text_stagger(id, text.c_str(), progress, opts);

        ImGui::Dummy(ImVec2(canvas_width, canvas_height));

        // Progress bar
        ImGui::ProgressBar(progress, ImVec2(-1, 0));

        // Restart button
        if (ImGui::Button("Restart")) {
            // Force restart by changing target briefly
            _restart_pending = true;
        }

        // Info
        ImGui::TextDisabled("Chars: %zu, Delay: %.2fs, Duration: %.2fs",
            text.length(), char_delay, char_duration);

        _data_bag->set("progress", Value(static_cast<double>(progress)));

        return Ok();
    }

private:
    bool _restart_pending = false;
};

} // namespace ymery::plugins::imanim
