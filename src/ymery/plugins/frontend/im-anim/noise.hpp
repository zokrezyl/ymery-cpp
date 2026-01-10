#pragma once
#include "../../../frontend/widget.hpp"
#include <imgui.h>
#include <im_anim.h>
#include <cmath>

namespace ymery::plugins::imanim {

class Noise : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Noise>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;
        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Noise::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        iam_update_begin_frame();
        float dt = ImGui::GetIO().DeltaTime;
        if (dt <= 0.0f) dt = 1.0f / 60.0f;
        if (dt > 0.1f) dt = 0.1f;

        std::string label = "Noise";
        if (auto res = _data_bag->get_static("label"); res) {
            if (auto l = get_as<std::string>(*res)) label = *l;
        }

        // Noise type (perlin, simplex, value, worley)
        int noise_type = iam_noise_perlin;
        if (auto res = _data_bag->get_static("type"); res) {
            if (auto t = get_as<double>(*res)) noise_type = static_cast<int>(*t);
        }

        // Amplitude
        float amplitude_x = 40.0f, amplitude_y = 40.0f;
        if (auto res = _data_bag->get_static("amplitude"); res) {
            if (auto a = get_as<double>(*res)) amplitude_x = amplitude_y = static_cast<float>(*a);
        }
        if (auto res = _data_bag->get_static("amplitude-x"); res) {
            if (auto a = get_as<double>(*res)) amplitude_x = static_cast<float>(*a);
        }
        if (auto res = _data_bag->get_static("amplitude-y"); res) {
            if (auto a = get_as<double>(*res)) amplitude_y = static_cast<float>(*a);
        }

        // Frequency
        float frequency = 1.5f;
        if (auto res = _data_bag->get_static("frequency"); res) {
            if (auto f = get_as<double>(*res)) frequency = static_cast<float>(*f);
        }

        // Octaves for fractal noise
        int octaves = 4;
        if (auto res = _data_bag->get_static("octaves"); res) {
            if (auto o = get_as<double>(*res)) octaves = static_cast<int>(*o);
        }

        // Persistence (amplitude decay per octave)
        float persistence = 0.5f;
        if (auto res = _data_bag->get_static("persistence"); res) {
            if (auto p = get_as<double>(*res)) persistence = static_cast<float>(*p);
        }

        // Lacunarity (frequency multiplier per octave)
        float lacunarity = 2.0f;
        if (auto res = _data_bag->get_static("lacunarity"); res) {
            if (auto l = get_as<double>(*res)) lacunarity = static_cast<float>(*l);
        }

        // Seed
        int seed = 0;
        if (auto res = _data_bag->get_static("seed"); res) {
            if (auto s = get_as<double>(*res)) seed = static_cast<int>(*s);
        }

        // Canvas size
        float canvas_width = 200.0f, canvas_height = 150.0f;
        if (auto res = _data_bag->get_static("width"); res) {
            if (auto w = get_as<double>(*res)) canvas_width = static_cast<float>(*w);
        }
        if (auto res = _data_bag->get_static("height"); res) {
            if (auto h = get_as<double>(*res)) canvas_height = static_cast<float>(*h);
        }

        ImGuiID id = ImGui::GetID(_uid.c_str());

        iam_noise_opts opts;
        opts.type = noise_type;
        opts.octaves = octaves;
        opts.persistence = persistence;
        opts.lacunarity = lacunarity;
        opts.seed = seed;

        ImVec2 noise_val = iam_noise_channel_vec2(id,
            ImVec2(frequency, frequency), ImVec2(amplitude_x, amplitude_y), opts, dt);

        const char* type_names[] = {"Perlin", "Simplex", "Value", "Worley"};
        ImGui::Text("%s (%s)", label.c_str(), type_names[noise_type % 4]);

        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // Draw canvas background
        draw_list->AddRectFilled(canvas_pos,
            ImVec2(canvas_pos.x + canvas_width, canvas_pos.y + canvas_height),
            IM_COL32(25, 30, 38, 255));
        draw_list->AddRect(canvas_pos,
            ImVec2(canvas_pos.x + canvas_width, canvas_pos.y + canvas_height),
            IM_COL32(55, 60, 70, 255));

        // Draw center crosshair
        float cx = canvas_pos.x + canvas_width / 2.0f;
        float cy = canvas_pos.y + canvas_height / 2.0f;
        draw_list->AddLine(ImVec2(cx - 15, cy), ImVec2(cx + 15, cy), IM_COL32(50, 55, 65, 255));
        draw_list->AddLine(ImVec2(cx, cy - 15), ImVec2(cx, cy + 15), IM_COL32(50, 55, 65, 255));

        // Draw noise-driven object
        float obj_x = cx + noise_val.x;
        float obj_y = cy + noise_val.y;

        // Trail effect (simple)
        ImU32 trail_color = IM_COL32(180, 120, 255, 80);
        draw_list->AddCircleFilled(ImVec2(obj_x, obj_y), 18, trail_color);

        // Main object
        draw_list->AddCircleFilled(ImVec2(obj_x, obj_y), 14, IM_COL32(180, 120, 255, 255));
        draw_list->AddCircle(ImVec2(obj_x, obj_y), 14, IM_COL32(220, 180, 255, 255), 0, 2.0f);

        ImGui::Dummy(ImVec2(canvas_width, canvas_height));

        // Info display
        ImGui::Text("Offset: (%.2f, %.2f)", noise_val.x, noise_val.y);
        ImGui::TextDisabled("Octaves: %d, Persist: %.2f", octaves, persistence);

        // Output values
        _data_bag->set("x", Value(static_cast<double>(noise_val.x)));
        _data_bag->set("y", Value(static_cast<double>(noise_val.y)));

        return Ok();
    }
};

} // namespace ymery::plugins::imanim
