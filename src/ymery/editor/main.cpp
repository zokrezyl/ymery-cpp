#include "editor_app.hpp"
#include <ytrace/ytrace.hpp>
#include <iostream>
#include <string>
#include <filesystem>

void print_help() {
    std::cout << "Ymery Widget Editor\n\n"
              << "Usage: ymery-editor [options]\n\n"
              << "Options:\n"
              << "  -p, --plugins <path>   Path to plugins directory\n"
              << "  -w, --width <width>    Window width (default: 1280)\n"
              << "  -h, --height <height>  Window height (default: 720)\n"
              << "  --help                 Show this help message\n";
}

int main(int argc, char* argv[]) {
    ymery::editor::EditorConfig config;

    // Default plugins path: relative to executable
    std::filesystem::path exe_path = std::filesystem::canonical("/proc/self/exe");
    config.plugins_path = (exe_path.parent_path() / "plugins").string();

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help") {
            print_help();
            return 0;
        }
        else if ((arg == "-p" || arg == "--plugins") && i + 1 < argc) {
            config.plugins_path = argv[++i];
        }
        else if ((arg == "-w" || arg == "--width") && i + 1 < argc) {
            config.window_width = std::stoi(argv[++i]);
        }
        else if ((arg == "-h" || arg == "--height") && i + 1 < argc) {
            config.window_height = std::stoi(argv[++i]);
        }
    }

    yinfo("Starting Ymery Widget Editor");

    auto app = ymery::editor::EditorApp::create(config);
    if (!app) {
        ywarn("Failed to create editor application");
        return 1;
    }

    app->run();

    yinfo("Ymery Widget Editor exiting");
    return 0;
}
