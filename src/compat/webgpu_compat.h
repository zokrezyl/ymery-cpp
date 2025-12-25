// WebGPU API compatibility shim for Emscripten
// Emscripten's WebGPU headers use an older API than wgpu-native/Dawn
// This header provides compatibility defines

#pragma once

#ifdef __EMSCRIPTEN__

// Emscripten uses 'source' instead of 'code' in WGPUShaderModuleWGSLDescriptor
// We use a macro to map the field name
#ifndef WEBGPU_COMPAT_SOURCE_FIELD
#define WEBGPU_COMPAT_SOURCE_FIELD
// Note: imgui_impl_wgpu.cpp uses wgsl_desc.code = wgsl_source;
// We need to patch this at compile time
#endif

// Emscripten uses WGPUFilterMode for mipmap filter, not WGPUMipmapFilterMode
#ifndef WGPUMipmapFilterMode_Linear
#define WGPUMipmapFilterMode_Linear WGPUFilterMode_Linear
#endif

#ifndef WGPUMipmapFilterMode_Nearest
#define WGPUMipmapFilterMode_Nearest WGPUFilterMode_Nearest
#endif

#endif // __EMSCRIPTEN__
