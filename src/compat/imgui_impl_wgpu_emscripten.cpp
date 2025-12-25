// ImGui WebGPU backend wrapper for Emscripten compatibility
// Emscripten uses an older WebGPU API that differs from wgpu-native/Dawn
//
// Key differences patched here:
// 1. WGPUShaderModuleWGSLDescriptor: 'source' instead of 'code'
// 2. WGPUMipmapFilterMode_* -> WGPUFilterMode_*

#ifdef __EMSCRIPTEN__

// Compatibility defines must come before including the original
#define WGPUMipmapFilterMode_Linear WGPUFilterMode_Linear
#define WGPUMipmapFilterMode_Nearest WGPUFilterMode_Nearest

// Patch the struct field name: code -> source
// The original code does: wgsl_desc.code = wgsl_source;
// We redefine to make it work with Emscripten's 'source' field
#define code source

// Include the original ImGui WebGPU backend
// The CPM download puts it in the build directory under _deps/imgui-src/
#include "imgui_impl_wgpu.cpp"

#undef code

#else
#error "This file is only for Emscripten builds"
#endif
