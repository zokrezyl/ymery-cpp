// shader-toy widget plugin - WebGPU custom shader POC
// Only available when YMERY_USE_WEBGPU is defined

#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>

#ifdef YMERY_USE_WEBGPU
#include <webgpu/webgpu.h>
#include "backends/imgui_impl_wgpu.h"
#include <chrono>
#include <cstring>

namespace ymery::plugins {

// Simple ShaderToy-style uniforms
struct ShaderToyUniforms {
    float iResolution[2];  // viewport resolution
    float iTime;           // time in seconds
    float _pad;            // padding for alignment
};

// WGSL shader source - simple animated gradient
static const char* SHADER_SOURCE = R"(
struct Uniforms {
    iResolution: vec2<f32>,
    iTime: f32,
    _pad: f32,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
    // Fullscreen triangle
    var pos = array<vec2<f32>, 3>(
        vec2<f32>(-1.0, -1.0),
        vec2<f32>(3.0, -1.0),
        vec2<f32>(-1.0, 3.0)
    );
    var output: VertexOutput;
    output.position = vec4<f32>(pos[vertexIndex], 0.0, 1.0);
    output.uv = (pos[vertexIndex] + 1.0) * 0.5;
    return output;
}

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    let uv = input.uv;
    let time = uniforms.iTime;

    // Simple animated gradient with sine waves
    let r = 0.5 + 0.5 * sin(time + uv.x * 6.28);
    let g = 0.5 + 0.5 * sin(time * 1.3 + uv.y * 6.28);
    let b = 0.5 + 0.5 * sin(time * 0.7 + (uv.x + uv.y) * 3.14);

    return vec4<f32>(r, g, b, 1.0);
}
)";

class ShaderToy : public Widget {
public:
    ~ShaderToy() {
        cleanup();
    }

    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<ShaderToy>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("ShaderToy::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        // Get size from data bag or use defaults
        float width = 400.0f;
        float height = 300.0f;
        if (auto res = _data_bag->get("width"); res) {
            if (auto w = get_as<float>(*res)) width = *w;
            else if (auto w = get_as<int>(*res)) width = static_cast<float>(*w);
        }
        if (auto res = _data_bag->get("height"); res) {
            if (auto h = get_as<float>(*res)) height = *h;
            else if (auto h = get_as<int>(*res)) height = static_cast<float>(*h);
        }

        ImVec2 size(width, height);
        ImVec2 pos = ImGui::GetCursorScreenPos();

        // Store widget bounds for callback
        _widgetPos = pos;
        _widgetSize = size;

        // Reserve space in ImGui
        ImGui::Dummy(size);

        // Initialize WebGPU resources lazily (need device from render state)
        if (!_initialized) {
            // Will init on first callback when we have device access
        }

        // Add draw callback
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddCallback(renderCallback, this);
        draw_list->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

        return Ok();
    }

private:
    static void renderCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd) {
        ShaderToy* self = static_cast<ShaderToy*>(cmd->UserCallbackData);
        self->renderShader();
    }

    void renderShader() {
        // Get render state from ImGui
        ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
        if (!platform_io.Renderer_RenderState) return;

        ImGui_ImplWGPU_RenderState* rs =
            static_cast<ImGui_ImplWGPU_RenderState*>(platform_io.Renderer_RenderState);

        WGPUDevice device = rs->Device;
        WGPURenderPassEncoder encoder = rs->RenderPassEncoder;

        // Initialize on first render
        if (!_initialized) {
            if (!initWebGPU(device)) {
                return;
            }
            _initialized = true;
            _startTime = std::chrono::steady_clock::now();
        }

        // Update uniforms
        auto now = std::chrono::steady_clock::now();
        float elapsed = std::chrono::duration<float>(now - _startTime).count();

        ShaderToyUniforms uniforms;
        uniforms.iResolution[0] = _widgetSize.x;
        uniforms.iResolution[1] = _widgetSize.y;
        uniforms.iTime = elapsed;
        uniforms._pad = 0.0f;

        // Get queue from device (need to store it during init)
        wgpuQueueWriteBuffer(_queue, _uniformBuffer, 0, &uniforms, sizeof(uniforms));

        // Set viewport to widget bounds
        wgpuRenderPassEncoderSetViewport(encoder,
            _widgetPos.x, _widgetPos.y,
            _widgetSize.x, _widgetSize.y,
            0.0f, 1.0f);

        // Set scissor rect
        wgpuRenderPassEncoderSetScissorRect(encoder,
            static_cast<uint32_t>(_widgetPos.x),
            static_cast<uint32_t>(_widgetPos.y),
            static_cast<uint32_t>(_widgetSize.x),
            static_cast<uint32_t>(_widgetSize.y));

        // Draw with custom pipeline
        wgpuRenderPassEncoderSetPipeline(encoder, _pipeline);
        wgpuRenderPassEncoderSetBindGroup(encoder, 0, _bindGroup, 0, nullptr);
        wgpuRenderPassEncoderDraw(encoder, 3, 1, 0, 0);  // Fullscreen triangle
    }

    bool initWebGPU(WGPUDevice device) {
        _device = device;
        _queue = wgpuDeviceGetQueue(device);

        // Create shader module
        WGPUShaderModuleWGSLDescriptor wgslDesc = {};
        wgslDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
        wgslDesc.code = SHADER_SOURCE;

        WGPUShaderModuleDescriptor moduleDesc = {};
        moduleDesc.nextInChain = &wgslDesc.chain;
        moduleDesc.label = "shader-toy";

        _shaderModule = wgpuDeviceCreateShaderModule(device, &moduleDesc);
        if (!_shaderModule) return false;

        // Create uniform buffer
        WGPUBufferDescriptor bufDesc = {};
        bufDesc.label = "shader-toy uniforms";
        bufDesc.size = sizeof(ShaderToyUniforms);
        bufDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
        _uniformBuffer = wgpuDeviceCreateBuffer(device, &bufDesc);
        if (!_uniformBuffer) return false;

        // Create bind group layout
        WGPUBindGroupLayoutEntry layoutEntry = {};
        layoutEntry.binding = 0;
        layoutEntry.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
        layoutEntry.buffer.type = WGPUBufferBindingType_Uniform;
        layoutEntry.buffer.minBindingSize = sizeof(ShaderToyUniforms);

        WGPUBindGroupLayoutDescriptor layoutDesc = {};
        layoutDesc.entryCount = 1;
        layoutDesc.entries = &layoutEntry;
        _bindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &layoutDesc);
        if (!_bindGroupLayout) return false;

        // Create bind group
        WGPUBindGroupEntry bgEntry = {};
        bgEntry.binding = 0;
        bgEntry.buffer = _uniformBuffer;
        bgEntry.size = sizeof(ShaderToyUniforms);

        WGPUBindGroupDescriptor bgDesc = {};
        bgDesc.layout = _bindGroupLayout;
        bgDesc.entryCount = 1;
        bgDesc.entries = &bgEntry;
        _bindGroup = wgpuDeviceCreateBindGroup(device, &bgDesc);
        if (!_bindGroup) return false;

        // Create pipeline layout
        WGPUPipelineLayoutDescriptor plDesc = {};
        plDesc.bindGroupLayoutCount = 1;
        plDesc.bindGroupLayouts = &_bindGroupLayout;
        _pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &plDesc);
        if (!_pipelineLayout) return false;

        // Create render pipeline
        WGPUColorTargetState colorTarget = {};
        colorTarget.format = WGPUTextureFormat_BGRA8UnormSrgb;  // Match ImGui's surface format
        colorTarget.writeMask = WGPUColorWriteMask_All;

        // Blend state for proper compositing
        WGPUBlendState blendState = {};
        blendState.color.srcFactor = WGPUBlendFactor_One;
        blendState.color.dstFactor = WGPUBlendFactor_Zero;
        blendState.color.operation = WGPUBlendOperation_Add;
        blendState.alpha.srcFactor = WGPUBlendFactor_One;
        blendState.alpha.dstFactor = WGPUBlendFactor_Zero;
        blendState.alpha.operation = WGPUBlendOperation_Add;
        colorTarget.blend = &blendState;

        WGPUFragmentState fragState = {};
        fragState.module = _shaderModule;
        fragState.entryPoint = "fs_main";
        fragState.targetCount = 1;
        fragState.targets = &colorTarget;

        WGPURenderPipelineDescriptor pipelineDesc = {};
        pipelineDesc.label = "shader-toy pipeline";
        pipelineDesc.layout = _pipelineLayout;
        pipelineDesc.vertex.module = _shaderModule;
        pipelineDesc.vertex.entryPoint = "vs_main";
        pipelineDesc.fragment = &fragState;
        pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        pipelineDesc.primitive.cullMode = WGPUCullMode_None;
        pipelineDesc.multisample.count = 1;
        pipelineDesc.multisample.mask = 0xFFFFFFFF;

        _pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);
        if (!_pipeline) return false;

        return true;
    }

    void cleanup() {
        if (_pipeline) { wgpuRenderPipelineRelease(_pipeline); _pipeline = nullptr; }
        if (_pipelineLayout) { wgpuPipelineLayoutRelease(_pipelineLayout); _pipelineLayout = nullptr; }
        if (_bindGroup) { wgpuBindGroupRelease(_bindGroup); _bindGroup = nullptr; }
        if (_bindGroupLayout) { wgpuBindGroupLayoutRelease(_bindGroupLayout); _bindGroupLayout = nullptr; }
        if (_uniformBuffer) { wgpuBufferRelease(_uniformBuffer); _uniformBuffer = nullptr; }
        if (_shaderModule) { wgpuShaderModuleRelease(_shaderModule); _shaderModule = nullptr; }
    }

    // WebGPU resources
    WGPUDevice _device = nullptr;
    WGPUQueue _queue = nullptr;
    WGPUShaderModule _shaderModule = nullptr;
    WGPUBuffer _uniformBuffer = nullptr;
    WGPUBindGroupLayout _bindGroupLayout = nullptr;
    WGPUBindGroup _bindGroup = nullptr;
    WGPUPipelineLayout _pipelineLayout = nullptr;
    WGPURenderPipeline _pipeline = nullptr;

    bool _initialized = false;
    std::chrono::steady_clock::time_point _startTime;
    ImVec2 _widgetPos;
    ImVec2 _widgetSize;
};

} // namespace ymery::plugins

extern "C" const char* name() { return "shader-toy"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::ShaderToy::create(wf, d, ns, db);
}

#else // !YMERY_USE_WEBGPU

// Stub for non-WebGPU builds
namespace ymery::plugins {

class ShaderToyStub : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<ShaderToyStub>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;
        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("ShaderToyStub::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        ImGui::TextColored(ImVec4(1, 0.5, 0, 1), "shader-toy requires WebGPU backend");
        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "shader-toy"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::ShaderToyStub::create(wf, d, ns, db);
}

#endif // YMERY_USE_WEBGPU
