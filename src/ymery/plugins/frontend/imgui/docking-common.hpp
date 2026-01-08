#pragma once
// Common types for docking widgets
#include "../../../frontend/widget.hpp"
#include <imgui.h>
#include <string>
#include <vector>
#include <map>

namespace ymery::plugins::imgui {

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

} // namespace ymery::plugins::imgui
