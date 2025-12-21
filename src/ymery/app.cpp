#include "app.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

namespace ymery {

Result<std::shared_ptr<App>> App::create(const AppConfig& config) {
    auto app = std::shared_ptr<App>(new App());
    app->_config = config;

    if (auto res = app->init(); !res) {
        return Err<std::shared_ptr<App>>("App::create: init failed", res);
    }

    return app;
}

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

    // Create data tree from plugin (or fallback)
    auto tree_res = _plugin_manager->create_tree("simple-tree");
    if (!tree_res) {
        spdlog::warn("Could not create simple-tree from plugin: {}", error_msg(tree_res));
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

Result<void> App::run() {
    spdlog::info("App::run starting main loop");
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

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

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

} // namespace ymery
