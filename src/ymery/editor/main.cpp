#include "editor_app.hpp"
#include <spdlog/spdlog.h>
#include <iostream>
#include <string>

void print_help() {
    std::cout << "Ymery Widget Editor\n\n"
              << "Usage: ymery-editor [options]\n\n"
              << "Options:\n"
              << "  -w, --width <width>    Window width (default: 1280)\n"
              << "  -h, --height <height>  Window height (default: 720)\n"
              << "  --help                 Show this help message\n";
}

int main(int argc, char* argv[]) {
    ymery::editor::EditorConfig config;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help") {
            print_help();
            return 0;
        }
        else if ((arg == "-w" || arg == "--width") && i + 1 < argc) {
            config.window_width = std::stoi(argv[++i]);
        }
        else if ((arg == "-h" || arg == "--height") && i + 1 < argc) {
            config.window_height = std::stoi(argv[++i]);
        }
    }

    spdlog::set_level(spdlog::level::info);
    spdlog::info("Starting Ymery Widget Editor");

    auto app = ymery::editor::EditorApp::create(config);
    if (!app) {
        spdlog::error("Failed to create editor application");
        return 1;
    }

    app->run();

    spdlog::info("Ymery Widget Editor exiting");
    return 0;
}
