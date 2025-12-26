#include "embedded.hpp"
#include <imgui.h>
#ifdef YMERY_USE_WEBGPU
#include <imgui_impl_wgpu.h>
#include <webgpu/webgpu.h>
#else
#include <imgui_impl_opengl3.h>
#endif
#include <implot.h>
#include <spdlog/spdlog.h>

namespace ymery {

// GLFW key to ImGui key translation (subset of imgui_impl_glfw.cpp)
static ImGuiKey glfw_key_to_imgui_key(int key) {
    switch (key) {
        case 256: return ImGuiKey_Escape;
        case 257: return ImGuiKey_Enter;
        case 258: return ImGuiKey_Tab;
        case 259: return ImGuiKey_Backspace;
        case 260: return ImGuiKey_Insert;
        case 261: return ImGuiKey_Delete;
        case 262: return ImGuiKey_RightArrow;
        case 263: return ImGuiKey_LeftArrow;
        case 264: return ImGuiKey_DownArrow;
        case 265: return ImGuiKey_UpArrow;
        case 266: return ImGuiKey_PageUp;
        case 267: return ImGuiKey_PageDown;
        case 268: return ImGuiKey_Home;
        case 269: return ImGuiKey_End;
        case 280: return ImGuiKey_CapsLock;
        case 281: return ImGuiKey_ScrollLock;
        case 282: return ImGuiKey_NumLock;
        case 283: return ImGuiKey_PrintScreen;
        case 284: return ImGuiKey_Pause;
        case 290: return ImGuiKey_F1;
        case 291: return ImGuiKey_F2;
        case 292: return ImGuiKey_F3;
        case 293: return ImGuiKey_F4;
        case 294: return ImGuiKey_F5;
        case 295: return ImGuiKey_F6;
        case 296: return ImGuiKey_F7;
        case 297: return ImGuiKey_F8;
        case 298: return ImGuiKey_F9;
        case 299: return ImGuiKey_F10;
        case 300: return ImGuiKey_F11;
        case 301: return ImGuiKey_F12;
        case 320: return ImGuiKey_Keypad0;
        case 321: return ImGuiKey_Keypad1;
        case 322: return ImGuiKey_Keypad2;
        case 323: return ImGuiKey_Keypad3;
        case 324: return ImGuiKey_Keypad4;
        case 325: return ImGuiKey_Keypad5;
        case 326: return ImGuiKey_Keypad6;
        case 327: return ImGuiKey_Keypad7;
        case 328: return ImGuiKey_Keypad8;
        case 329: return ImGuiKey_Keypad9;
        case 330: return ImGuiKey_KeypadDecimal;
        case 331: return ImGuiKey_KeypadDivide;
        case 332: return ImGuiKey_KeypadMultiply;
        case 333: return ImGuiKey_KeypadSubtract;
        case 334: return ImGuiKey_KeypadAdd;
        case 335: return ImGuiKey_KeypadEnter;
        case 336: return ImGuiKey_KeypadEqual;
        case 340: return ImGuiKey_LeftShift;
        case 341: return ImGuiKey_LeftCtrl;
        case 342: return ImGuiKey_LeftAlt;
        case 343: return ImGuiKey_LeftSuper;
        case 344: return ImGuiKey_RightShift;
        case 345: return ImGuiKey_RightCtrl;
        case 346: return ImGuiKey_RightAlt;
        case 347: return ImGuiKey_RightSuper;
        case 348: return ImGuiKey_Menu;
        case 32: return ImGuiKey_Space;
        case 39: return ImGuiKey_Apostrophe;
        case 44: return ImGuiKey_Comma;
        case 45: return ImGuiKey_Minus;
        case 46: return ImGuiKey_Period;
        case 47: return ImGuiKey_Slash;
        case 48: return ImGuiKey_0;
        case 49: return ImGuiKey_1;
        case 50: return ImGuiKey_2;
        case 51: return ImGuiKey_3;
        case 52: return ImGuiKey_4;
        case 53: return ImGuiKey_5;
        case 54: return ImGuiKey_6;
        case 55: return ImGuiKey_7;
        case 56: return ImGuiKey_8;
        case 57: return ImGuiKey_9;
        case 59: return ImGuiKey_Semicolon;
        case 61: return ImGuiKey_Equal;
        case 65: return ImGuiKey_A;
        case 66: return ImGuiKey_B;
        case 67: return ImGuiKey_C;
        case 68: return ImGuiKey_D;
        case 69: return ImGuiKey_E;
        case 70: return ImGuiKey_F;
        case 71: return ImGuiKey_G;
        case 72: return ImGuiKey_H;
        case 73: return ImGuiKey_I;
        case 74: return ImGuiKey_J;
        case 75: return ImGuiKey_K;
        case 76: return ImGuiKey_L;
        case 77: return ImGuiKey_M;
        case 78: return ImGuiKey_N;
        case 79: return ImGuiKey_O;
        case 80: return ImGuiKey_P;
        case 81: return ImGuiKey_Q;
        case 82: return ImGuiKey_R;
        case 83: return ImGuiKey_S;
        case 84: return ImGuiKey_T;
        case 85: return ImGuiKey_U;
        case 86: return ImGuiKey_V;
        case 87: return ImGuiKey_W;
        case 88: return ImGuiKey_X;
        case 89: return ImGuiKey_Y;
        case 90: return ImGuiKey_Z;
        case 91: return ImGuiKey_LeftBracket;
        case 92: return ImGuiKey_Backslash;
        case 93: return ImGuiKey_RightBracket;
        case 96: return ImGuiKey_GraveAccent;
        default: return ImGuiKey_None;
    }
}

Result<std::shared_ptr<EmbeddedApp>> EmbeddedApp::create(const EmbeddedConfig& config) {
#ifdef YMERY_USE_WEBGPU
    if (!config.device) {
        return Err<std::shared_ptr<EmbeddedApp>>("EmbeddedApp::create: device is required");
    }
    if (!config.queue) {
        return Err<std::shared_ptr<EmbeddedApp>>("EmbeddedApp::create: queue is required");
    }
#endif

    auto app = std::shared_ptr<EmbeddedApp>(new EmbeddedApp());
    app->_config = config;
    app->_width = config.width;
    app->_height = config.height;

    if (auto res = app->_init(); !res) {
        return Err<std::shared_ptr<EmbeddedApp>>("EmbeddedApp::create: init failed", res);
    }

    return app;
}

Result<void> EmbeddedApp::_init() {
    spdlog::info("EmbeddedApp::_init starting");

    // Initialize our own ImGui context first
    if (auto res = _init_imgui(); !res) {
        return Err<void>("EmbeddedApp::_init: imgui init failed", res);
    }

    // Create dispatcher
    auto disp_res = Dispatcher::create();
    if (!disp_res) {
        return Err<void>("EmbeddedApp::_init: dispatcher create failed", disp_res);
    }
    _dispatcher = *disp_res;

    // Build colon-separated plugin path string
    std::string plugins_path;
    for (const auto& p : _config.plugin_paths) {
        if (!plugins_path.empty()) plugins_path += ":";
        plugins_path += p.string();
    }

    // Create plugin manager
    auto pm_res = PluginManager::create(plugins_path);
    if (!pm_res) {
        return Err<void>("EmbeddedApp::_init: plugin manager create failed", pm_res);
    }
    _plugin_manager = *pm_res;

    // Load YAML modules
    auto lang_res = Lang::create(_config.layout_paths, _config.main_module);
    if (!lang_res) {
        return Err<void>("EmbeddedApp::_init: lang create failed", lang_res);
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
        return Err<void>("EmbeddedApp::_init: no tree-like plugin available", tree_res);
    }
    _data_tree = *tree_res;

    // Create widget factory with plugin manager
    auto wf_res = WidgetFactory::create(_lang, _dispatcher, _data_tree, _plugin_manager);
    if (!wf_res) {
        return Err<void>("EmbeddedApp::_init: widget factory create failed", wf_res);
    }
    _widget_factory = *wf_res;

    // Create root widget
    spdlog::info("Creating root widget");
    auto root_res = _widget_factory->create_root_widget();
    if (!root_res) {
        spdlog::error("EmbeddedApp::_init: root widget create failed: {}", error_msg(root_res));
        return Err<void>("EmbeddedApp::_init: root widget create failed", root_res);
    }
    _root_widget = *root_res;
    spdlog::info("Root widget created successfully");

    return Ok();
}

Result<void> EmbeddedApp::_init_imgui() {
    // Create separate ImGui context for this instance
    _imgui_ctx = ImGui::CreateContext();
    if (!_imgui_ctx) {
        return Err<void>("EmbeddedApp::_init_imgui: CreateContext failed");
    }
    ImGui::SetCurrentContext(_imgui_ctx);

    // Create ImPlot context
    _implot_ctx = ImPlot::CreateContext();
    if (!_implot_ctx) {
        ImGui::DestroyContext(_imgui_ctx);
        _imgui_ctx = nullptr;
        return Err<void>("EmbeddedApp::_init_imgui: ImPlot CreateContext failed");
    }
    ImPlot::SetCurrentContext(_implot_ctx);

    // Configure ImGui
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr;  // Don't save settings

    // Set display size
    io.DisplaySize = ImVec2(static_cast<float>(_width), static_cast<float>(_height));

    // Style
    ImGui::StyleColorsDark();

#ifdef YMERY_USE_WEBGPU
    // Initialize WebGPU backend (no GLFW backend - we handle input manually)
    ImGui_ImplWGPU_InitInfo wgpu_info = {};
    wgpu_info.Device = _config.device;
    wgpu_info.NumFramesInFlight = 3;
    wgpu_info.RenderTargetFormat = _config.format != 0
        ? static_cast<WGPUTextureFormat>(_config.format)
        : WGPUTextureFormat_BGRA8Unorm;
    wgpu_info.DepthStencilFormat = WGPUTextureFormat_Undefined;

    if (!ImGui_ImplWGPU_Init(&wgpu_info)) {
        ImPlot::DestroyContext(_implot_ctx);
        ImGui::DestroyContext(_imgui_ctx);
        _implot_ctx = nullptr;
        _imgui_ctx = nullptr;
        return Err<void>("EmbeddedApp::_init_imgui: ImGui_ImplWGPU_Init failed");
    }
    spdlog::info("EmbeddedApp: ImGui WebGPU context initialized ({}x{})", _width, _height);
#else
    // Initialize OpenGL backend (no GLFW backend - we handle input manually)
    if (!ImGui_ImplOpenGL3_Init(_config.glsl_version)) {
        ImPlot::DestroyContext(_implot_ctx);
        ImGui::DestroyContext(_imgui_ctx);
        _implot_ctx = nullptr;
        _imgui_ctx = nullptr;
        return Err<void>("EmbeddedApp::_init_imgui: ImGui_ImplOpenGL3_Init failed");
    }
    spdlog::info("EmbeddedApp: ImGui OpenGL context initialized ({}x{})", _width, _height);
#endif

    return Ok();
}

void EmbeddedApp::_shutdown_imgui() {
    if (_imgui_ctx) {
        ImGui::SetCurrentContext(_imgui_ctx);
#ifdef YMERY_USE_WEBGPU
        ImGui_ImplWGPU_Shutdown();
#else
        ImGui_ImplOpenGL3_Shutdown();
#endif

        if (_implot_ctx) {
            ImPlot::DestroyContext(_implot_ctx);
            _implot_ctx = nullptr;
        }

        ImGui::DestroyContext(_imgui_ctx);
        _imgui_ctx = nullptr;
    }
}

void EmbeddedApp::dispose() {
    if (_root_widget) {
        _root_widget->dispose();
        _root_widget.reset();
    }

    if (_plugin_manager) {
        _plugin_manager->dispose();
        _plugin_manager.reset();
    }

    _shutdown_imgui();
}

void EmbeddedApp::begin_frame(float delta_time) {
    if (!_imgui_ctx) return;

    ImGui::SetCurrentContext(_imgui_ctx);
    ImPlot::SetCurrentContext(_implot_ctx);

    ImGuiIO& io = ImGui::GetIO();
    io.DeltaTime = delta_time > 0.0f ? delta_time : 1.0f / 60.0f;

#ifdef YMERY_USE_WEBGPU
    ImGui_ImplWGPU_NewFrame();
#else
    ImGui_ImplOpenGL3_NewFrame();
#endif
    ImGui::NewFrame();
}

void EmbeddedApp::render_widgets() {
    if (!_imgui_ctx) return;

    // Ensure we're in the right context
    ImGui::SetCurrentContext(_imgui_ctx);
    ImPlot::SetCurrentContext(_implot_ctx);

    // Debug: log mouse state before rendering
    ImGuiIO& io = ImGui::GetIO();
    static int frame_count = 0;
    if (io.MouseDown[0] || (frame_count++ % 60 == 0)) {
        spdlog::debug("render_widgets: MousePos=({}, {}) MouseDown={} DisplaySize=({}, {})",
            io.MousePos.x, io.MousePos.y, io.MouseDown[0], io.DisplaySize.x, io.DisplaySize.y);
    }

    // In embedded mode, position and size the window within the framebuffer
    ImGui::SetNextWindowPos(ImVec2(_display_pos_x, _display_pos_y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(_display_w, _display_h), ImGuiCond_Always);

    if (_root_widget) {
        if (auto render_res = _root_widget->render(); !render_res) {
            spdlog::warn("EmbeddedApp::render_widgets: {}", error_msg(render_res));
        }
    }
}

void EmbeddedApp::set_display_pos(float x, float y, float w, float h) {
    _display_pos_x = x;
    _display_pos_y = y;
    _display_w = w;
    _display_h = h;
}

#ifdef YMERY_USE_WEBGPU
void EmbeddedApp::end_frame(WGPURenderPassEncoder pass) {
    if (!_imgui_ctx) return;

    ImGui::SetCurrentContext(_imgui_ctx);

    ImGui::Render();
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass);
}
#else
void EmbeddedApp::end_frame() {
    if (!_imgui_ctx) return;

    ImGui::SetCurrentContext(_imgui_ctx);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
#endif

void EmbeddedApp::resize(uint32_t width, uint32_t height) {
    _width = width;
    _height = height;

    if (_imgui_ctx) {
        ImGui::SetCurrentContext(_imgui_ctx);
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
    }
}

void EmbeddedApp::on_mouse_pos(float x, float y) {
    if (!_imgui_ctx) return;

    ImGui::SetCurrentContext(_imgui_ctx);
    ImGuiIO& io = ImGui::GetIO();
    // Add display position offset - host passes local coords, but ImGui needs absolute coords
    // since we use SetNextWindowPos with the display offset
    float abs_x = x + _display_pos_x;
    float abs_y = y + _display_pos_y;
    spdlog::debug("EmbeddedApp::on_mouse_pos({}, {}) -> abs({}, {}) DisplaySize=({}, {})",
                  x, y, abs_x, abs_y, io.DisplaySize.x, io.DisplaySize.y);
    io.AddMousePosEvent(abs_x, abs_y);
}

void EmbeddedApp::on_mouse_button(int button, bool pressed) {
    if (!_imgui_ctx) return;

    ImGui::SetCurrentContext(_imgui_ctx);
    ImGuiIO& io = ImGui::GetIO();
    spdlog::debug("EmbeddedApp::on_mouse_button({}, {}) MousePos=({}, {})", button, pressed, io.MousePos.x, io.MousePos.y);
    if (button >= 0 && button < ImGuiMouseButton_COUNT) {
        io.AddMouseButtonEvent(button, pressed);
    }
}

void EmbeddedApp::on_mouse_scroll(float x_offset, float y_offset) {
    if (!_imgui_ctx) return;

    ImGui::SetCurrentContext(_imgui_ctx);
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseWheelEvent(x_offset, y_offset);
}

void EmbeddedApp::on_key(int key, int scancode, int action, int mods) {
    if (!_imgui_ctx) return;
    (void)scancode;  // unused

    ImGui::SetCurrentContext(_imgui_ctx);
    ImGuiIO& io = ImGui::GetIO();

    ImGuiKey imgui_key = glfw_key_to_imgui_key(key);
    bool pressed = (action != 0);  // GLFW_PRESS=1, GLFW_REPEAT=2, GLFW_RELEASE=0

    io.AddKeyEvent(imgui_key, pressed);

    // Modifiers (GLFW mod bits)
    io.AddKeyEvent(ImGuiMod_Ctrl, (mods & 0x0002) != 0);   // GLFW_MOD_CONTROL
    io.AddKeyEvent(ImGuiMod_Shift, (mods & 0x0001) != 0);  // GLFW_MOD_SHIFT
    io.AddKeyEvent(ImGuiMod_Alt, (mods & 0x0004) != 0);    // GLFW_MOD_ALT
    io.AddKeyEvent(ImGuiMod_Super, (mods & 0x0008) != 0);  // GLFW_MOD_SUPER
}

void EmbeddedApp::on_char(unsigned int codepoint) {
    if (!_imgui_ctx) return;

    ImGui::SetCurrentContext(_imgui_ctx);
    ImGuiIO& io = ImGui::GetIO();
    io.AddInputCharacter(codepoint);
}

void EmbeddedApp::on_focus(bool focused) {
    if (!_imgui_ctx) return;

    ImGui::SetCurrentContext(_imgui_ctx);
    ImGuiIO& io = ImGui::GetIO();
    io.AddFocusEvent(focused);
}

bool EmbeddedApp::wants_keyboard() const {
    if (!_imgui_ctx) return false;

    ImGui::SetCurrentContext(_imgui_ctx);
    return ImGui::GetIO().WantCaptureKeyboard;
}

bool EmbeddedApp::wants_mouse() const {
    if (!_imgui_ctx) return false;

    ImGui::SetCurrentContext(_imgui_ctx);
    ImGuiIO& io = ImGui::GetIO();
    spdlog::debug("EmbeddedApp::wants_mouse() MousePos=({}, {}) WantCaptureMouse={}", io.MousePos.x, io.MousePos.y, io.WantCaptureMouse);
    return io.WantCaptureMouse;
}

} // namespace ymery
