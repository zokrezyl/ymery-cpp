#pragma once

#include "result.hpp"
#include "types.hpp"
#include <memory>
#include <string>

namespace ymery {

// Forward declarations
class Widget;
class WidgetFactory;
class Dispatcher;
class DataBag;

using WidgetPtr = std::shared_ptr<Widget>;

// Plugin create function signature - same for all widget plugins
using PluginCreateFn = Result<WidgetPtr>(*)(
    std::shared_ptr<WidgetFactory> widget_factory,
    std::shared_ptr<Dispatcher> dispatcher,
    const std::string& ns,
    std::shared_ptr<DataBag> data_bag
);

// Each plugin .so exports these C functions:
// extern "C" const char* name();     // e.g. "text", "button"
// extern "C" const char* type();     // "widget" or "tree-like"
// extern "C" Result<WidgetPtr> create(...);

} // namespace ymery
