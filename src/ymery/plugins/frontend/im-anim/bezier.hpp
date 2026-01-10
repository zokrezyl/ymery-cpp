#pragma once
#include "../../../frontend/widget.hpp"
#include <imgui.h>
#include <im_anim.h>
#include <cmath>

namespace ymery::plugins::imanim {

class Bezier : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Bezier>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;
        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Bezier::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        iam_update_begin_frame();
        float dt = ImGui::GetIO().DeltaTime;
        if (dt <= 0.0f) dt = 1.0f / 60.0f;
        if (dt > 0.1f) dt = 0.1f;

        std::string label = "Bezier Path";
        if (auto res = _data_bag->get_static("label"); res) {
            if (auto l = get_as<std::string>(*res)) label = *l;
        }

        // Control points for cubic bezier
        float p0x = 20.0f, p0y = 150.0f;
        float p1x = 60.0f, p1y = 20.0f;
        float p2x = 180.0f, p2y = 20.0f;
        float p3x = 220.0f, p3y = 150.0f;

        if (auto res = _data_bag->get_static("p0-x"); res) {
            if (auto v = get_as<double>(*res)) p0x = static_cast<float>(*v);
        }
        if (auto res = _data_bag->get_static("p0-y"); res) {
            if (auto v = get_as<double>(*res)) p0y = static_cast<float>(*v);
        }
        if (auto res = _data_bag->get_static("p1-x"); res) {
            if (auto v = get_as<double>(*res)) p1x = static_cast<float>(*v);
        }
        if (auto res = _data_bag->get_static("p1-y"); res) {
            if (auto v = get_as<double>(*res)) p1y = static_cast<float>(*v);
        }
        if (auto res = _data_bag->get_static("p2-x"); res) {
            if (auto v = get_as<double>(*res)) p2x = static_cast<float>(*v);
        }
        if (auto res = _data_bag->get_static("p2-y"); res) {
            if (auto v = get_as<double>(*res)) p2y = static_cast<float>(*v);
        }
        if (auto res = _data_bag->get_static("p3-x"); res) {
            if (auto v = get_as<double>(*res)) p3x = static_cast<float>(*v);
        }
        if (auto res = _data_bag->get_static("p3-y"); res) {
            if (auto v = get_as<double>(*res)) p3y = static_cast<float>(*v);
        }

        float duration = 2.0f;
        if (auto res = _data_bag->get_static("duration"); res) {
            if (auto d = get_as<double>(*res)) duration = static_cast<float>(*d);
        }

        int easing = iam_ease_in_out_cubic;
        if (auto res = _data_bag->get_static("easing"); res) {
            if (auto e = get_as<double>(*res)) easing = static_cast<int>(*e);
        }

        bool show_controls = true;
        if (auto res = _data_bag->get_static("show-controls"); res) {
            if (auto s = get_as<bool>(*res)) show_controls = *s;
        }

        bool loop = true;
        if (auto res = _data_bag->get_static("loop"); res) {
            if (auto l = get_as<bool>(*res)) loop = *l;
        }

        float object_size = 12.0f;
        if (auto res = _data_bag->get_static("object-size"); res) {
            if (auto s = get_as<double>(*res)) object_size = static_cast<float>(*s);
        }

        ImGuiID id = ImGui::GetID(_uid.c_str());

        // Animate t parameter from 0 to 1
        float target_t = loop ? 1.0f : 1.0f;
        float t = iam_tween_float(id, 0, target_t, duration,
            iam_ease_preset(static_cast<iam_ease_type>(easing)), iam_policy_crossfade, dt);

        // Calculate position on cubic bezier curve
        ImVec2 p0(p0x, p0y), p1(p1x, p1y), p2(p2x, p2y), p3(p3x, p3y);
        ImVec2 pos = iam_bezier_cubic(p0, p1, p2, p3, t);
        ImVec2 tangent = iam_bezier_cubic_deriv(p0, p1, p2, p3, t);

        // Normalize tangent for direction indicator
        float tangent_len = sqrtf(tangent.x * tangent.x + tangent.y * tangent.y);
        if (tangent_len > 0.001f) {
            tangent.x /= tangent_len;
            tangent.y /= tangent_len;
        }

        ImGui::Text("%s", label.c_str());

        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        float canvas_width = 250.0f, canvas_height = 180.0f;
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // Draw canvas background
        draw_list->AddRectFilled(canvas_pos,
            ImVec2(canvas_pos.x + canvas_width, canvas_pos.y + canvas_height),
            IM_COL32(30, 35, 42, 255));
        draw_list->AddRect(canvas_pos,
            ImVec2(canvas_pos.x + canvas_width, canvas_pos.y + canvas_height),
            IM_COL32(65, 70, 80, 255));

        // Offset points to canvas
        auto offset = [&](ImVec2 p) { return ImVec2(canvas_pos.x + p.x, canvas_pos.y + p.y); };

        // Draw control lines if enabled
        if (show_controls) {
            draw_list->AddLine(offset(p0), offset(p1), IM_COL32(100, 100, 100, 150), 1.0f);
            draw_list->AddLine(offset(p2), offset(p3), IM_COL32(100, 100, 100, 150), 1.0f);

            // Draw control points
            draw_list->AddCircleFilled(offset(p0), 5, IM_COL32(60, 160, 60, 255));
            draw_list->AddCircleFilled(offset(p1), 5, IM_COL32(160, 100, 60, 255));
            draw_list->AddCircleFilled(offset(p2), 5, IM_COL32(160, 100, 60, 255));
            draw_list->AddCircleFilled(offset(p3), 5, IM_COL32(160, 60, 60, 255));
        }

        // Draw bezier curve
        for (int i = 0; i < 50; i++) {
            float t1 = i / 50.0f;
            float t2 = (i + 1) / 50.0f;
            ImVec2 pt1 = iam_bezier_cubic(p0, p1, p2, p3, t1);
            ImVec2 pt2 = iam_bezier_cubic(p0, p1, p2, p3, t2);
            draw_list->AddLine(offset(pt1), offset(pt2), IM_COL32(100, 150, 220, 200), 2.0f);
        }

        // Draw moving object
        ImVec2 obj_pos = offset(pos);
        draw_list->AddCircleFilled(obj_pos, object_size, IM_COL32(255, 180, 80, 255));
        draw_list->AddCircle(obj_pos, object_size, IM_COL32(255, 220, 140, 255), 0, 2.0f);

        // Draw direction indicator
        float indicator_len = 20.0f;
        ImVec2 indicator_end(obj_pos.x + tangent.x * indicator_len, obj_pos.y + tangent.y * indicator_len);
        draw_list->AddLine(obj_pos, indicator_end, IM_COL32(255, 100, 100, 255), 2.0f);

        ImGui::Dummy(ImVec2(canvas_width, canvas_height));

        // Show info
        ImGui::Text("t: %.3f", t);
        ImGui::Text("Pos: (%.1f, %.1f)", pos.x, pos.y);

        // Output values
        _data_bag->set("t", Value(static_cast<double>(t)));
        _data_bag->set("x", Value(static_cast<double>(pos.x)));
        _data_bag->set("y", Value(static_cast<double>(pos.y)));

        return Ok();
    }
};

} // namespace ymery::plugins::imanim
