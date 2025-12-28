// Filesystem tree-like plugin
// Exposes filesystem as a tree-like plugin for direct use with data-tree: filesystem
#include "common.hpp"

// Forward declarations
namespace ymery { class Dispatcher; class PluginManager; }

extern "C" const char* name() { return "filesystem"; }
extern "C" const char* type() { return "tree-like"; }
extern "C" ymery::Result<ymery::TreeLikePtr> create(
    std::shared_ptr<ymery::Dispatcher> /*dispatcher*/,
    std::shared_ptr<ymery::PluginManager> /*plugin_manager*/
) {
    return ymery::plugins::FilesystemManager::create();
}
