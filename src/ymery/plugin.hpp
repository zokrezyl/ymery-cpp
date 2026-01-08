#pragma once

#include "result.hpp"
#include "types.hpp"
#include <memory>
#include <string>
#include <vector>

namespace ymery {

// Forward declarations
class Widget;
class WidgetFactory;
class Dispatcher;
class DataBag;
class PluginManager;

using WidgetPtr = std::shared_ptr<Widget>;

// Plugin - base class for all plugins
// Each plugin can create multiple widget types
// YAML references widgets as: plugin-name.widget-name (e.g., "imgui.button")
class Plugin {
public:
    Plugin() = default;
    virtual ~Plugin() = default;

    // Plugin name (e.g., "imgui", "implot", "imspinner")
    virtual const char* name() const = 0;

    // List of widget names this plugin can create
    virtual std::vector<std::string> widgets() const = 0;

    // Create a widget by name
    virtual Result<WidgetPtr> createWidget(
        const std::string& widget_name,
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) = 0;
};

using PluginPtr = std::shared_ptr<Plugin>;

// Raw function pointer type for plugin creation (for dlsym)
// Returns void* pointing to heap-allocated Plugin - caller takes ownership
using PluginCreateFnPtr = void*(*)();

} // namespace ymery
