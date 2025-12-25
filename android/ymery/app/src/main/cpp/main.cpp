// Ymery Android Native Activity
#include <android/log.h>
#include <android_native_app_glue.h>
#include <imgui.h>
#include <imgui_impl_android.h>
#include <ymery/app.hpp>
#include <cmath>
#include <ctime>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "ymery", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "ymery", __VA_ARGS__)

static std::shared_ptr<ymery::App> g_app;

// Long-press detection for right-click
static constexpr float LONG_PRESS_DURATION = 0.5f;  // seconds
static constexpr float LONG_PRESS_MAX_MOVE = 10.0f; // pixels
static double g_touch_down_time = 0.0;
static float g_touch_down_x = 0.0f;
static float g_touch_down_y = 0.0f;
static bool g_touch_is_down = false;
static bool g_long_press_triggered = false;

static double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static int32_t handle_input(struct android_app* app, AInputEvent* event) {
    int32_t event_type = AInputEvent_getType(event);

    if (event_type == AINPUT_EVENT_TYPE_MOTION) {
        int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
        float x = AMotionEvent_getX(event, 0);
        float y = AMotionEvent_getY(event, 0);
        int32_t tool_type = AMotionEvent_getToolType(event, 0);

        // Only handle finger touch for long-press
        if (tool_type == AMOTION_EVENT_TOOL_TYPE_FINGER || tool_type == AMOTION_EVENT_TOOL_TYPE_UNKNOWN) {
            if (action == AMOTION_EVENT_ACTION_DOWN) {
                g_touch_down_time = get_time();
                g_touch_down_x = x;
                g_touch_down_y = y;
                g_touch_is_down = true;
                g_long_press_triggered = false;
            } else if (action == AMOTION_EVENT_ACTION_MOVE) {
                // Cancel if moved too far
                float dx = x - g_touch_down_x;
                float dy = y - g_touch_down_y;
                if (std::sqrt(dx*dx + dy*dy) > LONG_PRESS_MAX_MOVE) {
                    g_touch_is_down = false;
                }
            } else if (action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_CANCEL) {
                g_touch_is_down = false;
            }
        }
    }

    return ImGui_ImplAndroid_HandleInputEvent(event);
}

static void handle_cmd(struct android_app* app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            LOGI("APP_CMD_INIT_WINDOW");
            if (app->window != nullptr && !g_app) {
                ymery::AppConfig config;
                // Plugin path will be added automatically from nativeLibraryDir
                config.window_title = "Ymery";

                auto result = ymery::App::create(app, config);
                if (result) {
                    g_app = *result;
                    LOGI("App created successfully");
                } else {
                    LOGE("Failed to create app: %s", ymery::error_msg(result).c_str());
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

    LOGI("Ymery Android starting...");

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

        if (g_app) {
            // Check for long-press to trigger right-click
            if (g_touch_is_down && !g_long_press_triggered) {
                double elapsed = get_time() - g_touch_down_time;
                if (elapsed >= LONG_PRESS_DURATION) {
                    ImGuiIO& io = ImGui::GetIO();
                    io.AddMousePosEvent(g_touch_down_x, g_touch_down_y);
                    io.AddMouseButtonEvent(1, true);  // Right-click down
                    io.AddMouseButtonEvent(1, false); // Right-click up
                    g_long_press_triggered = true;
                }
            }

            g_app->frame();
        }
    }
}
