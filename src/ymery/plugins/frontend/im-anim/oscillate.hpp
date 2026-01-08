#pragma once
#include "../../../frontend/widget.hpp"
#include <imgui.h>
#include <im_anim.h>

namespace ymery::plugins::imanim {

class Oscillate : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Oscillate>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;
        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Oscillate::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        iam_update_begin_frame();
        float dt = ImGui::GetIO().DeltaTime;
        if (dt <= 0.0f) dt = 1.0f / 60.0f;
        if (dt > 0.1f) dt = 0.1f;

        std::string label = "Oscillate";
        if (auto res = _data_bag->get_static("label"); res) {
            if (auto l = get_as<std::string>(*res)) label = *l;
        }

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

        ImGuiID id = ImGui::GetID(_uid.c_str());
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

        return Ok();
    }
};

} // namespace ymery::plugins::imanim
