#pragma once
#include "../../../frontend/composite.hpp"
#include "docking-common.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <ytrace/ytrace.hpp>
#include <vector>
#include <map>

namespace ymery::plugins::imgui {

class HelloImguiMainWindow : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<HelloImguiMainWindow>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;
        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("HelloImguiMainWindow::create failed", res);
        }
        return widget;
    }

    Result<void> render() override {
        if (auto res = _ensure_children(); !res) {
            return res;
        }

        _classify_children();

        // Menu bar
        if (ImGui::BeginMainMenuBar()) {
            for (auto& menu_widget : _menu_widgets) {
                menu_widget->render();
            }
            ImGui::EndMainMenuBar();
        }

        // Dockspace
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking |
                                         ImGuiWindowFlags_NoTitleBar |
                                         ImGuiWindowFlags_NoCollapse |
                                         ImGuiWindowFlags_NoResize |
                                         ImGuiWindowFlags_NoMove |
                                         ImGuiWindowFlags_NoBringToFrontOnFocus |
                                         ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("DockSpaceWindow", nullptr, window_flags);
        ImGui::PopStyleVar(3);

        ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        // First time setup
        if (!_layout_initialized && !_splits.empty()) {
            _layout_initialized = true;

            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_None);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

            _dock_ids["MainDockSpace"] = dockspace_id;

            for (const auto& split : _splits) {
                ImGuiID initial_id = dockspace_id;
                if (auto it = _dock_ids.find(split.initial_dock); it != _dock_ids.end()) {
                    initial_id = it->second;
                }

                ImGuiID new_id;
                ImGuiID remaining_id;
                ImGui::DockBuilderSplitNode(initial_id, split.direction, split.ratio, &new_id, &remaining_id);

                _dock_ids[split.new_dock] = new_id;
                _dock_ids[split.initial_dock] = remaining_id;
            }

            for (const auto& dw : _dockable_windows) {
                ImGuiID dock_id = dockspace_id;
                if (auto it = _dock_ids.find(dw.dock_space_name); it != _dock_ids.end()) {
                    dock_id = it->second;
                }
                ImGui::DockBuilderDockWindow(dw.label.c_str(), dock_id);
            }

            ImGui::DockBuilderFinish(dockspace_id);
        }

        ImGui::End();

        // Render dockable windows
        for (const auto& dw : _dockable_windows) {
            ImGui::Begin(dw.label.c_str());
            if (dw.widget) {
                dw.widget->render();
            }
            ImGui::End();
        }

        // Render regular widgets
        for (auto& widget : _regular_widgets) {
            widget->render();
        }

        return Ok();
    }

private:
    bool _layout_initialized = false;
    bool _children_classified = false;

    std::vector<DockingSplitInfo> _splits;
    std::vector<DockableWindowInfo> _dockable_windows;
    std::vector<WidgetPtr> _regular_widgets;
    std::vector<WidgetPtr> _menu_widgets;
    std::map<std::string, ImGuiID> _dock_ids;

    void _classify_children() {
        if (_children_classified) return;
        _children_classified = true;

        _splits.clear();
        _dockable_windows.clear();
        _regular_widgets.clear();
        _menu_widgets.clear();

        for (auto& child : _children) {
            auto child_bag = child->data_bag();
            if (!child_bag) continue;

            std::string widget_type;
            if (auto res = child_bag->get("type"); res && res->has_value()) {
                if (auto t = get_as<std::string>(*res)) {
                    widget_type = *t;
                }
            }

            ydebug("HelloImguiMainWindow: child widget_type = '{}'", widget_type);

            if (widget_type == "hello-imgui-menu" || widget_type == "hello-imgui-app-menu-items" ||
                widget_type == "menu-bar" || widget_type == "main-menu-bar") {
                _menu_widgets.push_back(child);
            }
            else if (widget_type == "docking-split") {
                DockingSplitInfo split;

                if (auto res = child_bag->get("initial-dock"); res && res->has_value()) {
                    if (auto s = get_as<std::string>(*res)) split.initial_dock = *s;
                }
                if (auto res = child_bag->get("new-dock"); res && res->has_value()) {
                    if (auto s = get_as<std::string>(*res)) split.new_dock = *s;
                }
                if (auto res = child_bag->get("ratio"); res && res->has_value()) {
                    if (auto d = get_as<double>(*res)) split.ratio = static_cast<float>(*d);
                    else if (auto f = get_as<float>(*res)) split.ratio = *f;
                    else if (auto i = get_as<int>(*res)) split.ratio = static_cast<float>(*i) / 100.0f;
                    else split.ratio = 0.5f;
                } else {
                    split.ratio = 0.5f;
                }

                std::string dir_str = "down";
                if (auto res = child_bag->get("direction"); res && res->has_value()) {
                    if (auto s = get_as<std::string>(*res)) dir_str = *s;
                }
                if (dir_str == "left") split.direction = ImGuiDir_Left;
                else if (dir_str == "right") split.direction = ImGuiDir_Right;
                else if (dir_str == "up") split.direction = ImGuiDir_Up;
                else split.direction = ImGuiDir_Down;

                ydebug("HelloImguiMainWindow: split initial='{}' new='{}' ratio={}",
                    split.initial_dock, split.new_dock, split.ratio);
                _splits.push_back(split);
            }
            else if (widget_type == "dockable-window") {
                DockableWindowInfo dw;

                if (auto res = child_bag->get("label"); res && res->has_value()) {
                    if (auto s = get_as<std::string>(*res)) dw.label = *s;
                }
                if (auto res = child_bag->get("dock-space-name"); res && res->has_value()) {
                    if (auto s = get_as<std::string>(*res)) dw.dock_space_name = *s;
                } else {
                    dw.dock_space_name = "MainDockSpace";
                }
                dw.widget = child;

                ydebug("HelloImguiMainWindow: dockable-window label='{}' dock='{}'",
                    dw.label, dw.dock_space_name);
                _dockable_windows.push_back(dw);
            }
            else {
                _regular_widgets.push_back(child);
            }
        }

        ydebug("HelloImguiMainWindow: {} splits, {} dockable_windows, {} menus, {} regular",
            _splits.size(), _dockable_windows.size(), _menu_widgets.size(), _regular_widgets.size());
    }
};

} // namespace ymery::plugins::imgui
