#include "editor_app.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#ifdef YMERY_ANDROID
#include <android/log.h>
#include <android/configuration.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <jni.h>
#include <imgui_impl_android.h>
#include <imgui_impl_opengl3.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "ymery-editor", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "ymery-editor", __VA_ARGS__)
#else
#include <imgui_impl_glfw.h>
#ifdef YMERY_USE_WEBGPU
#include <imgui_impl_wgpu.h>
#include <webgpu/webgpu.h>
#else
#include <imgui_impl_opengl3.h>
#endif
#include <GLFW/glfw3.h>
#endif

#include <spdlog/spdlog.h>

#if defined(YMERY_USE_WEBGPU) && !defined(YMERY_ANDROID)
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#endif

namespace ymery::editor {

#ifdef YMERY_ANDROID
// Get native library directory via JNI
static std::string get_native_library_dir(ANativeActivity* activity) {
    JNIEnv* env = nullptr;
    activity->vm->AttachCurrentThread(&env, nullptr);

    jclass activityClass = env->GetObjectClass(activity->clazz);
    jmethodID getApplicationInfo = env->GetMethodID(activityClass, "getApplicationInfo",
        "()Landroid/content/pm/ApplicationInfo;");
    jobject appInfo = env->CallObjectMethod(activity->clazz, getApplicationInfo);

    jclass appInfoClass = env->GetObjectClass(appInfo);
    jfieldID nativeLibDirField = env->GetFieldID(appInfoClass, "nativeLibraryDir", "Ljava/lang/String;");
    jstring nativeLibDir = (jstring)env->GetObjectField(appInfo, nativeLibDirField);

    const char* pathChars = env->GetStringUTFChars(nativeLibDir, nullptr);
    std::string result(pathChars);
    env->ReleaseStringUTFChars(nativeLibDir, pathChars);

    env->DeleteLocalRef(nativeLibDir);
    env->DeleteLocalRef(appInfo);
    env->DeleteLocalRef(activityClass);
    env->DeleteLocalRef(appInfoClass);

    activity->vm->DetachCurrentThread();
    return result;
}
#endif

#ifndef YMERY_ANDROID
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
#endif // !YMERY_ANDROID

std::unique_ptr<EditorApp> EditorApp::create(const EditorConfig& config) {
    auto app = std::unique_ptr<EditorApp>(new EditorApp());
    if (!app->init(config)) {
        return nullptr;
    }
    return app;
}

#ifdef YMERY_ANDROID
std::unique_ptr<EditorApp> EditorApp::create(struct android_app* android_app, const EditorConfig& config) {
    auto app = std::unique_ptr<EditorApp>(new EditorApp());
    app->_android_app = android_app;

    // Get native library directory for plugins
    EditorConfig cfg = config;
    cfg.plugins_path = get_native_library_dir(android_app->activity);
    LOGI("Native library dir: %s", cfg.plugins_path.c_str());

    if (!app->init(cfg)) {
        return nullptr;
    }
    return app;
}
#endif

EditorApp::~EditorApp() {
    dispose();
}

bool EditorApp::init(const EditorConfig& config) {
    spdlog::info("EditorApp::init starting");

#ifdef YMERY_ANDROID
    // Android/EGL initialization
    LOGI("EditorApp::init starting (Android/EGL)");

    if (!_android_app || !_android_app->window) {
        LOGE("No Android window available");
        return false;
    }

    const EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_NONE
    };

    EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        LOGE("eglGetDisplay failed");
        return false;
    }

    if (!eglInitialize(display, nullptr, nullptr)) {
        LOGE("eglInitialize failed");
        return false;
    }

    EGLConfig egl_config;
    EGLint numConfigs;
    if (!eglChooseConfig(display, attribs, &egl_config, 1, &numConfigs) || numConfigs == 0) {
        LOGE("eglChooseConfig failed");
        return false;
    }

    EGLint format;
    eglGetConfigAttrib(display, egl_config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(_android_app->window, 0, 0, format);

    EGLSurface surface = eglCreateWindowSurface(display, egl_config, _android_app->window, nullptr);
    if (surface == EGL_NO_SURFACE) {
        LOGE("eglCreateWindowSurface failed");
        return false;
    }

    EGLContext context = eglCreateContext(display, egl_config, EGL_NO_CONTEXT, contextAttribs);
    if (context == EGL_NO_CONTEXT) {
        LOGE("eglCreateContext failed");
        return false;
    }

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
        LOGE("eglMakeCurrent failed");
        return false;
    }

    eglQuerySurface(display, surface, EGL_WIDTH, &_display_width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &_display_height);

    _egl_display = display;
    _egl_context = context;
    _egl_surface = surface;

    // Get density
    _display_density = AConfiguration_getDensity(_android_app->config) / 160.0f;
    if (_display_density < 1.0f) _display_density = 1.0f;

    LOGI("EGL initialized: %dx%d, density=%.2f", _display_width, _display_height, _display_density);

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.FontGlobalScale = _display_density;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(_display_density);

    ImGui_ImplAndroid_Init(_android_app->window);
    ImGui_ImplOpenGL3_Init("#version 300 es");

    LOGI("ImGui initialized successfully");

#else
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
#endif // !YMERY_ANDROID

    // Create plugin manager
    if (!config.plugins_path.empty()) {
        auto pm_res = ymery::PluginManager::create(config.plugins_path);
        if (pm_res) {
            _plugin_manager = *pm_res;
            spdlog::info("Plugin manager created with path: {}", config.plugins_path);
        } else {
            spdlog::warn("Failed to create plugin manager: {}", ymery::error_msg(pm_res));
        }
    }

    // Create editor components
    _widget_tree = std::make_unique<WidgetTree>();
    _canvas = std::make_unique<EditorCanvas>(_model, *_widget_tree, _plugin_manager);

    spdlog::info("EditorApp initialized successfully");
    return true;
}

void EditorApp::dispose() {
    _canvas.reset();
    _widget_tree.reset();

#ifdef YMERY_ANDROID
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplAndroid_Shutdown();
    ImGui::DestroyContext();

    EGLDisplay display = static_cast<EGLDisplay>(_egl_display);
    if (display != EGL_NO_DISPLAY) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (_egl_context != EGL_NO_CONTEXT) {
            eglDestroyContext(display, static_cast<EGLContext>(_egl_context));
        }
        if (_egl_surface != EGL_NO_SURFACE) {
            eglDestroySurface(display, static_cast<EGLSurface>(_egl_surface));
        }
        eglTerminate(display);
    }
    _egl_display = EGL_NO_DISPLAY;
    _egl_context = EGL_NO_CONTEXT;
    _egl_surface = EGL_NO_SURFACE;
#elif defined(YMERY_USE_WEBGPU)
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

    if (_window) {
        glfwDestroyWindow(_window);
        _window = nullptr;
    }
    glfwTerminate();
#endif
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
#ifdef YMERY_ANDROID
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();
#else
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
#endif
}

void EditorApp::end_frame() {
    ImGui::Render();

#ifdef YMERY_ANDROID
    glViewport(0, 0, _display_width, _display_height);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    eglSwapBuffers(static_cast<EGLDisplay>(_egl_display), static_cast<EGLSurface>(_egl_surface));
#elif defined(YMERY_USE_WEBGPU)
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
            ImGui::MenuItem("Layout View", nullptr, true);
            ImGui::MenuItem("Preview", nullptr, true);
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
    spdlog::info("EDITOR: DockSpace dockspace_id={}", dockspace_id);
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    // First time setup: create default layout
    static bool first_time = true;
    spdlog::info("EDITOR: first_time={}", first_time);
    if (first_time) {
        first_time = false;

        spdlog::info("EDITOR: DockBuilderRemoveNode({})", dockspace_id);
        ImGui::DockBuilderRemoveNode(dockspace_id);
        spdlog::info("EDITOR: DockBuilderAddNode({})", dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_None);
        spdlog::info("EDITOR: DockBuilderSetNodeSize({}, {}x{})", dockspace_id, viewport->WorkSize.x, viewport->WorkSize.y);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

        // Split into left and right
        ImGuiID dock_left, dock_right;
        ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.25f, &dock_left, &dock_right);
        spdlog::info("EDITOR: SplitNode({}, Left, 0.25) -> dock_left={}, dock_right={}", dockspace_id, dock_left, dock_right);

        // Split right into top (Layout View) and bottom (Preview)
        ImGuiID dock_right_top, dock_right_bottom;
        ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Up, 0.5f, &dock_right_top, &dock_right_bottom);
        spdlog::info("EDITOR: SplitNode({}, Up, 0.5) -> dock_right_top={}, dock_right_bottom={}", dock_right, dock_right_top, dock_right_bottom);

        spdlog::info("EDITOR: DockBuilderDockWindow('Widget Browser', {})", dock_left);
        ImGui::DockBuilderDockWindow("Widget Browser", dock_left);
        spdlog::info("EDITOR: DockBuilderDockWindow('Layout View', {})", dock_right_top);
        ImGui::DockBuilderDockWindow("Layout View", dock_right_top);
        spdlog::info("EDITOR: DockBuilderDockWindow('Preview', {})", dock_right_bottom);
        ImGui::DockBuilderDockWindow("Preview", dock_right_bottom);

        spdlog::info("EDITOR: DockBuilderFinish({})", dockspace_id);
        ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGui::End();

    // Render the three panels
    ImGui::Begin("Widget Browser");
    _widget_tree->render();
    ImGui::End();

    ImGui::Begin("Layout View");
    _canvas->render();
    ImGui::End();

    ImGui::Begin("Preview");
    render_preview();
    ImGui::End();
}

void EditorApp::render_preview() {
    if (_model.empty()) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 text_size = ImGui::CalcTextSize("No widgets in layout");
        ImGui::SetCursorPos(ImVec2(
            (avail.x - text_size.x) / 2,
            (avail.y - text_size.y) / 2
        ));
        ImGui::TextDisabled("No widgets in layout");
        return;
    }

    render_preview_node(_model.root());
}

void EditorApp::render_preview_node(LayoutNode* node) {
    if (!node) return;

    ImGui::PushID(node->id);

    const std::string& type = node->widget_type;
    const std::string& label = node->label;

    // Handle same-line positioning
    if (node->position == LayoutPosition::SameLine && node->parent) {
        // Check if this is not the first child
        auto& siblings = node->parent->children;
        for (size_t i = 0; i < siblings.size(); ++i) {
            if (siblings[i].get() == node && i > 0) {
                ImGui::SameLine();
                break;
            }
        }
    }

    // Render widget based on type
    bool has_children = !node->children.empty();

    if (type == "window") {
        if (ImGui::Begin(label.c_str())) {
            for (auto& child : node->children) {
                render_preview_node(child.get());
            }
        }
        ImGui::End();
    }
    else if (type == "button") {
        ImGui::Button(label.c_str());
    }
    else if (type == "text") {
        ImGui::Text("%s", label.c_str());
    }
    else if (type == "separator") {
        ImGui::Separator();
    }
    else if (type == "spacing") {
        ImGui::Spacing();
    }
    else if (type == "checkbox") {
        static std::map<int, bool> checkbox_states;
        ImGui::Checkbox(label.c_str(), &checkbox_states[node->id]);
    }
    else if (type == "input-text") {
        static std::map<int, std::string> input_buffers;
        if (input_buffers.find(node->id) == input_buffers.end()) {
            input_buffers[node->id] = "";
        }
        char buf[256];
        strncpy(buf, input_buffers[node->id].c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        if (ImGui::InputText(label.c_str(), buf, sizeof(buf))) {
            input_buffers[node->id] = buf;
        }
    }
    else if (type == "input-int") {
        static std::map<int, int> int_values;
        ImGui::InputInt(label.c_str(), &int_values[node->id]);
    }
    else if (type == "slider-int") {
        static std::map<int, int> slider_values;
        ImGui::SliderInt(label.c_str(), &slider_values[node->id], 0, 100);
    }
    else if (type == "slider-float") {
        static std::map<int, float> slider_values;
        ImGui::SliderFloat(label.c_str(), &slider_values[node->id], 0.0f, 1.0f);
    }
    else if (type == "combo") {
        static std::map<int, int> combo_values;
        const char* items[] = { "Option 1", "Option 2", "Option 3" };
        ImGui::Combo(label.c_str(), &combo_values[node->id], items, 3);
    }
    else if (type == "color-edit") {
        static std::map<int, ImVec4> colors;
        if (colors.find(node->id) == colors.end()) {
            colors[node->id] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        }
        ImGui::ColorEdit4(label.c_str(), &colors[node->id].x);
    }
    else if (type == "row" || type == "group") {
        ImGui::BeginGroup();
        bool first = true;
        for (auto& child : node->children) {
            if (!first) ImGui::SameLine();
            render_preview_node(child.get());
            first = false;
        }
        ImGui::EndGroup();
    }
    else if (type == "column") {
        ImGui::BeginGroup();
        for (auto& child : node->children) {
            render_preview_node(child.get());
        }
        ImGui::EndGroup();
    }
    else if (type == "child") {
        ImVec2 size(0, 100);  // Default child size
        if (ImGui::BeginChild(label.c_str(), size, true)) {
            for (auto& child : node->children) {
                render_preview_node(child.get());
            }
        }
        ImGui::EndChild();
    }
    else if (type == "tree-node") {
        if (ImGui::TreeNode(label.c_str())) {
            for (auto& child : node->children) {
                render_preview_node(child.get());
            }
            ImGui::TreePop();
        }
    }
    else if (type == "collapsing-header") {
        if (ImGui::CollapsingHeader(label.c_str())) {
            for (auto& child : node->children) {
                render_preview_node(child.get());
            }
        }
    }
    else if (type == "tab-bar") {
        if (ImGui::BeginTabBar(label.c_str())) {
            for (auto& child : node->children) {
                render_preview_node(child.get());
            }
            ImGui::EndTabBar();
        }
    }
    else if (type == "tab-item") {
        if (ImGui::BeginTabItem(label.c_str())) {
            for (auto& child : node->children) {
                render_preview_node(child.get());
            }
            ImGui::EndTabItem();
        }
    }
    else if (type == "tooltip") {
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", label.c_str());
            ImGui::EndTooltip();
        }
    }
    else if (type == "popup" || type == "popup-modal") {
        // Popups need to be triggered - show as button
        if (ImGui::Button(("Open " + label).c_str())) {
            ImGui::OpenPopup(label.c_str());
        }
        bool open = true;
        if (type == "popup-modal") {
            if (ImGui::BeginPopupModal(label.c_str(), &open)) {
                for (auto& child : node->children) {
                    render_preview_node(child.get());
                }
                if (ImGui::Button("Close")) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        } else {
            if (ImGui::BeginPopup(label.c_str())) {
                for (auto& child : node->children) {
                    render_preview_node(child.get());
                }
                ImGui::EndPopup();
            }
        }
    }
    else if (type == "toggle") {
        static std::map<int, bool> toggle_states;
        // Simple checkbox as toggle representation
        ImGui::Checkbox(label.c_str(), &toggle_states[node->id]);
    }
    else if (type == "knob") {
        static std::map<int, float> knob_values;
        // Show as slider since ImGui doesn't have built-in knob
        ImGui::SliderFloat(label.c_str(), &knob_values[node->id], 0.0f, 1.0f);
    }
    else if (type == "markdown") {
        // Show markdown as plain text
        ImGui::TextWrapped("%s", label.c_str());
    }
    else {
        // Unknown widget type - show placeholder
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "[%s: %s]", type.c_str(), label.c_str());
        // Still render children for containers
        if (has_children) {
            ImGui::Indent();
            for (auto& child : node->children) {
                render_preview_node(child.get());
            }
            ImGui::Unindent();
        }
    }

    ImGui::PopID();
}

} // namespace ymery::editor
