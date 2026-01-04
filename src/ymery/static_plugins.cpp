// Static plugin registry - no longer used
// Plugins are now loaded dynamically on all platforms (including web via dlopen)

#include "static_plugins.hpp"

namespace ymery {

const std::vector<StaticPlugin>& get_static_plugins() {
    static std::vector<StaticPlugin> empty;
    return empty;
}

} // namespace ymery
