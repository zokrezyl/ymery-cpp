// Filesystem device-manager plugin
// Exposes filesystem as a device-manager for use by kernel at /providers/filesystem
#include "common.hpp"

// Forward declarations
namespace ymery { class Dispatcher; class PluginManager; }

extern "C" const char* name() { return "filesystem"; }
extern "C" const char* type() { return "device-manager"; }
extern "C" void* create(
    std::shared_ptr<ymery::Dispatcher> /*dispatcher*/,
    std::shared_ptr<ymery::PluginManager> /*plugin_manager*/
) {
    return static_cast<void*>(new ymery::Result<ymery::TreeLikePtr>(ymery::plugins::FilesystemManager::create()));
}
