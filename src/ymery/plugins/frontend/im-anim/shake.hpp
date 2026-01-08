#pragma once
#include "../../../frontend/widget.hpp"
#include <imgui.h>
#include <im_anim.h>

namespace ymery::plugins::imanim {

class Shake : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Shake>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;
        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Shake::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        iam_update_begin_frame();
        float dt = ImGui::GetIO().DeltaTime;
        if (dt <= 0.0f) dt = 1.0f / 60.0f;
        if (dt > 0.1f) dt = 0.1f;

        std::string label = "Shake";
        if (auto res = _data_bag->get_static("label"); res) {
            if (auto l = get_as<std::string>(*res)) label = *l;
        }

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

        ImGuiID id = ImGui::GetID(_uid.c_str());

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

        return Ok();
    }
};

} // namespace ymery::plugins::imanim
