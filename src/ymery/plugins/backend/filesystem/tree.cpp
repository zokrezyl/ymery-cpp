// Filesystem tree-like plugin
// Exposes filesystem as a tree-like plugin for direct use with data-tree: filesystem
#include "common.hpp"
#include "../../../plugin_export.hpp"

// Forward declarations
namespace ymery { class Dispatcher; class PluginManager; }

extern "C" const char* PLUGIN_EXPORT_NAME() { return "filesystem"; }
extern "C" const char* PLUGIN_EXPORT_TYPE() { return "tree-like"; }
extern "C" ymery::Result<ymery::TreeLikePtr> PLUGIN_EXPORT_CREATE(
    std::shared_ptr<ymery::Dispatcher> /*dispatcher*/,
    std::shared_ptr<ymery::PluginManager> /*plugin_manager*/
) {
    return ymery::plugins::FilesystemManager::create();
}
