// Ymery Android Native Activity
#include <android/log.h>
#include <android/configuration.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <imgui.h>
#include <imgui_impl_android.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>

#include <cmath>
#include <vector>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "ymery", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "ymery", __VA_ARGS__)

struct AppState {
    struct android_app* app;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int32_t width;
    int32_t height;
    bool initialized;
    float density;
};

static int init_display(AppState* state) {
    LOGI("init_display starting...");

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
        return -1;
    }

    if (!eglInitialize(display, nullptr, nullptr)) {
        LOGE("eglInitialize failed: 0x%x", eglGetError());
        return -1;
    }

    EGLConfig config;
    EGLint numConfigs;
    if (!eglChooseConfig(display, attribs, &config, 1, &numConfigs) || numConfigs == 0) {
        LOGE("eglChooseConfig failed: 0x%x, numConfigs=%d", eglGetError(), numConfigs);
        return -1;
    }

    EGLint format;
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(state->app->window, 0, 0, format);

    EGLSurface surface = eglCreateWindowSurface(display, config, state->app->window, nullptr);
    if (surface == EGL_NO_SURFACE) {
        LOGE("eglCreateWindowSurface failed: 0x%x", eglGetError());
        return -1;
    }

    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
    if (context == EGL_NO_CONTEXT) {
        LOGE("eglCreateContext failed: 0x%x", eglGetError());
        return -1;
    }

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
        LOGE("eglMakeCurrent failed: 0x%x", eglGetError());
        return -1;
    }

    eglQuerySurface(display, surface, EGL_WIDTH, &state->width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &state->height);

    state->display = display;
    state->context = context;
    state->surface = surface;

    // Get density
    state->density = AConfiguration_getDensity(state->app->config) / 160.0f;
    if (state->density < 1.0f) state->density = 1.0f;

    LOGI("EGL initialized: %dx%d, density=%.2f", state->width, state->height, state->density);

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.FontGlobalScale = state->density;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(state->density);

    ImGui_ImplAndroid_Init(state->app->window);
    ImGui_ImplOpenGL3_Init("#version 300 es");

    state->initialized = true;
    LOGI("ImGui initialized successfully");
    return 0;
}

static void term_display(AppState* state) {
    LOGI("term_display");
    if (state->initialized) {
        ImPlot::DestroyContext();
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplAndroid_Shutdown();
        ImGui::DestroyContext();
        state->initialized = false;
    }

    if (state->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(state->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (state->context != EGL_NO_CONTEXT) {
            eglDestroyContext(state->display, state->context);
        }
        if (state->surface != EGL_NO_SURFACE) {
            eglDestroySurface(state->display, state->surface);
        }
        eglTerminate(state->display);
    }
    state->display = EGL_NO_DISPLAY;
    state->context = EGL_NO_CONTEXT;
    state->surface = EGL_NO_SURFACE;
}

static void draw_frame(AppState* state) {
    if (state->display == EGL_NO_DISPLAY) return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();

    // Demo UI
    static int counter = 0;
    static float slider_value = 0.5f;
    static int slider_int = 50;
    static bool checkbox = true;
    static char text_buf[256] = "Hello!";
    static std::vector<float> plot_data;
    static float time_acc = 0.0f;

    // Generate plot data
    time_acc += ImGui::GetIO().DeltaTime;
    if (plot_data.size() > 100) plot_data.erase(plot_data.begin());
    plot_data.push_back(sinf(time_acc * 2.0f));

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(state->width - 20.0f, state->height - 20.0f), ImGuiCond_FirstUseEver);

    ImGui::Begin("Ymery on Android");

    ImGui::Text("Welcome to Ymery!");
    ImGui::Text("Screen: %dx%d @ %.1fx", state->width, state->height, state->density);
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

    ImGui::Separator();

    if (ImGui::Button("Click Me!")) {
        counter++;
    }
    ImGui::SameLine();
    ImGui::Text("Clicks: %d", counter);

    ImGui::SliderFloat("Float Slider", &slider_value, 0.0f, 1.0f);
    ImGui::SliderInt("Int Slider", &slider_int, 0, 100);
    ImGui::Checkbox("Checkbox", &checkbox);
    ImGui::InputText("Text Input", text_buf, sizeof(text_buf));

    ImGui::Separator();

    if (ImPlot::BeginPlot("Sine Wave", ImVec2(-1, 200 * state->density))) {
        ImPlot::PlotLine("sin(t)", plot_data.data(), (int)plot_data.size());
        ImPlot::EndPlot();
    }

    ImGui::End();

    ImGui::Render();

    glViewport(0, 0, state->width, state->height);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    eglSwapBuffers(state->display, state->surface);
}

static int32_t handle_input(struct android_app* app, AInputEvent* event) {
    return ImGui_ImplAndroid_HandleInputEvent(event);
}

static void handle_cmd(struct android_app* app, int32_t cmd) {
    AppState* state = (AppState*)app->userData;
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            LOGI("APP_CMD_INIT_WINDOW");
            if (app->window != nullptr) {
                init_display(state);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            LOGI("APP_CMD_TERM_WINDOW");
            term_display(state);
            break;
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONFIG_CHANGED:
            LOGI("APP_CMD_CONFIG_CHANGED");
            if (state->display != EGL_NO_DISPLAY) {
                eglQuerySurface(state->display, state->surface, EGL_WIDTH, &state->width);
                eglQuerySurface(state->display, state->surface, EGL_HEIGHT, &state->height);
            }
            break;
        case APP_CMD_GAINED_FOCUS:
            LOGI("APP_CMD_GAINED_FOCUS");
            break;
        case APP_CMD_LOST_FOCUS:
            LOGI("APP_CMD_LOST_FOCUS");
            break;
    }
}

void android_main(struct android_app* app) {
    AppState state = {};
    state.app = app;
    state.display = EGL_NO_DISPLAY;
    state.surface = EGL_NO_SURFACE;
    state.context = EGL_NO_CONTEXT;
    state.density = 1.0f;

    app->userData = &state;
    app->onAppCmd = handle_cmd;
    app->onInputEvent = handle_input;

    LOGI("Ymery Android starting...");

    while (true) {
        int events;
        struct android_poll_source* source;

        while (ALooper_pollAll(state.initialized ? 0 : -1, nullptr, &events, (void**)&source) >= 0) {
            if (source != nullptr) {
                source->process(app, source);
            }
            if (app->destroyRequested) {
                LOGI("Destroy requested");
                term_display(&state);
                return;
            }
        }

        if (state.initialized) {
            draw_frame(&state);
        }
    }
}
