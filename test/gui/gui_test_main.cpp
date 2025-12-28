// GUI Tests using imgui_test_engine
// Main entry point for GUI automation tests
//
// NOTE: Full GUI testing requires a graphics backend. This test currently
// runs in a simplified mode that verifies test registration works.
// For full testing, run with the ymery-cli integration or use -gui flag
// with proper graphics initialization.

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

#include "imgui_test_engine/imgui_te_engine.h"
#include "imgui_test_engine/imgui_te_ui.h"
#include "imgui_test_engine/imgui_te_utils.h"
#include "imgui_test_engine/imgui_te_coroutine.h"

#include "ymery/embedded.hpp"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <cstring>

// Test registration functions (defined in separate files)
extern void RegisterTreeNodeTests(ImGuiTestEngine* engine);
extern void RegisterBasicWidgetTests(ImGuiTestEngine* engine);

// Global app state for tests
static ymery::EmbeddedConfig g_config;
static std::shared_ptr<ymery::EmbeddedApp> g_app;

// Helper to get test layout path
static std::filesystem::path get_layout_path(const std::string& layout_name) {
    return std::filesystem::path(TEST_LAYOUTS_DIR) / layout_name / "app.yaml";
}

// Initialize ymery app with a specific layout
bool init_app_with_layout(const std::string& layout_name) {
    auto layout_path = get_layout_path(layout_name);

    if (!std::filesystem::exists(layout_path)) {
        spdlog::error("Layout not found: {}", layout_path.string());
        return false;
    }

    g_config.layout_paths = { layout_path.parent_path() };
    g_config.main_module = "app";

    // Get plugins path from build directory
    std::filesystem::path exe_path = std::filesystem::current_path();
    g_config.plugin_paths = { exe_path / "plugins" };

    auto app_res = ymery::EmbeddedApp::create(g_config);
    if (!app_res) {
        spdlog::error("Failed to create app: {}", ymery::error_msg(app_res));
        return false;
    }

    g_app = *app_res;
    return true;
}

// Cleanup app
void cleanup_app() {
    if (g_app) {
        g_app->dispose();
        g_app.reset();
    }
}

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("GUI Tests starting");

    // Parse command line args
    bool list_only = false;
    bool fast_mode = true;
    const char* filter = nullptr;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-list") == 0) {
            list_only = true;
        } else if (strcmp(argv[i], "-slow") == 0) {
            fast_mode = false;
        } else if (strcmp(argv[i], "-filter") == 0 && i + 1 < argc) {
            filter = argv[++i];
        }
    }

    spdlog::info("Creating ImGui context...");

    // Create ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Set display size for headless mode
    io.DisplaySize = ImVec2(1280, 720);

    // Build font atlas (required for headless mode)
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID((ImTextureID)(intptr_t)1);  // Dummy texture ID

    spdlog::info("Creating test engine...");

    // Create test engine
    ImGuiTestEngine* engine = ImGuiTestEngine_CreateContext();
    if (!engine) {
        spdlog::error("Failed to create test engine");
        ImGui::DestroyContext();
        return 1;
    }

    ImGuiTestEngineIO& test_io = ImGuiTestEngine_GetIO(engine);

    // Set up coroutine interface (required)
    test_io.CoroutineFuncs = Coroutine_ImplStdThread_GetInterface();

    test_io.ConfigVerboseLevel = ImGuiTestVerboseLevel_Info;
    test_io.ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Debug;
    test_io.ConfigRunSpeed = fast_mode ? ImGuiTestRunSpeed_Fast : ImGuiTestRunSpeed_Normal;

    spdlog::info("Starting test engine...");

    // Start test engine
    ImGuiTestEngine_Start(engine, ImGui::GetCurrentContext());
    ImGuiTestEngine_InstallDefaultCrashHandler();

    spdlog::info("Registering tests...");

    // Register all tests
    RegisterTreeNodeTests(engine);
    RegisterBasicWidgetTests(engine);

    // Get test list
    ImVector<ImGuiTest*> tests;
    ImGuiTestEngine_GetTestList(engine, &tests);
    spdlog::info("Registered {} tests", tests.Size);

    if (list_only) {
        // Just list tests and exit
        for (int i = 0; i < tests.Size; i++) {
            spdlog::info("  [{}] {}/{}", i, tests[i]->Category, tests[i]->Name);
        }
        spdlog::info("Use -filter to run specific tests");
        spdlog::info("Note: Full test execution requires graphics backend integration");

        ImGuiTestEngine_Stop(engine);
        ImGui::DestroyContext();
        ImGuiTestEngine_DestroyContext(engine);
        return 0;
    }

    // Queue tests
    if (filter) {
        ImGuiTestEngine_QueueTests(engine, ImGuiTestGroup_Tests, filter);
    } else {
        ImGuiTestEngine_QueueTests(engine, ImGuiTestGroup_Tests, nullptr);
    }

    // Run tests in headless mode
    int frame_count = 0;
    const int max_frames = 10000;

    spdlog::info("Running tests (max {} frames)...", max_frames);

    // Run at least one frame to start tests, then continue until done
    bool tests_done = false;
    while (!tests_done && frame_count < max_frames) {
        io.DeltaTime = 1.0f / 60.0f;
        ImGui::NewFrame();

        // Render ymery widgets if app is initialized
        if (g_app) {
            g_app->render_widgets();
        }

        ImGui::EndFrame();
        ImGui::Render();  // Generate draw data (required even in headless mode)

        // Post-swap handler (required for test engine)
        ImGuiTestEngine_PostSwap(engine);

        frame_count++;

        // Check if tests are still running (after at least one frame)
        if (frame_count > 1 && !test_io.IsRunningTests) {
            tests_done = true;
        }
    }

    if (frame_count >= max_frames) {
        spdlog::error("Test execution exceeded max frames ({})", max_frames);
    }

    // Get results
    int count_tested = 0;
    int count_success = 0;
    ImGuiTestEngine_GetResult(engine, count_tested, count_success);

    int count_failed = count_tested - count_success;
    spdlog::info("Test Results: {} tested, {} success, {} failed ({} frames)",
                 count_tested, count_success, count_failed, frame_count);

    // Cleanup
    // IMPORTANT: Stop test engine, then destroy ImGui context BEFORE test engine context
    ImGuiTestEngine_Stop(engine);
    cleanup_app();
    ImGui::DestroyContext();
    ImGuiTestEngine_DestroyContext(engine);  // After ImGui context

    return count_failed > 0 ? 1 : 0;
}
