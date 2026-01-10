#pragma once
#include "../../../frontend/widget.hpp"
#include <imgui.h>
#include <im_anim.h>

namespace ymery::plugins::imanim {

class Wiggle : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Wiggle>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;
        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Wiggle::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        iam_update_begin_frame();
        float dt = ImGui::GetIO().DeltaTime;
        if (dt <= 0.0f) dt = 1.0f / 60.0f;
        if (dt > 0.1f) dt = 0.1f;

        std::string label = "Wiggle";
        if (auto res = _data_bag->get_static("label"); res) {
            if (auto l = get_as<std::string>(*res)) label = *l;
        }

        // Amplitude controls how far it moves
        float amplitude_x = 30.0f, amplitude_y = 30.0f;
        if (auto res = _data_bag->get_static("amplitude"); res) {
            if (auto a = get_as<double>(*res)) {
                amplitude_x = amplitude_y = static_cast<float>(*a);
            }
        }
        if (auto res = _data_bag->get_static("amplitude-x"); res) {
            if (auto a = get_as<double>(*res)) amplitude_x = static_cast<float>(*a);
        }
        if (auto res = _data_bag->get_static("amplitude-y"); res) {
            if (auto a = get_as<double>(*res)) amplitude_y = static_cast<float>(*a);
        }

        // Frequency controls how fast it wiggles
        float frequency = 2.0f;
        if (auto res = _data_bag->get_static("frequency"); res) {
            if (auto f = get_as<double>(*res)) frequency = static_cast<float>(*f);
        }

        // Canvas size
        float canvas_width = 200.0f, canvas_height = 150.0f;
        if (auto res = _data_bag->get_static("width"); res) {
            if (auto w = get_as<double>(*res)) canvas_width = static_cast<float>(*w);
        }
        if (auto res = _data_bag->get_static("height"); res) {
            if (auto h = get_as<double>(*res)) canvas_height = static_cast<float>(*h);
        }

        // Object size
        float object_size = 20.0f;
        if (auto res = _data_bag->get_static("object-size"); res) {
            if (auto s = get_as<double>(*res)) object_size = static_cast<float>(*s);
        }

        ImGuiID id = ImGui::GetID(_uid.c_str());
        ImVec2 wiggle = iam_wiggle_vec2(id, ImVec2(amplitude_x, amplitude_y), frequency, dt);

        ImGui::Text("%s", label.c_str());

        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // Draw canvas background
        draw_list->AddRectFilled(canvas_pos,
            ImVec2(canvas_pos.x + canvas_width, canvas_pos.y + canvas_height),
            IM_COL32(35, 38, 45, 255));
        draw_list->AddRect(canvas_pos,
            ImVec2(canvas_pos.x + canvas_width, canvas_pos.y + canvas_height),
            IM_COL32(70, 75, 85, 255));

        // Draw center crosshair
        float cx = canvas_pos.x + canvas_width / 2.0f;
        float cy = canvas_pos.y + canvas_height / 2.0f;
        draw_list->AddLine(ImVec2(cx - 10, cy), ImVec2(cx + 10, cy), IM_COL32(60, 65, 75, 255));
        draw_list->AddLine(ImVec2(cx, cy - 10), ImVec2(cx, cy + 10), IM_COL32(60, 65, 75, 255));

        // Draw wiggling object
        float obj_x = cx + wiggle.x;
        float obj_y = cy + wiggle.y;
        draw_list->AddCircleFilled(ImVec2(obj_x, obj_y), object_size,
            IM_COL32(120, 220, 150, 255));
        draw_list->AddCircle(ImVec2(obj_x, obj_y), object_size,
            IM_COL32(80, 180, 110, 255), 0, 2.0f);

        ImGui::Dummy(ImVec2(canvas_width, canvas_height));
        ImGui::Text("Offset: (%.2f, %.2f)", wiggle.x, wiggle.y);

        // Output values
        _data_bag->set("x", Value(static_cast<double>(wiggle.x)));
        _data_bag->set("y", Value(static_cast<double>(wiggle.y)));

        return Ok();
    }
};

} // namespace ymery::plugins::imanim
