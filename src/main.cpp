#include "ymery/app.hpp"
#include "ymery/log_buffer.hpp"
#include <iostream>
#include <filesystem>
#include <spdlog/spdlog.h>

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::info);
    ymery::setup_log_buffer_sink();  // Capture spdlog messages for logs-view widget
    spdlog::info("ymery-cli starting");

    // Parse command line arguments
    std::vector<std::filesystem::path> layout_paths;
    std::vector<std::filesystem::path> plugin_paths;
    std::filesystem::path main_file;  // Now a file path, not a module name

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-p" || arg == "--layouts-path") {
            if (i + 1 < argc) {
                layout_paths.push_back(argv[++i]);
            }
        } else if (arg == "-m" || arg == "--main") {
            if (i + 1 < argc) {
                main_file = argv[++i];
            }
        } else if (arg == "--plugins-path") {
            if (i + 1 < argc) {
                plugin_paths.push_back(argv[++i]);
            }
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: ymery [options] [layout-file]\n"
                      << "Options:\n"
                      << "  -p, --layouts-path <path>  Add layout search path (for imports)\n"
                      << "  -m, --main <file>          Main layout file\n"
                      << "  --plugins-path <path>      Add plugin search path\n"
                      << "  -h, --help                 Show this help\n"
                      << "\nExamples:\n"
                      << "  ymery                                   # Opens builtin file browser\n"
                      << "  ymery /home/user/layouts/app.yaml\n"
                      << "  ymery -m layouts/app.yaml\n"
                      << "  ymery app.yaml -p /path/to/shared/layouts\n";
            return 0;
        } else if (main_file.empty()) {
            // First positional arg is the main file
            main_file = arg;
        } else {
            // Treat as layout path - if it's a file, use its parent directory
            std::filesystem::path path(arg);
            std::error_code ec;
            if (std::filesystem::is_regular_file(path, ec) && !ec) {
                layout_paths.push_back(path.parent_path());
            } else {
                layout_paths.push_back(path);
            }
        }
    }

    // Determine main module
    std::string main_module;
    bool use_builtin = false;

    if (main_file.empty()) {
        // Use builtin filesystem browser
        main_module = "builtin";
        use_builtin = true;
#ifdef YMERY_WEB
        // Web build: add /demo to layout paths for virtual filesystem
        layout_paths.push_back("/demo");
        spdlog::debug("Web build: using builtin filesystem browser with /demo");
#else
        spdlog::debug("No layout specified, using builtin filesystem browser");
#endif
    }

    if (!use_builtin) {
#ifndef YMERY_WEB
        // Resolve main file path (relative to current directory if not absolute)
        if (!main_file.is_absolute()) {
            std::error_code ec;
            auto cwd = std::filesystem::current_path(ec);
            if (!ec) {
                main_file = cwd / main_file;
            }
        }

        // Check if main file exists
        std::error_code ec;
        if (!std::filesystem::exists(main_file, ec) || ec) {
            std::cerr << "Error: Main file not found: " << main_file << std::endl;
            return 1;
        }
#endif

        // Add main file's directory to layout_paths (at the front for highest priority)
        auto main_dir = main_file.parent_path();
        layout_paths.insert(layout_paths.begin(), main_dir);

        // Extract module name from file (strip .yaml extension)
        main_module = main_file.stem().string();

        spdlog::debug("Main file: {}", main_file.string());
        spdlog::debug("Main module: {}", main_module);
    }
    for (const auto& p : layout_paths) {
        spdlog::debug("Layout path: {}", p.string());
    }

#ifndef YMERY_WEB
    // Default plugin path - look for plugins directory next to executable (Linux only)
    if (plugin_paths.empty()) {
        std::error_code ec;
        auto exe_path = std::filesystem::canonical("/proc/self/exe", ec);
        if (!ec) {
            plugin_paths.push_back(exe_path.parent_path() / "plugins");
        }
    }
#endif

    // Create app config
    spdlog::debug("Creating app config");
    ymery::AppConfig config;
    config.layout_paths = layout_paths;
    config.plugin_paths = plugin_paths;
    config.main_module = main_module;
    config.window_title = "Ymery";
    spdlog::debug("App config created, calling App::create");

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
