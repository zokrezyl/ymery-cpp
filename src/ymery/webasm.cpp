// ymery WebAssembly/Emscripten mode with Dawn WebGPU
// This file is used instead of app.cpp for YMERY_WEB builds

#ifdef YMERY_WEB

#include "webasm.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_wgpu.h>
#include <implot.h>
#include <GLFW/glfw3.h>

#include <emscripten.h>
#include <emscripten/html5.h>
#include <webgpu/webgpu.h>

#include <spdlog/spdlog.h>

namespace ymery {

// Static instance for emscripten main loop callback
static WebApp* g_web_app = nullptr;

// GLFW error callback
static void glfw_error_callback(int error, const char* description) {
    spdlog::error("GLFW Error {}: {}", error, description);
}

// WebGPU error callback
static void wgpu_error_callback(WGPUErrorType type, const char* message, void* userdata) {
    spdlog::error("WebGPU Error ({}): {}", static_cast<int>(type), message ? message : "unknown");
}

static void wgpu_device_lost_callback(WGPUDeviceLostReason reason, const char* message, void* userdata) {
    spdlog::error("WebGPU Device Lost ({}): {}", static_cast<int>(reason), message ? message : "unknown");
}

// Emscripten main loop callback
static void em_main_loop_callback() {
    if (g_web_app && !g_web_app->should_close()) {
        g_web_app->frame();
    }
}

Result<std::shared_ptr<WebApp>> WebApp::create(const WebAppConfig& config) {
    auto app = std::shared_ptr<WebApp>(new WebApp());
    app->_config = config;

    if (auto res = app->_init(); !res) {
        return Err<std::shared_ptr<WebApp>>("WebApp::create: init failed", res);
    }

    return app;
}

Result<void> WebApp::_init() {
    spdlog::info("WebApp::_init starting");

    // Initialize graphics backend
    if (auto gfx_res = _init_graphics(); !gfx_res) {
        spdlog::error("WebApp::_init: graphics init failed");
        return Err<void>("WebApp::_init: graphics init failed", gfx_res);
    }
    spdlog::info("Graphics initialized");

    // Create dispatcher
    auto disp_res = Dispatcher::create();
    if (!disp_res) {
        return Err<void>("WebApp::_init: dispatcher create failed", disp_res);
    }
    _dispatcher = *disp_res;

    // Build colon-separated plugin path string
    std::string plugins_path;
    for (const auto& p : _config.plugin_paths) {
        if (!plugins_path.empty()) plugins_path += ":";
        plugins_path += p.string();
    }

    // Create plugin manager
    auto pm_res = PluginManager::create(plugins_path);
    if (!pm_res) {
        return Err<void>("WebApp::_init: plugin manager create failed", pm_res);
    }
    _plugin_manager = *pm_res;

    // Load YAML modules
    auto lang_res = Lang::create(_config.layout_paths, _config.main_module);
    if (!lang_res) {
        return Err<void>("WebApp::_init: lang create failed", lang_res);
    }
    _lang = *lang_res;

    // Get data tree type from app config
    std::string tree_type = "simple-data-tree";
    const auto& app_config = _lang->app_config();
    auto tree_it = app_config.find("data-tree");
    if (tree_it != app_config.end()) {
        if (auto t = get_as<std::string>(tree_it->second)) {
            tree_type = *t;
            spdlog::info("Using data-tree type from config: {}", tree_type);
        }
    }

    // Create data tree from plugin
    auto tree_res = _plugin_manager->create_tree(tree_type, _dispatcher);
    if (!tree_res) {
        spdlog::warn("Could not create {} from plugin: {}", tree_type, error_msg(tree_res));
        return Err<void>("WebApp::_init: no tree-like plugin available", tree_res);
    }
    _data_tree = *tree_res;

    // Create widget factory
    auto wf_res = WidgetFactory::create(_lang, _dispatcher, _data_tree, _plugin_manager);
    if (!wf_res) {
        return Err<void>("WebApp::_init: widget factory create failed", wf_res);
    }
    _widget_factory = *wf_res;

    // Create root widget
    spdlog::info("Creating root widget");
    auto root_res = _widget_factory->create_root_widget();
    if (!root_res) {
        spdlog::error("WebApp::_init: root widget create failed: {}", error_msg(root_res));
        return Err<void>("WebApp::_init: root widget create failed", root_res);
    }
    _root_widget = *root_res;
    spdlog::info("Root widget created successfully");

    return Ok();
}

Result<void> WebApp::dispose() {
    if (_root_widget) {
        _root_widget->dispose();
        _root_widget.reset();
    }

    if (_plugin_manager) {
        _plugin_manager->dispose();
        _plugin_manager.reset();
    }

    _shutdown_graphics();

    return Ok();
}

Result<void> WebApp::run() {
    spdlog::info("WebApp::run starting main loop");

    g_web_app = this;
    emscripten_set_main_loop(em_main_loop_callback, 0, true);

    return Ok();
}

Result<void> WebApp::frame() {
    if (auto begin_res = _begin_frame(); !begin_res) {
        return Err<void>("WebApp::frame: _begin_frame failed", begin_res);
    }

    // Render root widget
    if (_root_widget) {
        if (auto render_res = _root_widget->render(); !render_res) {
            // Log but continue
        }
    }

    if (auto end_res = _end_frame(); !end_res) {
        return Err<void>("WebApp::frame: _end_frame failed", end_res);
    }

    return Ok();
}

void WebApp::_configure_surface(int width, int height) {
    if (width <= 0 || height <= 0) return;

    _surface_width = width;
    _surface_height = height;

    WGPUSurfaceConfiguration config = {};
    config.device = _wgpu_device;
    config.format = _wgpu_format;
    config.usage = WGPUTextureUsage_RenderAttachment;
    config.width = static_cast<uint32_t>(width);
    config.height = static_cast<uint32_t>(height);
    config.presentMode = WGPUPresentMode_Fifo;
    config.alphaMode = WGPUCompositeAlphaMode_Opaque;

    wgpuSurfaceConfigure(_wgpu_surface, &config);
}

Result<void> WebApp::_init_graphics() {
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit()) {
        return Err<void>("WebApp::_init_graphics: glfwInit failed");
    }

    // No OpenGL context for WebGPU
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // Create window - for Emscripten this creates a canvas
    GLFWwindow* window = glfwCreateWindow(
        _config.window_width,
        _config.window_height,
        _config.window_title.c_str(),
        nullptr,
        nullptr
    );

    if (!window) {
        glfwTerminate();
        return Err<void>("WebApp::_init_graphics: glfwCreateWindow failed");
    }

    _glfw_window = window;

    // Create WebGPU instance
    _wgpu_instance = wgpuCreateInstance(nullptr);
    if (!_wgpu_instance) {
        return Err<void>("WebApp::_init_graphics: wgpuCreateInstance failed");
    }

    // Create surface from canvas using Dawn/Emscripten API
    WGPUSurfaceDescriptorFromCanvasHTMLSelector canvas_desc = {};
    canvas_desc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
    canvas_desc.selector = _config.canvas_selector;

    WGPUSurfaceDescriptor surface_desc = {};
    surface_desc.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&canvas_desc);

    _wgpu_surface = wgpuInstanceCreateSurface(_wgpu_instance, &surface_desc);
    if (!_wgpu_surface) {
        return Err<void>("WebApp::_init_graphics: wgpuInstanceCreateSurface failed");
    }

    // Request adapter
    WGPURequestAdapterOptions adapter_opts = {};
    adapter_opts.compatibleSurface = _wgpu_surface;
    adapter_opts.powerPreference = WGPUPowerPreference_HighPerformance;

    struct AdapterUserData {
        WGPUAdapter adapter = nullptr;
        bool done = false;
    } adapter_data;

    wgpuInstanceRequestAdapter(_wgpu_instance, &adapter_opts,
        [](WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message, void* userdata) {
            auto* data = static_cast<AdapterUserData*>(userdata);
            if (status == WGPURequestAdapterStatus_Success) {
                data->adapter = adapter;
            } else {
                spdlog::error("Failed to get WebGPU adapter: {}", message ? message : "unknown");
            }
            data->done = true;
        }, &adapter_data);

    // For Emscripten, we need to wait for async operation
    while (!adapter_data.done) {
        emscripten_sleep(10);
    }

    _wgpu_adapter = adapter_data.adapter;
    if (!_wgpu_adapter) {
        return Err<void>("WebApp::_init_graphics: failed to get WebGPU adapter");
    }

    // Request device
    WGPUDeviceDescriptor device_desc = {};
    device_desc.deviceLostCallback = wgpu_device_lost_callback;

    struct DeviceUserData {
        WGPUDevice device = nullptr;
        bool done = false;
    } device_data;

    wgpuAdapterRequestDevice(_wgpu_adapter, &device_desc,
        [](WGPURequestDeviceStatus status, WGPUDevice device, const char* message, void* userdata) {
            auto* data = static_cast<DeviceUserData*>(userdata);
            if (status == WGPURequestDeviceStatus_Success) {
                data->device = device;
            } else {
                spdlog::error("Failed to get WebGPU device: {}", message ? message : "unknown");
            }
            data->done = true;
        }, &device_data);

    while (!device_data.done) {
        emscripten_sleep(10);
    }

    _wgpu_device = device_data.device;
    if (!_wgpu_device) {
        return Err<void>("WebApp::_init_graphics: failed to get WebGPU device");
    }

    // Set error callback on device
    wgpuDeviceSetUncapturedErrorCallback(_wgpu_device, wgpu_error_callback, nullptr);

    _wgpu_queue = wgpuDeviceGetQueue(_wgpu_device);

    // Get preferred surface format
    WGPUSurfaceCapabilities caps = {};
    wgpuSurfaceGetCapabilities(_wgpu_surface, _wgpu_adapter, &caps);
    if (caps.formatCount > 0) {
        _wgpu_format = caps.formats[0];
    }
    wgpuSurfaceCapabilitiesFreeMembers(caps);

    // Configure surface with initial size
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    _configure_surface(width, height);

    // Setup Dear ImGui
    if (auto res = _init_imgui(); !res) {
        return Err<void>("WebApp::_init_graphics: ImGui init failed", res);
    }

    return Ok();
}

Result<void> WebApp::_shutdown_graphics() {
    _shutdown_imgui();

    if (_wgpu_surface) wgpuSurfaceUnconfigure(_wgpu_surface);
    if (_wgpu_queue) wgpuQueueRelease(_wgpu_queue);
    if (_wgpu_device) wgpuDeviceRelease(_wgpu_device);
    if (_wgpu_adapter) wgpuAdapterRelease(_wgpu_adapter);
    if (_wgpu_surface) wgpuSurfaceRelease(_wgpu_surface);
    if (_wgpu_instance) wgpuInstanceRelease(_wgpu_instance);

    _wgpu_queue = nullptr;
    _wgpu_device = nullptr;
    _wgpu_adapter = nullptr;
    _wgpu_surface = nullptr;
    _wgpu_instance = nullptr;

    if (_glfw_window) {
        glfwDestroyWindow(static_cast<GLFWwindow*>(_glfw_window));
        _glfw_window = nullptr;
    }
    glfwTerminate();

    return Ok();
}

Result<void> WebApp::_init_imgui() {
    IMGUI_CHECKVERSION();
    _imgui_ctx = ImGui::CreateContext();
    _implot_ctx = ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOther(static_cast<GLFWwindow*>(_glfw_window), true);

    ImGui_ImplWGPU_InitInfo wgpu_init_info = {};
    wgpu_init_info.Device = _wgpu_device;
    wgpu_init_info.NumFramesInFlight = 3;
    wgpu_init_info.RenderTargetFormat = _wgpu_format;
    wgpu_init_info.DepthStencilFormat = WGPUTextureFormat_Undefined;

    if (!ImGui_ImplWGPU_Init(&wgpu_init_info)) {
        return Err<void>("ImGui_ImplWGPU_Init failed");
    }

    return Ok();
}

void WebApp::_shutdown_imgui() {
    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    if (_implot_ctx) {
        ImPlot::DestroyContext(_implot_ctx);
        _implot_ctx = nullptr;
    }

    if (_imgui_ctx) {
        ImGui::DestroyContext(_imgui_ctx);
        _imgui_ctx = nullptr;
    }
}

Result<void> WebApp::_begin_frame() {
    glfwPollEvents();

    // Handle resize
    int width, height;
    glfwGetFramebufferSize(static_cast<GLFWwindow*>(_glfw_window), &width, &height);
    if (width != _surface_width || height != _surface_height) {
        _configure_surface(width, height);
    }

    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    return Ok();
}

Result<void> WebApp::_end_frame() {
    ImGui::Render();

    // Get current surface texture
    WGPUSurfaceTexture surface_texture;
    wgpuSurfaceGetCurrentTexture(_wgpu_surface, &surface_texture);

    if (surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
        // Handle surface not ready (e.g., minimized window)
        if (surface_texture.texture) {
            wgpuTextureRelease(surface_texture.texture);
        }
        return Ok();
    }

    // Create texture view
    WGPUTextureViewDescriptor view_desc = {};
    view_desc.format = _wgpu_format;
    view_desc.dimension = WGPUTextureViewDimension_2D;
    view_desc.mipLevelCount = 1;
    view_desc.arrayLayerCount = 1;
    WGPUTextureView view = wgpuTextureCreateView(surface_texture.texture, &view_desc);

    if (!view) {
        wgpuTextureRelease(surface_texture.texture);
        return Err<void>("Failed to create texture view");
    }

    // Create render pass
    WGPURenderPassColorAttachment color_attachment = {};
    color_attachment.view = view;
    color_attachment.loadOp = WGPULoadOp_Clear;
    color_attachment.storeOp = WGPUStoreOp_Store;
    color_attachment.clearValue = {0.1f, 0.1f, 0.1f, 1.0f};
#ifdef IMGUI_IMPL_WEBGPU_BACKEND_DAWN
    color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif

    WGPURenderPassDescriptor render_pass_desc = {};
    render_pass_desc.colorAttachmentCount = 1;
    render_pass_desc.colorAttachments = &color_attachment;

    WGPUCommandEncoderDescriptor encoder_desc = {};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(_wgpu_device, &encoder_desc);

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &render_pass_desc);
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    WGPUCommandBufferDescriptor cmd_desc = {};
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, &cmd_desc);
    wgpuQueueSubmit(_wgpu_queue, 1, &cmd);

    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(encoder);
    wgpuTextureViewRelease(view);

    wgpuSurfacePresent(_wgpu_surface);

    wgpuTextureRelease(surface_texture.texture);

    return Ok();
}

} // namespace ymery

#endif // YMERY_WEB
