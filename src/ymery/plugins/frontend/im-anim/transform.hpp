#pragma once
#include "../../../frontend/widget.hpp"
#include <imgui.h>
#include <im_anim.h>
#include <cmath>

namespace ymery::plugins::imanim {

class Transform : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Transform>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;
        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Transform::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        iam_update_begin_frame();
        float dt = ImGui::GetIO().DeltaTime;
        if (dt <= 0.0f) dt = 1.0f / 60.0f;
        if (dt > 0.1f) dt = 0.1f;

        std::string label = "Transform";
        if (auto res = _data_bag->get_static("label"); res) {
            if (auto l = get_as<std::string>(*res)) label = *l;
        }

        // Target transform values
        float target_x = 100.0f, target_y = 50.0f;
        float target_rotation = 45.0f;  // degrees
        float target_scale_x = 1.5f, target_scale_y = 1.5f;

        if (auto res = _data_bag->get("target-x"); res) {
            if (auto v = get_as<double>(*res)) target_x = static_cast<float>(*v);
        }
        if (auto res = _data_bag->get("target-y"); res) {
            if (auto v = get_as<double>(*res)) target_y = static_cast<float>(*v);
        }
        if (auto res = _data_bag->get("target-rotation"); res) {
            if (auto v = get_as<double>(*res)) target_rotation = static_cast<float>(*v);
        }
        if (auto res = _data_bag->get("target-scale"); res) {
            if (auto v = get_as<double>(*res)) target_scale_x = target_scale_y = static_cast<float>(*v);
        }
        if (auto res = _data_bag->get("target-scale-x"); res) {
            if (auto v = get_as<double>(*res)) target_scale_x = static_cast<float>(*v);
        }
        if (auto res = _data_bag->get("target-scale-y"); res) {
            if (auto v = get_as<double>(*res)) target_scale_y = static_cast<float>(*v);
        }

        float duration = 1.5f;
        if (auto res = _data_bag->get_static("duration"); res) {
            if (auto d = get_as<double>(*res)) duration = static_cast<float>(*d);
        }

        int easing = iam_ease_out_back;
        if (auto res = _data_bag->get_static("easing"); res) {
            if (auto e = get_as<double>(*res)) easing = static_cast<int>(*e);
        }

        int rotation_mode = iam_rotation_shortest;
        if (auto res = _data_bag->get_static("rotation-mode"); res) {
            if (auto m = get_as<double>(*res)) rotation_mode = static_cast<int>(*m);
        }

        // Canvas dimensions
        float canvas_width = 250.0f, canvas_height = 200.0f;
        if (auto res = _data_bag->get_static("width"); res) {
            if (auto w = get_as<double>(*res)) canvas_width = static_cast<float>(*w);
        }
        if (auto res = _data_bag->get_static("height"); res) {
            if (auto h = get_as<double>(*res)) canvas_height = static_cast<float>(*h);
        }

        ImGuiID id = ImGui::GetID(_uid.c_str());

        // Create target transform (convert degrees to radians)
        iam_transform target_tf;
        target_tf.position = ImVec2(target_x, target_y);
        target_tf.rotation = target_rotation * (3.14159265f / 180.0f);
        target_tf.scale = ImVec2(target_scale_x, target_scale_y);

        iam_transform tf = iam_tween_transform(id, 0, target_tf, duration,
            iam_ease_preset(static_cast<iam_ease_type>(easing)), iam_policy_crossfade, rotation_mode, dt);

        ImGui::Text("%s", label.c_str());

        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // Draw canvas
        draw_list->AddRectFilled(canvas_pos,
            ImVec2(canvas_pos.x + canvas_width, canvas_pos.y + canvas_height),
            IM_COL32(30, 35, 42, 255));
        draw_list->AddRect(canvas_pos,
            ImVec2(canvas_pos.x + canvas_width, canvas_pos.y + canvas_height),
            IM_COL32(65, 70, 80, 255));

        // Draw origin marker
        float ox = canvas_pos.x + 20;
        float oy = canvas_pos.y + canvas_height - 20;
        draw_list->AddCircleFilled(ImVec2(ox, oy), 4, IM_COL32(100, 100, 100, 255));

        // Draw transformed rectangle
        float base_w = 40.0f, base_h = 30.0f;
        float hw = base_w * 0.5f * tf.scale.x;
        float hh = base_h * 0.5f * tf.scale.y;

        // Calculate center position
        float cx = ox + tf.position.x;
        float cy = oy - tf.position.y;  // Y is flipped for screen coords

        // Calculate rotated corners
        float cos_r = cosf(tf.rotation);
        float sin_r = sinf(tf.rotation);

        ImVec2 corners[4];
        float dx[4] = {-hw, hw, hw, -hw};
        float dy[4] = {-hh, -hh, hh, hh};

        for (int i = 0; i < 4; i++) {
            corners[i].x = cx + dx[i] * cos_r - dy[i] * sin_r;
            corners[i].y = cy + dx[i] * sin_r + dy[i] * cos_r;
        }

        // Draw filled rotated rectangle
        draw_list->AddQuadFilled(corners[0], corners[1], corners[2], corners[3],
            IM_COL32(80, 140, 220, 200));
        draw_list->AddQuad(corners[0], corners[1], corners[2], corners[3],
            IM_COL32(120, 180, 255, 255), 2.0f);

        // Draw direction indicator
        float indicator_len = 20.0f * tf.scale.x;
        ImVec2 indicator_end(cx + indicator_len * cos_r, cy + indicator_len * sin_r);
        draw_list->AddLine(ImVec2(cx, cy), indicator_end, IM_COL32(255, 200, 100, 255), 2.0f);

        ImGui::Dummy(ImVec2(canvas_width, canvas_height));

        // Show current values
        float rotation_deg = tf.rotation * (180.0f / 3.14159265f);
        ImGui::Text("Pos: (%.1f, %.1f)", tf.position.x, tf.position.y);
        ImGui::Text("Rot: %.1f deg", rotation_deg);
        ImGui::Text("Scale: (%.2f, %.2f)", tf.scale.x, tf.scale.y);

        // Output values
        _data_bag->set("x", Value(static_cast<double>(tf.position.x)));
        _data_bag->set("y", Value(static_cast<double>(tf.position.y)));
        _data_bag->set("rotation", Value(static_cast<double>(rotation_deg)));
        _data_bag->set("scale-x", Value(static_cast<double>(tf.scale.x)));
        _data_bag->set("scale-y", Value(static_cast<double>(tf.scale.y)));

        return Ok();
    }
};

} // namespace ymery::plugins::imanim
