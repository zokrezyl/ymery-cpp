#pragma once
// Static plugin registry for web builds (no dlopen available)

#include "plugin_manager.hpp"  // For WidgetCreateFnPtr, TreeLikeCreateFnPtr
#include <vector>

namespace ymery {

// Static plugin entry
struct StaticPlugin {
    const char* (*name)();
    const char* (*type)();
    void* create;  // Either WidgetCreateFnPtr or TreeLikeCreateFnPtr
};

// Get all static plugins (defined in static_plugins.cpp)
const std::vector<StaticPlugin>& get_static_plugins();

} // namespace ymery
