#include "ymery/app.hpp"
#include <iostream>
#include <filesystem>
#include <spdlog/spdlog.h>

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("ymery-cli starting");

    // Parse command line arguments
    std::vector<std::filesystem::path> layout_paths;
    std::vector<std::filesystem::path> plugin_paths;
    std::string main_module = "app";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-l" || arg == "--layout") {
            if (i + 1 < argc) {
                layout_paths.push_back(argv[++i]);
            }
        } else if (arg == "-m" || arg == "--module") {
            if (i + 1 < argc) {
                main_module = argv[++i];
            }
        } else if (arg == "-p" || arg == "--plugins") {
            if (i + 1 < argc) {
                plugin_paths.push_back(argv[++i]);
            }
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: ymery [options]\n"
                      << "Options:\n"
                      << "  -l, --layout <path>   Add layout search path\n"
                      << "  -m, --module <name>   Main module name (default: app)\n"
                      << "  -p, --plugins <path>  Add plugin search path\n"
                      << "  -h, --help            Show this help\n";
            return 0;
        } else {
            // Treat as layout path
            layout_paths.push_back(arg);
        }
    }

    // Default layout path
    if (layout_paths.empty()) {
        layout_paths.push_back(std::filesystem::current_path() / "layout");
    }

    // Default plugin path - look for plugins directory next to executable
    if (plugin_paths.empty()) {
        auto exe_path = std::filesystem::canonical("/proc/self/exe").parent_path();
        plugin_paths.push_back(exe_path / "plugins");
    }

    // Create app config
    ymery::AppConfig config;
    config.layout_paths = layout_paths;
    config.plugin_paths = plugin_paths;
    config.main_module = main_module;
    config.window_title = "Ymery";

    // Create and run app
    auto app_res = ymery::App::create(config);
    if (!app_res) {
        std::cerr << "Failed to create app: " << ymery::error_msg(app_res) << std::endl;
        return 1;
    }

    auto app = *app_res;
    if (auto run_res = app->run(); !run_res) {
        std::cerr << "App error: " << ymery::error_msg(run_res) << std::endl;
    }

    app->dispose();
    return 0;
}
