#pragma once
#include "../../../frontend/widget.hpp"
#include <imgui.h>
#include <im_anim.h>

namespace ymery::plugins::imanim {

class IntTween : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<IntTween>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;
        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("IntTween::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        iam_update_begin_frame();
        float dt = ImGui::GetIO().DeltaTime;
        if (dt <= 0.0f) dt = 1.0f / 60.0f;
        if (dt > 0.1f) dt = 0.1f;

        std::string label = "Int Tween";
        if (auto res = _data_bag->get_static("label"); res) {
            if (auto l = get_as<std::string>(*res)) label = *l;
        }

        int target = 100;
        if (auto res = _data_bag->get("target"); res) {
            if (auto t = get_as<double>(*res)) target = static_cast<int>(*t);
        }

        float duration = 1.0f;
        if (auto res = _data_bag->get_static("duration"); res) {
            if (auto d = get_as<double>(*res)) duration = static_cast<float>(*d);
        }

        int easing = iam_ease_out_cubic;
        if (auto res = _data_bag->get_static("easing"); res) {
            if (auto e = get_as<double>(*res)) easing = static_cast<int>(*e);
        }

        // Display format (decimal, hex, binary)
        std::string format = "decimal";
        if (auto res = _data_bag->get_static("format"); res) {
            if (auto f = get_as<std::string>(*res)) format = *f;
        }

        ImGuiID id = ImGui::GetID(_uid.c_str());
        int value = iam_tween_int(id, 0, target, duration,
            iam_ease_preset(static_cast<iam_ease_type>(easing)), iam_policy_crossfade, dt);

        ImGui::Text("%s", label.c_str());

        // Large number display
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts.Size > 0 ? ImGui::GetIO().Fonts->Fonts[0] : nullptr);
        if (format == "hex") {
            ImGui::Text("0x%08X", value);
        } else if (format == "binary") {
            // Display as binary with groups of 4
            char bin[40];
            int idx = 0;
            for (int i = 31; i >= 0; i--) {
                bin[idx++] = ((value >> i) & 1) ? '1' : '0';
                if (i > 0 && i % 4 == 0) bin[idx++] = ' ';
            }
            bin[idx] = '\0';
            ImGui::TextWrapped("%s", bin);
        } else {
            ImGui::Text("%d", value);
        }
        ImGui::PopFont();

        // Progress bar
        float progress = static_cast<float>(value) / static_cast<float>(target);
        ImGui::ProgressBar(progress, ImVec2(-1, 0));

        // Show target
        ImGui::TextDisabled("Target: %d", target);

        _data_bag->set("value", Value(static_cast<double>(value)));

        return Ok();
    }
};

} // namespace ymery::plugins::imanim
