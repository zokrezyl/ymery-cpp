#include "app.hpp"

#ifdef YMERY_ANDROID
#include <android/log.h>
#include <android/configuration.h>
#include <android_native_app_glue.h>
#include <jni.h>
#include <imgui.h>
#include <imgui_impl_android.h>
#ifdef YMERY_USE_WEBGPU
#include <imgui_impl_wgpu.h>
#include <webgpu/webgpu.h>
#else
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <imgui_impl_opengl3.h>
#endif
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
#ifndef YMERY_WEB_DAWN
#include <emscripten/html5_webgpu.h>
#endif
#elif defined(YMERY_USE_WEBGPU) && !defined(YMERY_ANDROID)
// For native surface creation with wgpu-native
#ifdef __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#else
#define GLFW_EXPOSE_NATIVE_X11
#endif
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
    spdlog::debug("App::init starting");

    // Initialize graphics backend (platform-specific)
    if (auto gfx_res = _init_graphics(); !gfx_res) {
        spdlog::error("App::init: graphics init failed");
        return Err<void>("App::init: graphics init failed", gfx_res);
    }
    spdlog::debug("Graphics initialized");

    // Initialize core components (platform-independent, implemented in app-core.cpp)
    if (auto core_res = _init_core(); !core_res) {
        return Err<void>("App::init: core init failed", core_res);
    }

    return Ok();
}

Result<void> App::dispose() {
    // Dispose core components (platform-independent, implemented in app-core.cpp)
    _dispose_core();

    // Shutdown graphics (platform-specific)
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

#if defined(YMERY_ANDROID) && !defined(YMERY_USE_WEBGPU)
// Android/EGL backend (OpenGL ES)

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

#elif defined(YMERY_ANDROID) && defined(YMERY_USE_WEBGPU)
// Android/WebGPU backend

Result<void> App::_init_graphics() {
    LOGI("App::_init_graphics starting (Android/WebGPU)");

    if (!_android_app || !_android_app->window) {
        return Err<void>("App::_init_graphics: no Android window");
    }

    // Get window dimensions
    _display_width = ANativeWindow_getWidth(_android_app->window);
    _display_height = ANativeWindow_getHeight(_android_app->window);

    // Get density
    _display_density = AConfiguration_getDensity(_android_app->config) / 160.0f;
    if (_display_density < 1.0f) _display_density = 1.0f;

    LOGI("Window: %dx%d, density=%.2f", _display_width, _display_height, _display_density);

    // Create WebGPU instance
    WGPUInstanceDescriptor instance_desc = {};
    _wgpu_instance = wgpuCreateInstance(&instance_desc);
    if (!_wgpu_instance) {
        return Err<void>("App::_init_graphics: wgpuCreateInstance failed");
    }
    LOGI("WebGPU instance created");

    // Create surface from Android native window
    WGPUSurfaceSourceAndroidNativeWindow android_surface_desc = {};
    android_surface_desc.chain.sType = WGPUSType_SurfaceSourceAndroidNativeWindow;
    android_surface_desc.window = _android_app->window;

    WGPUSurfaceDescriptor surface_desc = {};
    surface_desc.nextInChain = &android_surface_desc.chain;
    _wgpu_surface = wgpuInstanceCreateSurface(_wgpu_instance, &surface_desc);
    if (!_wgpu_surface) {
        return Err<void>("App::_init_graphics: wgpuInstanceCreateSurface failed");
    }
    LOGI("WebGPU surface created");

    // Request adapter
    WGPURequestAdapterOptions adapter_opts = {};
    adapter_opts.compatibleSurface = _wgpu_surface;
    adapter_opts.powerPreference = WGPUPowerPreference_HighPerformance;

    WGPURequestAdapterCallbackInfo adapter_callback_info = {};
    adapter_callback_info.mode = WGPUCallbackMode_AllowSpontaneous;
    adapter_callback_info.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* userdata1, void* userdata2) {
        if (status == WGPURequestAdapterStatus_Success) {
            *static_cast<WGPUAdapter*>(userdata1) = adapter;
        } else {
            __android_log_print(ANDROID_LOG_ERROR, "ymery", "Adapter request failed: %.*s",
                (int)message.length, message.data ? message.data : "unknown");
        }
    };
    adapter_callback_info.userdata1 = &_wgpu_adapter;
    wgpuInstanceRequestAdapter(_wgpu_instance, &adapter_opts, adapter_callback_info);

    if (!_wgpu_adapter) {
        return Err<void>("App::_init_graphics: wgpuInstanceRequestAdapter failed");
    }
    LOGI("WebGPU adapter obtained");

    // Request device
    WGPUDeviceDescriptor device_desc = {};
    device_desc.label = {"ymery device", WGPU_STRLEN};
    device_desc.requiredFeatureCount = 0;
    device_desc.requiredLimits = nullptr;
    device_desc.defaultQueue.label = {"default queue", WGPU_STRLEN};

    WGPURequestDeviceCallbackInfo device_callback_info = {};
    device_callback_info.mode = WGPUCallbackMode_AllowSpontaneous;
    device_callback_info.callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* userdata1, void* userdata2) {
        if (status == WGPURequestDeviceStatus_Success) {
            *static_cast<WGPUDevice*>(userdata1) = device;
        } else {
            __android_log_print(ANDROID_LOG_ERROR, "ymery", "Device request failed: %.*s",
                (int)message.length, message.data ? message.data : "unknown");
        }
    };
    device_callback_info.userdata1 = &_wgpu_device;
    wgpuAdapterRequestDevice(_wgpu_adapter, &device_desc, device_callback_info);

    if (!_wgpu_device) {
        return Err<void>("App::_init_graphics: wgpuAdapterRequestDevice failed");
    }
    LOGI("WebGPU device obtained");

    // Get queue
    _wgpu_queue = wgpuDeviceGetQueue(_wgpu_device);

    // Get preferred surface format
    WGPUSurfaceCapabilities caps = {};
    wgpuSurfaceGetCapabilities(_wgpu_surface, _wgpu_adapter, &caps);
    if (caps.formatCount > 0) {
        _wgpu_preferred_format = caps.formats[0];
    }
    wgpuSurfaceCapabilitiesFreeMembers(caps);

    // Configure surface
    _wgpu_surface_width = _display_width;
    _wgpu_surface_height = _display_height;

    WGPUSurfaceConfiguration config = {};
    config.device = _wgpu_device;
    config.format = _wgpu_preferred_format;
    config.usage = WGPUTextureUsage_RenderAttachment;
    config.alphaMode = WGPUCompositeAlphaMode_Auto;
    config.width = _wgpu_surface_width;
    config.height = _wgpu_surface_height;
    config.presentMode = WGPUPresentMode_Fifo;

    wgpuSurfaceConfigure(_wgpu_surface, &config);
    LOGI("WebGPU surface configured");

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

    // Initialize ImGui backends
    ImGui_ImplAndroid_Init(_android_app->window);

    ImGui_ImplWGPU_InitInfo wgpu_init_info = {};
    wgpu_init_info.Device = _wgpu_device;
    wgpu_init_info.RenderTargetFormat = _wgpu_preferred_format;
    wgpu_init_info.DepthStencilFormat = WGPUTextureFormat_Undefined;
    wgpu_init_info.NumFramesInFlight = 3;
    ImGui_ImplWGPU_Init(&wgpu_init_info);

    LOGI("ImGui initialized successfully (Android/WebGPU)");
    return Ok();
}

Result<void> App::_shutdown_graphics() {
    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplAndroid_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    if (_wgpu_surface) {
        wgpuSurfaceUnconfigure(_wgpu_surface);
        wgpuSurfaceRelease(_wgpu_surface);
        _wgpu_surface = nullptr;
    }
    if (_wgpu_device) {
        wgpuDeviceRelease(_wgpu_device);
        _wgpu_device = nullptr;
    }
    if (_wgpu_adapter) {
        wgpuAdapterRelease(_wgpu_adapter);
        _wgpu_adapter = nullptr;
    }
    if (_wgpu_instance) {
        wgpuInstanceRelease(_wgpu_instance);
        _wgpu_instance = nullptr;
    }

    return Ok();
}

Result<void> App::_begin_frame() {
    // Get current texture view
    WGPUSurfaceTexture surface_texture;
    wgpuSurfaceGetCurrentTexture(_wgpu_surface, &surface_texture);

    if (surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        return Err<void>("Failed to get surface texture");
    }

    WGPUTextureViewDescriptor view_desc = {};
    view_desc.format = _wgpu_preferred_format;
    view_desc.dimension = WGPUTextureViewDimension_2D;
    view_desc.baseMipLevel = 0;
    view_desc.mipLevelCount = 1;
    view_desc.baseArrayLayer = 0;
    view_desc.arrayLayerCount = 1;
    view_desc.aspect = WGPUTextureAspect_All;

    WGPUTextureView texture_view = wgpuTextureCreateView(surface_texture.texture, &view_desc);
    if (!texture_view) {
        return Err<void>("Failed to create texture view");
    }

    // Begin ImGui frame
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();

    // Store texture view for _end_frame
    _wgpu_current_texture_view = texture_view;

    return Ok();
}

Result<void> App::_end_frame() {
    ImGui::Render();

    WGPUTextureView texture_view = static_cast<WGPUTextureView>(_wgpu_current_texture_view);

    // Create render pass
    WGPURenderPassColorAttachment color_attachment = {};
    color_attachment.view = texture_view;
    color_attachment.resolveTarget = nullptr;
    color_attachment.loadOp = WGPULoadOp_Clear;
    color_attachment.storeOp = WGPUStoreOp_Store;
    color_attachment.clearValue = {0.1f, 0.1f, 0.1f, 1.0f};
    color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

    WGPURenderPassDescriptor render_pass_desc = {};
    render_pass_desc.colorAttachmentCount = 1;
    render_pass_desc.colorAttachments = &color_attachment;
    render_pass_desc.depthStencilAttachment = nullptr;

    WGPUCommandEncoderDescriptor encoder_desc = {};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(_wgpu_device, &encoder_desc);
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &render_pass_desc);

    // Render ImGui
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass);

    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    WGPUCommandBufferDescriptor cmd_buffer_desc = {};
    WGPUCommandBuffer cmd_buffer = wgpuCommandEncoderFinish(encoder, &cmd_buffer_desc);
    wgpuCommandEncoderRelease(encoder);

    wgpuQueueSubmit(_wgpu_queue, 1, &cmd_buffer);
    wgpuCommandBufferRelease(cmd_buffer);

    // Release texture view and present
    wgpuTextureViewRelease(texture_view);
    _wgpu_current_texture_view = nullptr;

    wgpuSurfacePresent(_wgpu_surface);

    return Ok();
}

#elif defined(YMERY_USE_WEBGPU)

// WebGPU callbacks (v22+ API with WGPUStringView)
static void wgpu_error_callback(WGPUDevice const* device, WGPUErrorType type, WGPUStringView message, void* userdata1, void* userdata2) {
    std::string msg(message.data, message.length);
    spdlog::error("WebGPU Error ({}): {}", static_cast<int>(type), msg);
}

static void wgpu_device_lost_callback(WGPUDevice const* device, WGPUDeviceLostReason reason, WGPUStringView message, void* userdata1, void* userdata2) {
    std::string msg(message.data, message.length);
    spdlog::error("WebGPU Device Lost ({}): {}", static_cast<int>(reason), msg);
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
    spdlog::debug("_init_graphics starting (WebGPU desktop/web)");
    glfwSetErrorCallback(glfw_error_callback);

    spdlog::debug("Calling glfwInit");
    if (!glfwInit()) {
        return Err<void>("App::_init_graphics: glfwInit failed");
    }
    spdlog::debug("glfwInit succeeded");

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
#ifdef YMERY_WEB_DAWN
    // Emscripten + Dawn WebGPU: create surface from canvas
    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvas_desc = {};
    canvas_desc.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
    canvas_desc.selector = {.data = "#canvas", .length = 7};

    WGPUSurfaceDescriptor surface_desc = {};
    surface_desc.nextInChain = &canvas_desc.chain;

    _wgpu_surface = wgpuInstanceCreateSurface(_wgpu_instance, &surface_desc);
#elif defined(YMERY_WEB)
    // Emscripten: create surface from canvas (old API)
    WGPUSurfaceDescriptorFromCanvasHTMLSelector canvas_desc = {};
    canvas_desc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
    canvas_desc.selector = "#canvas";

    WGPUSurfaceDescriptor surface_desc = {};
    surface_desc.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&canvas_desc);

    _wgpu_surface = wgpuInstanceCreateSurface(_wgpu_instance, &surface_desc);
#elif defined(__APPLE__)
    // macOS: create surface from Cocoa window using Metal
    id metal_layer = nullptr;
    NSWindow* ns_window = glfwGetCocoaWindow(static_cast<GLFWwindow*>(_window));
    [ns_window.contentView setWantsLayer:YES];
    metal_layer = [CAMetalLayer layer];
    [ns_window.contentView setLayer:metal_layer];

    WGPUSurfaceSourceMetalLayer metal_surface_desc = {};
    metal_surface_desc.chain.sType = WGPUSType_SurfaceSourceMetalLayer;
    metal_surface_desc.layer = metal_layer;

    WGPUSurfaceDescriptor surface_desc = {};
    surface_desc.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&metal_surface_desc);

    _wgpu_surface = wgpuInstanceCreateSurface(_wgpu_instance, &surface_desc);
#else
    // Linux: create surface from X11 window
    Display* x11_display = glfwGetX11Display();
    Window x11_window = glfwGetX11Window(static_cast<GLFWwindow*>(_window));

    WGPUSurfaceSourceXlibWindow x11_surface_desc = {};
    x11_surface_desc.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
    x11_surface_desc.display = x11_display;
    x11_surface_desc.window = static_cast<uint64_t>(x11_window);

    WGPUSurfaceDescriptor surface_desc = {};
    surface_desc.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&x11_surface_desc);

    _wgpu_surface = wgpuInstanceCreateSurface(_wgpu_instance, &surface_desc);
#endif
    if (!_wgpu_surface) {
        return Err<void>("App::_init_graphics: wgpuInstanceCreateSurface failed");
    }
    spdlog::debug("Surface created, requesting adapter");

    // Request adapter
    WGPURequestAdapterOptions adapter_opts = {};
    adapter_opts.compatibleSurface = _wgpu_surface;
    adapter_opts.powerPreference = WGPUPowerPreference_HighPerformance;

    struct AdapterUserData {
        WGPUAdapter adapter = nullptr;
        bool done = false;
    } adapter_data;

    // v22+ API: use WGPURequestAdapterCallbackInfo
    WGPURequestAdapterCallbackInfo adapter_callback_info = {};
#ifdef YMERY_WEB_DAWN
    spdlog::debug("Using WGPUCallbackMode_AllowProcessEvents for web");
    adapter_callback_info.mode = WGPUCallbackMode_AllowProcessEvents;
#else
    adapter_callback_info.mode = WGPUCallbackMode_AllowSpontaneous;
#endif
    adapter_callback_info.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* userdata1, void* userdata2) {
        auto* data = static_cast<AdapterUserData*>(userdata1);
        if (status == WGPURequestAdapterStatus_Success) {
            data->adapter = adapter;
        } else {
            std::string msg(message.data, message.length);
            spdlog::error("Failed to get WebGPU adapter: {}", msg);
        }
        data->done = true;
    };
    adapter_callback_info.userdata1 = &adapter_data;
    spdlog::debug("Calling wgpuInstanceRequestAdapter");
    wgpuInstanceRequestAdapter(_wgpu_instance, &adapter_opts, adapter_callback_info);
    spdlog::debug("wgpuInstanceRequestAdapter returned, waiting for callback");

    while (!adapter_data.done) {
        // Process events to trigger callbacks
#ifdef YMERY_WEB_DAWN
        wgpuInstanceProcessEvents(_wgpu_instance);
        emscripten_sleep(10);
#endif
    }
    spdlog::debug("Adapter callback completed");

    _wgpu_adapter = adapter_data.adapter;
    if (!_wgpu_adapter) {
        return Err<void>("App::_init_graphics: failed to get WebGPU adapter");
    }

    // Request device
    WGPUDeviceDescriptor device_desc = {};
    // v22+ API: set callbacks via callback info structs
#ifdef YMERY_WEB_DAWN
    device_desc.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
#else
    device_desc.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
#endif
    device_desc.deviceLostCallbackInfo.callback = wgpu_device_lost_callback;
    device_desc.uncapturedErrorCallbackInfo.callback = wgpu_error_callback;

    struct DeviceUserData {
        WGPUDevice device = nullptr;
        bool done = false;
    } device_data;

    // v22+ API: use WGPURequestDeviceCallbackInfo
    WGPURequestDeviceCallbackInfo device_callback_info = {};
#ifdef YMERY_WEB_DAWN
    device_callback_info.mode = WGPUCallbackMode_AllowProcessEvents;
#else
    device_callback_info.mode = WGPUCallbackMode_AllowSpontaneous;
#endif
    device_callback_info.callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* userdata1, void* userdata2) {
        auto* data = static_cast<DeviceUserData*>(userdata1);
        if (status == WGPURequestDeviceStatus_Success) {
            data->device = device;
        } else {
            std::string msg(message.data, message.length);
            spdlog::error("Failed to get WebGPU device: {}", msg);
        }
        data->done = true;
    };
    device_callback_info.userdata1 = &device_data;
    wgpuAdapterRequestDevice(_wgpu_adapter, &device_desc, device_callback_info);

    while (!device_data.done) {
        // Process events to trigger callbacks
#ifdef YMERY_WEB_DAWN
        wgpuInstanceProcessEvents(_wgpu_instance);
        emscripten_sleep(10);
#endif
    }

    _wgpu_device = device_data.device;
    if (!_wgpu_device) {
        return Err<void>("App::_init_graphics: failed to get WebGPU device");
    }

    _wgpu_queue = wgpuDeviceGetQueue(_wgpu_device);

    // Get preferred surface format (v22+ API: wgpuSurfaceGetCapabilities)
    WGPUSurfaceCapabilities caps = {};
    wgpuSurfaceGetCapabilities(_wgpu_surface, _wgpu_adapter, &caps);
    _wgpu_preferred_format = (caps.formatCount > 0) ? caps.formats[0] : WGPUTextureFormat_BGRA8Unorm;
    wgpuSurfaceCapabilitiesFreeMembers(caps);

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
    if (surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
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
    color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

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

#ifndef YMERY_WEB
    // Browser handles presentation via requestAnimationFrame
    wgpuSurfacePresent(_wgpu_surface);
#endif

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
