#pragma once

#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"
#include <imgui.h>
#include <ImGuizmo.h>

namespace ymery::plugins::imguizmo {

class Gizmo : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Gizmo>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Gizmo::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        std::string operation = "translate";
        if (auto res = _data_bag->get_static("operation"); res) {
            if (auto o = get_as<std::string>(*res)) {
                operation = *o;
            }
        }

        std::string mode = "local";
        if (auto res = _data_bag->get_static("mode"); res) {
            if (auto m = get_as<std::string>(*res)) {
                mode = *m;
            }
        }

        ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
        if (operation == "translate") op = ImGuizmo::TRANSLATE;
        else if (operation == "rotate") op = ImGuizmo::ROTATE;
        else if (operation == "scale") op = ImGuizmo::SCALE;
        else if (operation == "universal") op = ImGuizmo::UNIVERSAL;

        ImGuizmo::MODE md = ImGuizmo::LOCAL;
        if (mode == "local") md = ImGuizmo::LOCAL;
        else if (mode == "world") md = ImGuizmo::WORLD;

        // Set the gizmo drawing rect to the window
        ImGuizmo::SetDrawlist();
        ImVec2 window_pos = ImGui::GetWindowPos();
        ImVec2 window_size = ImGui::GetWindowSize();
        ImGuizmo::SetRect(window_pos.x, window_pos.y, window_size.x, window_size.y);

        // Identity matrices for demo - in real use, get from data_bag
        static float view[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, -5.0f, 1.0f
        };
        static float projection[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, -1.0f, -1.0f,
            0.0f, 0.0f, -0.2f, 0.0f
        };
        static float matrix[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };

        ImGuizmo::Manipulate(view, projection, op, md, matrix);

        return Ok();
    }
};

} // namespace ymery::plugins::imguizmo
