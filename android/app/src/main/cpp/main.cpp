// Ymery Android Native Activity
#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <imgui.h>
#include <imgui_impl_android.h>
#include <imgui_impl_opengl3.h>

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
};

static int init_display(AppState* state) {
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
    eglInitialize(display, nullptr, nullptr);

    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);

    EGLint format;
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(state->app->window, 0, 0, format);

    EGLSurface surface = eglCreateWindowSurface(display, config, state->app->window, nullptr);
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
        LOGE("Unable to eglMakeCurrent");
        return -1;
    }

    eglQuerySurface(display, surface, EGL_WIDTH, &state->width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &state->height);

    state->display = display;
    state->context = context;
    state->surface = surface;

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();
    ImGui_ImplAndroid_Init(state->app->window);
    ImGui_ImplOpenGL3_Init("#version 300 es");

    state->initialized = true;
    LOGI("Display initialized: %dx%d", state->width, state->height);
    return 0;
}

static void term_display(AppState* state) {
    if (state->initialized) {
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

    // Demo window
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);
    ImGui::Begin("Ymery on Android");
    ImGui::Text("Hello from Ymery!");
    ImGui::Text("Screen: %dx%d", state->width, state->height);
    static int counter = 0;
    if (ImGui::Button("Click me!")) {
        counter++;
    }
    ImGui::Text("Clicks: %d", counter);
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
            if (app->window != nullptr) {
                init_display(state);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            term_display(state);
            break;
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONFIG_CHANGED:
            if (state->display != EGL_NO_DISPLAY) {
                eglQuerySurface(state->display, state->surface, EGL_WIDTH, &state->width);
                eglQuerySurface(state->display, state->surface, EGL_HEIGHT, &state->height);
            }
            break;
    }
}

void android_main(struct android_app* app) {
    AppState state = {};
    state.app = app;
    state.display = EGL_NO_DISPLAY;
    state.surface = EGL_NO_SURFACE;
    state.context = EGL_NO_CONTEXT;

    app->userData = &state;
    app->onAppCmd = handle_cmd;
    app->onInputEvent = handle_input;

    while (true) {
        int events;
        struct android_poll_source* source;

        while (ALooper_pollAll(state.initialized ? 0 : -1, nullptr, &events, (void**)&source) >= 0) {
            if (source != nullptr) {
                source->process(app, source);
            }
            if (app->destroyRequested) {
                term_display(&state);
                return;
            }
        }

        if (state.initialized) {
            draw_frame(&state);
        }
    }
}
