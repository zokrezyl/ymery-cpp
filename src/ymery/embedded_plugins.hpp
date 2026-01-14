#pragma once

#include "plugin.hpp"
#include "types.hpp"
#include "result.hpp"

// Forward declarations
namespace ymery {
    class Dispatcher;
    class PluginManager;
}

namespace ymery::embedded {

// Frontend plugins (Widget-based)
Plugin* create_imgui_plugin();

// Backend plugins (TreeLike-based)
Result<TreeLikePtr> create_data_tree();
Result<TreeLikePtr> create_simple_data_tree();
Result<TreeLikePtr> create_audio_file_manager();
Result<TreeLikePtr> create_waveform_manager();
Result<TreeLikePtr> create_kernel(std::shared_ptr<Dispatcher> dispatcher, std::shared_ptr<PluginManager> plugin_manager);

} // namespace ymery::embedded
