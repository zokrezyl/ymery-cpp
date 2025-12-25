#include "app.hpp"

#ifdef YMERY_ANDROID
#include <android/log.h>
#include <android/configuration.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <jni.h>
#include <imgui.h>
#include <imgui_impl_android.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "ymery", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "ymery", __VA_ARGS__)
#else
#include <imgui.h>
#include <imgui_impl_glfw.h>
#ifdef YMERY_USE_WEBGPU
#include <imgui_impl_wgpu.h>
#include <webgpu/webgpu.h>
#else
#include <imgui_impl_opengl3.h>
#endif
#include <implot.h>
#include <GLFW/glfw3.h>
#endif

#include <spdlog/spdlog.h>

#ifdef YMERY_WEB
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>
#elif defined(YMERY_USE_WEBGPU) && !defined(YMERY_ANDROID)
// For X11 surface creation with wgpu-native
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#endif

namespace ymery {

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

#ifdef YMERY_USE_WEBGPU
// WebGPU state
static WGPUInstance _wgpu_instance = nullptr;
static WGPUAdapter _wgpu_adapter = nullptr;
static WGPUDevice _wgpu_device = nullptr;
static WGPUSurface _wgpu_surface = nullptr;
static WGPUQueue _wgpu_queue = nullptr;
static int _wgpu_surface_width = 0;
static int _wgpu_surface_height = 0;
static WGPUTextureFormat _wgpu_preferred_format = WGPUTextureFormat_BGRA8Unorm;
#endif

Result<std::shared_ptr<App>> App::create(const AppConfig& config) {
    auto app = std::shared_ptr<App>(new App());
    app->_config = config;

    if (auto res = app->init(); !res) {
        return Err<std::shared_ptr<App>>("App::create: init failed", res);
    }

    return app;
}

#ifdef YMERY_ANDROID
Result<std::shared_ptr<App>> App::create(struct android_app* android_app, const AppConfig& config) {
    auto app = std::shared_ptr<App>(new App());
    app->_config = config;
    app->_android_app = android_app;

    // Get native library directory and add to plugin paths
    std::string native_lib_dir = get_native_library_dir(android_app->activity);
    LOGI("Native library dir: %s", native_lib_dir.c_str());
    app->_config.plugin_paths.push_back(native_lib_dir);

    if (auto res = app->init(); !res) {
        return Err<std::shared_ptr<App>>("App::create: init failed", res);
    }

    return app;
}
#endif

Result<void> App::init() {
    spdlog::info("App::init starting");

    // Initialize graphics backend
    if (auto gfx_res = _init_graphics(); !gfx_res) {
        spdlog::error("App::init: graphics init failed");
        return Err<void>("App::init: graphics init failed", gfx_res);
    }
    spdlog::info("Graphics initialized");

    // Create dispatcher
    auto disp_res = Dispatcher::create();
    if (!disp_res) {
        return Err<void>("App::init: dispatcher create failed", disp_res);
    }
    _dispatcher = *disp_res;

    // Build colon-separated plugin path string
    std::string plugins_path;
    for (const auto& p : _config.plugin_paths) {
        if (!plugins_path.empty()) plugins_path += ":";
        plugins_path += p.string();
    }

    // Create plugin manager (TreeLike that holds all plugins)
    auto pm_res = PluginManager::create(plugins_path);
    if (!pm_res) {
        return Err<void>("App::init: plugin manager create failed", pm_res);
    }
    _plugin_manager = *pm_res;

    // Load YAML modules
    auto lang_res = Lang::create(_config.layout_paths, _config.main_module);
    if (!lang_res) {
        return Err<void>("App::init: lang create failed", lang_res);
    }
    _lang = *lang_res;

    // Get data tree type from app config (default: simple-data-tree)
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
        // Fallback to a minimal implementation if needed
        return Err<void>("App::init: no tree-like plugin available", tree_res);
    }
    _data_tree = *tree_res;

    // Create widget factory with plugin manager
    auto wf_res = WidgetFactory::create(_lang, _dispatcher, _data_tree, _plugin_manager);
    if (!wf_res) {
        return Err<void>("App::init: widget factory create failed", wf_res);
    }
    _widget_factory = *wf_res;

    // Create root widget
    spdlog::info("Creating root widget");
    auto root_res = _widget_factory->create_root_widget();
    if (!root_res) {
        spdlog::error("App::init: root widget create failed: {}", error_msg(root_res));
        return Err<void>("App::init: root widget create failed", root_res);
    }
    _root_widget = *root_res;
    spdlog::info("Root widget created successfully");

    return Ok();
}

Result<void> App::dispose() {
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

#ifdef YMERY_WEB
// Emscripten main loop callback
static App* _em_app = nullptr;
static void em_main_loop() {
    if (_em_app) {
        _em_app->frame();
    }
}
#endif

Result<void> App::run() {
    spdlog::info("App::run starting main loop");

#ifdef YMERY_WEB
    // Emscripten: use browser's requestAnimationFrame
    _em_app = this;
    emscripten_set_main_loop(em_main_loop, 0, true);
#else
    // Native: traditional while loop
    int frame_count = 0;
    while (!_should_close) {
        if (auto res = frame(); !res) {
            spdlog::warn("Frame error: {}", error_msg(res));
        }
        if (frame_count < 3) {
            spdlog::debug("Frame {} completed", frame_count);
        }
        frame_count++;
    }
    spdlog::info("App::run exiting after {} frames", frame_count);
#endif

    return Ok();
}

Result<void> App::frame() {
    if (auto begin_res = _begin_frame(); !begin_res) {
        return Err<void>("App::frame: _begin_frame failed", begin_res);
    }

    // Render root widget
    if (_root_widget) {
        if (auto render_res = _root_widget->render(); !render_res) {
            // Log but continue
        }
    }

    if (auto end_res = _end_frame(); !end_res) {
        return Err<void>("App::frame: _end_frame failed", end_res);
    }

    return Ok();
}

#ifndef YMERY_ANDROID
static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}
#endif

#ifdef YMERY_ANDROID
// Android/EGL backend

Result<void> App::_init_graphics() {
    LOGI("App::_init_graphics starting (Android/EGL)");

    if (!_android_app || !_android_app->window) {
        return Err<void>("App::_init_graphics: no Android window");
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
        return Err<void>("App::_init_graphics: eglGetDisplay failed");
    }

    if (!eglInitialize(display, nullptr, nullptr)) {
        return Err<void>("App::_init_graphics: eglInitialize failed");
    }

    EGLConfig config;
    EGLint numConfigs;
    if (!eglChooseConfig(display, attribs, &config, 1, &numConfigs) || numConfigs == 0) {
        return Err<void>("App::_init_graphics: eglChooseConfig failed");
    }

    EGLint format;
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(_android_app->window, 0, 0, format);

    EGLSurface surface = eglCreateWindowSurface(display, config, _android_app->window, nullptr);
    if (surface == EGL_NO_SURFACE) {
        return Err<void>("App::_init_graphics: eglCreateWindowSurface failed");
    }

    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
    if (context == EGL_NO_CONTEXT) {
        return Err<void>("App::_init_graphics: eglCreateContext failed");
    }

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
        return Err<void>("App::_init_graphics: eglMakeCurrent failed");
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

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.FontGlobalScale = _display_density;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(_display_density);

    ImGui_ImplAndroid_Init(_android_app->window);
    ImGui_ImplOpenGL3_Init("#version 300 es");

    LOGI("ImGui initialized successfully");
    return Ok();
}

Result<void> App::_shutdown_graphics() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplAndroid_Shutdown();
    ImPlot::DestroyContext();
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

    return Ok();
}

Result<void> App::_begin_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();

    return Ok();
}

Result<void> App::_end_frame() {
    ImGui::Render();

    glViewport(0, 0, _display_width, _display_height);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    eglSwapBuffers(static_cast<EGLDisplay>(_egl_display), static_cast<EGLSurface>(_egl_surface));

    return Ok();
}

#elif defined(YMERY_USE_WEBGPU)

// WebGPU callbacks
static void wgpu_error_callback(WGPUErrorType type, const char* message, void* userdata) {
    spdlog::error("WebGPU Error ({}): {}", static_cast<int>(type), message);
}

static void wgpu_device_lost_callback(WGPUDeviceLostReason reason, const char* message, void* userdata) {
    spdlog::error("WebGPU Device Lost ({}): {}", static_cast<int>(reason), message);
}

static void configure_wgpu_surface(int width, int height) {
    _wgpu_surface_width = width;
    _wgpu_surface_height = height;

    WGPUSurfaceConfiguration config = {};
    config.device = _wgpu_device;
    config.format = _wgpu_preferred_format;
    config.usage = WGPUTextureUsage_RenderAttachment;
    config.width = width;
    config.height = height;
    config.presentMode = WGPUPresentMode_Fifo;
    config.alphaMode = WGPUCompositeAlphaMode_Auto;

    wgpuSurfaceConfigure(_wgpu_surface, &config);
}

Result<void> App::_init_graphics() {
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit()) {
        return Err<void>("App::_init_graphics: glfwInit failed");
    }

    // No OpenGL context for WebGPU
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    _window = glfwCreateWindow(
        _config.window_width,
        _config.window_height,
        _config.window_title.c_str(),
        nullptr,
        nullptr
    );

    if (!_window) {
        glfwTerminate();
        return Err<void>("App::_init_graphics: glfwCreateWindow failed");
    }

    // Create WebGPU instance
#ifdef YMERY_WEB
    _wgpu_instance = wgpuCreateInstance(nullptr);
#else
    WGPUInstanceDescriptor instance_desc = {};
    _wgpu_instance = wgpuCreateInstance(&instance_desc);
#endif
    if (!_wgpu_instance) {
        return Err<void>("App::_init_graphics: wgpuCreateInstance failed");
    }

    // Create surface
#ifdef YMERY_WEB
    // Emscripten: create surface from canvas
    WGPUSurfaceDescriptorFromCanvasHTMLSelector canvas_desc = {};
    canvas_desc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
    canvas_desc.selector = "#canvas";

    WGPUSurfaceDescriptor surface_desc = {};
    surface_desc.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&canvas_desc);

    _wgpu_surface = wgpuInstanceCreateSurface(_wgpu_instance, &surface_desc);
#else
    // Native: create surface from X11 window
    Display* x11_display = glfwGetX11Display();
    Window x11_window = glfwGetX11Window(static_cast<GLFWwindow*>(_window));

    WGPUSurfaceDescriptorFromXlibWindow x11_surface_desc = {};
    x11_surface_desc.chain.sType = WGPUSType_SurfaceDescriptorFromXlibWindow;
    x11_surface_desc.display = x11_display;
    x11_surface_desc.window = x11_window;

    WGPUSurfaceDescriptor surface_desc = {};
    surface_desc.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&x11_surface_desc);

    _wgpu_surface = wgpuInstanceCreateSurface(_wgpu_instance, &surface_desc);
#endif
    if (!_wgpu_surface) {
        return Err<void>("App::_init_graphics: wgpuInstanceCreateSurface failed");
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

    while (!adapter_data.done) {
        // Busy wait for adapter (could use proper async)
    }

    _wgpu_adapter = adapter_data.adapter;
    if (!_wgpu_adapter) {
        return Err<void>("App::_init_graphics: failed to get WebGPU adapter");
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
        // Busy wait for device
    }

    _wgpu_device = device_data.device;
    if (!_wgpu_device) {
        return Err<void>("App::_init_graphics: failed to get WebGPU device");
    }

    _wgpu_queue = wgpuDeviceGetQueue(_wgpu_device);

    // Get preferred surface format
    _wgpu_preferred_format = wgpuSurfaceGetPreferredFormat(_wgpu_surface, _wgpu_adapter);

    // Configure surface
    int width, height;
    glfwGetFramebufferSize(static_cast<GLFWwindow*>(_window), &width, &height);
    configure_wgpu_surface(width, height);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Setup style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOther(static_cast<GLFWwindow*>(_window), true);

    ImGui_ImplWGPU_InitInfo wgpu_init_info = {};
    wgpu_init_info.Device = _wgpu_device;
    wgpu_init_info.NumFramesInFlight = 3;
    wgpu_init_info.RenderTargetFormat = _wgpu_preferred_format;
    wgpu_init_info.DepthStencilFormat = WGPUTextureFormat_Undefined;
    ImGui_ImplWGPU_Init(&wgpu_init_info);

    return Ok();
}

Result<void> App::_shutdown_graphics() {
    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    if (_wgpu_surface) wgpuSurfaceUnconfigure(_wgpu_surface);
    if (_wgpu_queue) wgpuQueueRelease(_wgpu_queue);
    if (_wgpu_device) wgpuDeviceRelease(_wgpu_device);
    if (_wgpu_adapter) wgpuAdapterRelease(_wgpu_adapter);
    if (_wgpu_surface) wgpuSurfaceRelease(_wgpu_surface);
    if (_wgpu_instance) wgpuInstanceRelease(_wgpu_instance);

    if (_window) {
        glfwDestroyWindow(static_cast<GLFWwindow*>(_window));
    }
    glfwTerminate();

    return Ok();
}

Result<void> App::_begin_frame() {
    glfwPollEvents();

    if (glfwWindowShouldClose(static_cast<GLFWwindow*>(_window))) {
        _should_close = true;
    }

    // Handle resize
    int width, height;
    glfwGetFramebufferSize(static_cast<GLFWwindow*>(_window), &width, &height);
    if (width != _wgpu_surface_width || height != _wgpu_surface_height) {
        if (width > 0 && height > 0) {
            configure_wgpu_surface(width, height);
        }
    }

    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    return Ok();
}

Result<void> App::_end_frame() {
    ImGui::Render();

    // Get current surface texture
    WGPUSurfaceTexture surface_texture;
    wgpuSurfaceGetCurrentTexture(_wgpu_surface, &surface_texture);
    if (surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
        return Err<void>("Failed to get surface texture");
    }

    // Create texture view
    WGPUTextureViewDescriptor view_desc = {};
    view_desc.format = _wgpu_preferred_format;
    view_desc.dimension = WGPUTextureViewDimension_2D;
    view_desc.mipLevelCount = 1;
    view_desc.arrayLayerCount = 1;
    WGPUTextureView view = wgpuTextureCreateView(surface_texture.texture, &view_desc);
    if (!view) {
        return Err<void>("Failed to create texture view");
    }

    WGPURenderPassColorAttachment color_attachment = {};
    color_attachment.view = view;
    color_attachment.loadOp = WGPULoadOp_Clear;
    color_attachment.storeOp = WGPUStoreOp_Store;
    color_attachment.clearValue = {0.1f, 0.1f, 0.1f, 1.0f};

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

    // Release texture after present
    wgpuTextureRelease(surface_texture.texture);

    return Ok();
}

#else // OpenGL backend

Result<void> App::_init_graphics() {
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit()) {
        return Err<void>("App::_init_graphics: glfwInit failed");
    }

    // OpenGL 2.1 for maximum compatibility with VirtualGL
    const char* glsl_version = "#version 120";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    _window = glfwCreateWindow(
        _config.window_width,
        _config.window_height,
        _config.window_title.c_str(),
        nullptr,
        nullptr
    );

    if (!_window) {
        glfwTerminate();
        return Err<void>("App::_init_graphics: glfwCreateWindow failed");
    }

    glfwMakeContextCurrent(static_cast<GLFWwindow*>(_window));
    glfwSwapInterval(1); // VSync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Setup style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(static_cast<GLFWwindow*>(_window), true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    return Ok();
}

Result<void> App::_shutdown_graphics() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    if (_window) {
        glfwDestroyWindow(static_cast<GLFWwindow*>(_window));
    }
    glfwTerminate();

    return Ok();
}

Result<void> App::_begin_frame() {
    glfwPollEvents();

    if (glfwWindowShouldClose(static_cast<GLFWwindow*>(_window))) {
        _should_close = true;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    return Ok();
}

Result<void> App::_end_frame() {
    ImGui::Render();

    int display_w, display_h;
    glfwGetFramebufferSize(static_cast<GLFWwindow*>(_window), &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(static_cast<GLFWwindow*>(_window));

    return Ok();
}

#endif // YMERY_USE_WEBGPU

} // namespace ymery
