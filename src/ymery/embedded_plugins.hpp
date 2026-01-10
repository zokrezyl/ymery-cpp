#pragma once

#include "plugin.hpp"
#include "types.hpp"
#include "result.hpp"

namespace ymery::embedded {

// Embedded plugin creation functions
// These are linked directly into the executable

// Frontend plugins (Widget-based)
Plugin* create_imgui_plugin();

// Backend plugins (TreeLike-based)
Result<TreeLikePtr> create_data_tree();
Result<TreeLikePtr> create_simple_data_tree();

} // namespace ymery::embedded
