// Ymery Editor Android Native Activity
#include <android/log.h>
#include <android_native_app_glue.h>
#include <imgui_impl_android.h>
#include <ymery/editor/editor_app.hpp>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "ymery-editor", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "ymery-editor", __VA_ARGS__)

static std::unique_ptr<ymery::editor::EditorApp> g_app;

static int32_t handle_input(struct android_app* app, AInputEvent* event) {
    return ImGui_ImplAndroid_HandleInputEvent(event);
}

static void handle_cmd(struct android_app* app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            LOGI("APP_CMD_INIT_WINDOW");
            if (app->window != nullptr && !g_app) {
                ymery::editor::EditorConfig config;
                config.window_title = "Ymery Editor";

                g_app = ymery::editor::EditorApp::create(app, config);
                if (g_app) {
                    LOGI("Editor created successfully");
                } else {
                    LOGE("Failed to create editor");
                }
            }
            break;

        case APP_CMD_TERM_WINDOW:
            LOGI("APP_CMD_TERM_WINDOW");
            if (g_app) {
                g_app->dispose();
                g_app.reset();
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
    app->onAppCmd = handle_cmd;
    app->onInputEvent = handle_input;

    LOGI("Ymery Editor Android starting...");

    while (true) {
        int events;
        struct android_poll_source* source;

        while (ALooper_pollAll(g_app ? 0 : -1, nullptr, &events, (void**)&source) >= 0) {
            if (source != nullptr) {
                source->process(app, source);
            }
            if (app->destroyRequested) {
                LOGI("Destroy requested");
                if (g_app) {
                    g_app->dispose();
                    g_app.reset();
                }
                return;
            }
        }

        if (g_app && !g_app->should_close()) {
            g_app->frame();
        }
    }
}
