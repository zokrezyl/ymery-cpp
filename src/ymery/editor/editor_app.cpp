#include "editor_app.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#ifdef YMERY_USE_WEBGPU
#include <imgui_impl_wgpu.h>
#include <webgpu/webgpu.h>
#else
#include <imgui_impl_opengl3.h>
#endif
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#ifdef YMERY_USE_WEBGPU
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#endif

namespace ymery::editor {

#ifdef YMERY_USE_WEBGPU
// WebGPU state
static WGPUInstance s_wgpu_instance = nullptr;
static WGPUAdapter s_wgpu_adapter = nullptr;
static WGPUDevice s_wgpu_device = nullptr;
static WGPUSurface s_wgpu_surface = nullptr;
static WGPUQueue s_wgpu_queue = nullptr;
static int s_wgpu_surface_width = 0;
static int s_wgpu_surface_height = 0;
static WGPUTextureFormat s_wgpu_preferred_format = WGPUTextureFormat_BGRA8Unorm;

static void wgpu_error_callback(WGPUErrorType type, const char* message, void* userdata) {
    spdlog::error("WebGPU Error ({}): {}", static_cast<int>(type), message);
}

static void wgpu_device_lost_callback(WGPUDeviceLostReason reason, const char* message, void* userdata) {
    spdlog::error("WebGPU Device Lost ({}): {}", static_cast<int>(reason), message);
}

static void configure_wgpu_surface(int width, int height) {
    s_wgpu_surface_width = width;
    s_wgpu_surface_height = height;

    WGPUSurfaceConfiguration config = {};
    config.device = s_wgpu_device;
    config.format = s_wgpu_preferred_format;
    config.usage = WGPUTextureUsage_RenderAttachment;
    config.width = width;
    config.height = height;
    config.presentMode = WGPUPresentMode_Fifo;
    config.alphaMode = WGPUCompositeAlphaMode_Auto;

    wgpuSurfaceConfigure(s_wgpu_surface, &config);
}
#endif

static void glfw_error_callback(int error, const char* description) {
    spdlog::error("GLFW Error {}: {}", error, description);
}

std::unique_ptr<EditorApp> EditorApp::create(const EditorConfig& config) {
    auto app = std::unique_ptr<EditorApp>(new EditorApp());
    if (!app->init(config)) {
        return nullptr;
    }
    return app;
}

EditorApp::~EditorApp() {
    dispose();
}

bool EditorApp::init(const EditorConfig& config) {
    spdlog::info("EditorApp::init starting");

    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit()) {
        spdlog::error("glfwInit failed");
        return false;
    }

#ifdef YMERY_USE_WEBGPU
    // WebGPU initialization
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    _window = glfwCreateWindow(
        config.window_width,
        config.window_height,
        config.window_title.c_str(),
        nullptr,
        nullptr
    );

    if (!_window) {
        glfwTerminate();
        spdlog::error("glfwCreateWindow failed");
        return false;
    }

    // Create WebGPU instance
    WGPUInstanceDescriptor instance_desc = {};
    s_wgpu_instance = wgpuCreateInstance(&instance_desc);
    if (!s_wgpu_instance) {
        spdlog::error("wgpuCreateInstance failed");
        return false;
    }

    // Create surface from X11 window
    Display* x11_display = glfwGetX11Display();
    Window x11_window = glfwGetX11Window(_window);

    WGPUSurfaceDescriptorFromXlibWindow x11_surface_desc = {};
    x11_surface_desc.chain.sType = WGPUSType_SurfaceDescriptorFromXlibWindow;
    x11_surface_desc.display = x11_display;
    x11_surface_desc.window = x11_window;

    WGPUSurfaceDescriptor surface_desc = {};
    surface_desc.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&x11_surface_desc);

    s_wgpu_surface = wgpuInstanceCreateSurface(s_wgpu_instance, &surface_desc);
    if (!s_wgpu_surface) {
        spdlog::error("wgpuInstanceCreateSurface failed");
        return false;
    }

    // Request adapter
    WGPURequestAdapterOptions adapter_opts = {};
    adapter_opts.compatibleSurface = s_wgpu_surface;
    adapter_opts.powerPreference = WGPUPowerPreference_HighPerformance;

    struct AdapterUserData {
        WGPUAdapter adapter = nullptr;
        bool done = false;
    } adapter_data;

    wgpuInstanceRequestAdapter(s_wgpu_instance, &adapter_opts,
        [](WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message, void* userdata) {
            auto* data = static_cast<AdapterUserData*>(userdata);
            if (status == WGPURequestAdapterStatus_Success) {
                data->adapter = adapter;
            } else {
                spdlog::error("Failed to get WebGPU adapter: {}", message ? message : "unknown");
            }
            data->done = true;
        }, &adapter_data);

    while (!adapter_data.done) {}

    s_wgpu_adapter = adapter_data.adapter;
    if (!s_wgpu_adapter) {
        spdlog::error("Failed to get WebGPU adapter");
        return false;
    }

    // Request device
    WGPUDeviceDescriptor device_desc = {};
    device_desc.deviceLostCallback = wgpu_device_lost_callback;

    struct DeviceUserData {
        WGPUDevice device = nullptr;
        bool done = false;
    } device_data;

    wgpuAdapterRequestDevice(s_wgpu_adapter, &device_desc,
        [](WGPURequestDeviceStatus status, WGPUDevice device, const char* message, void* userdata) {
            auto* data = static_cast<DeviceUserData*>(userdata);
            if (status == WGPURequestDeviceStatus_Success) {
                data->device = device;
            } else {
                spdlog::error("Failed to get WebGPU device: {}", message ? message : "unknown");
            }
            data->done = true;
        }, &device_data);

    while (!device_data.done) {}

    s_wgpu_device = device_data.device;
    if (!s_wgpu_device) {
        spdlog::error("Failed to get WebGPU device");
        return false;
    }

    s_wgpu_queue = wgpuDeviceGetQueue(s_wgpu_device);
    s_wgpu_preferred_format = wgpuSurfaceGetPreferredFormat(s_wgpu_surface, s_wgpu_adapter);

    int width, height;
    glfwGetFramebufferSize(_window, &width, &height);
    configure_wgpu_surface(width, height);

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOther(_window, true);

    ImGui_ImplWGPU_InitInfo wgpu_init_info = {};
    wgpu_init_info.Device = s_wgpu_device;
    wgpu_init_info.NumFramesInFlight = 3;
    wgpu_init_info.RenderTargetFormat = s_wgpu_preferred_format;
    wgpu_init_info.DepthStencilFormat = WGPUTextureFormat_Undefined;
    ImGui_ImplWGPU_Init(&wgpu_init_info);

#else
    // OpenGL initialization
    const char* glsl_version = "#version 120";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    _window = glfwCreateWindow(
        config.window_width,
        config.window_height,
        config.window_title.c_str(),
        nullptr,
        nullptr
    );

    if (!_window) {
        glfwTerminate();
        spdlog::error("glfwCreateWindow failed");
        return false;
    }

    glfwMakeContextCurrent(_window);
    glfwSwapInterval(1);

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(_window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
#endif

    // Create editor components
    _widget_tree = std::make_unique<WidgetTree>();
    _canvas = std::make_unique<EditorCanvas>(_model, *_widget_tree);

    spdlog::info("EditorApp initialized successfully");
    return true;
}

void EditorApp::dispose() {
    _canvas.reset();
    _widget_tree.reset();

#ifdef YMERY_USE_WEBGPU
    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (s_wgpu_surface) wgpuSurfaceUnconfigure(s_wgpu_surface);
    if (s_wgpu_queue) wgpuQueueRelease(s_wgpu_queue);
    if (s_wgpu_device) wgpuDeviceRelease(s_wgpu_device);
    if (s_wgpu_adapter) wgpuAdapterRelease(s_wgpu_adapter);
    if (s_wgpu_surface) wgpuSurfaceRelease(s_wgpu_surface);
    if (s_wgpu_instance) wgpuInstanceRelease(s_wgpu_instance);
#else
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
#endif

    if (_window) {
        glfwDestroyWindow(_window);
        _window = nullptr;
    }
    glfwTerminate();
}

void EditorApp::run() {
    spdlog::info("EditorApp::run starting main loop");

    while (!_should_close) {
        frame();
    }

    spdlog::info("EditorApp::run exiting");
}

void EditorApp::frame() {
    begin_frame();

    // Menu bar
    render_menu_bar();

    // Dockspace
    render_dockspace();

    end_frame();
}

void EditorApp::begin_frame() {
    glfwPollEvents();

    if (glfwWindowShouldClose(_window)) {
        _should_close = true;
    }

#ifdef YMERY_USE_WEBGPU
    // Handle resize
    int width, height;
    glfwGetFramebufferSize(_window, &width, &height);
    if (width != s_wgpu_surface_width || height != s_wgpu_surface_height) {
        if (width > 0 && height > 0) {
            configure_wgpu_surface(width, height);
        }
    }

    ImGui_ImplWGPU_NewFrame();
#else
    ImGui_ImplOpenGL3_NewFrame();
#endif
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void EditorApp::end_frame() {
    ImGui::Render();

#ifdef YMERY_USE_WEBGPU
    WGPUSurfaceTexture surface_texture;
    wgpuSurfaceGetCurrentTexture(s_wgpu_surface, &surface_texture);
    if (surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
        spdlog::error("Failed to get surface texture");
        return;
    }

    WGPUTextureViewDescriptor view_desc = {};
    view_desc.format = s_wgpu_preferred_format;
    view_desc.dimension = WGPUTextureViewDimension_2D;
    view_desc.mipLevelCount = 1;
    view_desc.arrayLayerCount = 1;
    WGPUTextureView view = wgpuTextureCreateView(surface_texture.texture, &view_desc);

    WGPURenderPassColorAttachment color_attachment = {};
    color_attachment.view = view;
    color_attachment.loadOp = WGPULoadOp_Clear;
    color_attachment.storeOp = WGPUStoreOp_Store;
    color_attachment.clearValue = {0.1f, 0.1f, 0.1f, 1.0f};

    WGPURenderPassDescriptor render_pass_desc = {};
    render_pass_desc.colorAttachmentCount = 1;
    render_pass_desc.colorAttachments = &color_attachment;

    WGPUCommandEncoderDescriptor encoder_desc = {};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(s_wgpu_device, &encoder_desc);

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &render_pass_desc);
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    WGPUCommandBufferDescriptor cmd_desc = {};
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, &cmd_desc);
    wgpuQueueSubmit(s_wgpu_queue, 1, &cmd);

    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(encoder);
    wgpuTextureViewRelease(view);

    wgpuSurfacePresent(s_wgpu_surface);
    wgpuTextureRelease(surface_texture.texture);
#else
    int display_w, display_h;
    glfwGetFramebufferSize(_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(_window);
#endif
}

void EditorApp::render_menu_bar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New")) {
                _model.clear();
                _current_file.clear();
                _modified = false;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Export YAML...")) {
                std::string yaml = _model.to_yaml();
                spdlog::info("Generated YAML:\n{}", yaml);
                // TODO: Save dialog
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                _should_close = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Clear All")) {
                _model.clear();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Widget Browser", nullptr, true);
            ImGui::MenuItem("Layout Editor", nullptr, true);
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void EditorApp::render_dockspace() {
    // Create a fullscreen dockspace
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking |
                                     ImGuiWindowFlags_NoTitleBar |
                                     ImGuiWindowFlags_NoCollapse |
                                     ImGuiWindowFlags_NoResize |
                                     ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoBringToFrontOnFocus |
                                     ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("DockSpaceWindow", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    // Create the dockspace
    ImGuiID dockspace_id = ImGui::GetID("EditorDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    // First time setup: create default layout
    static bool first_time = true;
    if (first_time) {
        first_time = false;

        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_None);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

        // Split into left and right
        ImGuiID dock_left, dock_right;
        ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.25f, &dock_left, &dock_right);

        ImGui::DockBuilderDockWindow("Widget Browser", dock_left);
        ImGui::DockBuilderDockWindow("Layout Editor", dock_right);

        ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGui::End();

    // Render the two panels
    ImGui::Begin("Widget Browser");
    _widget_tree->render();
    ImGui::End();

    ImGui::Begin("Layout Editor");
    _canvas->render();
    ImGui::End();
}

} // namespace ymery::editor
