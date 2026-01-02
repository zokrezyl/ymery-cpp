// docking-main-window widget plugin - provides full-screen dockspace
#include "../../frontend/composite.hpp"
#include "../../frontend/widget_factory.hpp"
#include "../../types.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <vector>
#include <map>
#include <spdlog/spdlog.h>

namespace ymery::plugins {

// Forward declarations for docking child widgets
struct DockingSplitInfo {
    std::string initial_dock;
    std::string new_dock;
    ImGuiDir direction;
    float ratio;
};

struct DockableWindowInfo {
    std::string label;
    std::string dock_space_name;
    WidgetPtr widget;
};

struct MenuBarInfo {
    WidgetPtr widget;
};

class DockingMainWindow : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<DockingMainWindow>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("DockingMainWindow::create failed", res);
        }
        return widget;
    }

    Result<void> init() override {
        // Get window title
        if (auto res = _data_bag->get("title"); res && res->has_value()) {
            if (auto t = get_as<std::string>(*res)) {
                _title = *t;
            }
        }

        return Composite::init();
    }

    Result<void> render() override {
        // Ensure children are created
        if (auto res = _ensure_children(); !res) {
            return res;
        }

        // Classify children into splits, dockable windows, and regular widgets
        _classify_children();

        // Menu bar - EXACTLY like ymery-editor render_menu_bar()
        if (ImGui::BeginMainMenuBar()) {
            if (_menu_bar.widget) {
                _menu_bar.widget->render();
            }
            ImGui::EndMainMenuBar();
        }

        // Dockspace - EXACTLY like ymery-editor render_dockspace()
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

        // Create the dockspace
        ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        // First time setup - INLINE like ymery-editor (static bool)
        static bool first_time = true;
        if (first_time && !_splits.empty()) {
            first_time = false;

            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_None);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

            // Store the main dock space
            _dock_ids["MainDockSpace"] = dockspace_id;

            // Process splits
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

            // Dock windows to their spaces
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

        // Render dockable windows - EXACTLY like ymery-editor
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
    std::string _title = "Ymery";
    bool _layout_initialized = false;
    bool _children_classified = false;

    std::vector<DockingSplitInfo> _splits;
    std::vector<DockableWindowInfo> _dockable_windows;
    std::vector<WidgetPtr> _regular_widgets;
    MenuBarInfo _menu_bar;
    std::map<std::string, ImGuiID> _dock_ids;

    void _classify_children() {
        if (_children_classified) return;
        _children_classified = true;

        _splits.clear();
        _dockable_windows.clear();
        _regular_widgets.clear();
        _menu_bar.widget = nullptr;

        for (auto& child : _children) {
            // Check if it's a docking-split by checking the widget type
            auto child_bag = child->data_bag();
            if (!child_bag) continue;

            // Get widget type from statics
            std::string widget_type;
            if (auto res = child_bag->get("type"); res && res->has_value()) {
                if (auto t = get_as<std::string>(*res)) {
                    widget_type = *t;
                }
            }

            spdlog::debug("DockingMainWindow: child widget_type = '{}'", widget_type);

            if (widget_type == "menu-bar" || widget_type == "main-menu-bar") {
                _menu_bar.widget = child;
                spdlog::debug("DockingMainWindow: found menu-bar widget");
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

                spdlog::debug("DockingMainWindow: split initial='{}' new='{}' dir='{}' ratio={}",
                    split.initial_dock, split.new_dock, dir_str, split.ratio);
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

                spdlog::debug("DockingMainWindow: dockable-window label='{}' dock='{}'",
                    dw.label, dw.dock_space_name);
                _dockable_windows.push_back(dw);
            }
            else {
                _regular_widgets.push_back(child);
            }
        }

        spdlog::debug("DockingMainWindow: classified {} splits, {} dockable_windows, {} regular",
            _splits.size(), _dockable_windows.size(), _regular_widgets.size());
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "docking-main-window"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::DockingMainWindow::create(wf, d, ns, db);
}
