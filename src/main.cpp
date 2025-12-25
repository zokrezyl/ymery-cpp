#include "ymery/app.hpp"
#include <iostream>
#include <filesystem>
#include <spdlog/spdlog.h>

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("ymery-cli starting");

    // Parse command line arguments (matching Python CLI)
    std::vector<std::filesystem::path> layout_paths;
    std::vector<std::filesystem::path> plugin_paths;
    std::string main_module = "app";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-p" || arg == "--layouts-path") {
            if (i + 1 < argc) {
                layout_paths.push_back(argv[++i]);
            }
        } else if (arg == "-m" || arg == "--main") {
            if (i + 1 < argc) {
                main_module = argv[++i];
            }
        } else if (arg == "--plugins-path") {
            if (i + 1 < argc) {
                plugin_paths.push_back(argv[++i]);
            }
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: ymery [options]\n"
                      << "Options:\n"
                      << "  -p, --layouts-path <path>  Add layout search path\n"
                      << "  -m, --main <name>          Main module name (default: app)\n"
                      << "  --plugins-path <path>      Add plugin search path\n"
                      << "  -h, --help                 Show this help\n";
            return 0;
        } else {
            // Treat as layout path - if it's a file, use its parent directory
            std::filesystem::path path(arg);
            if (std::filesystem::is_regular_file(path)) {
                layout_paths.push_back(path.parent_path());
            } else {
                layout_paths.push_back(path);
            }
        }
    }

    // Default layout path
    if (layout_paths.empty()) {
        layout_paths.push_back(std::filesystem::current_path());
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
