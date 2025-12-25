#include "ymery/app.hpp"
#include <iostream>
#include <filesystem>

int main() {
    ymery::AppConfig config;
    config.layout_paths = {
        std::filesystem::path(__FILE__).parent_path() / "layout"
    };
    config.main_module = "app";
    config.window_title = "Simple Example";

    auto app_res = ymery::App::create(config);
    if (!app_res) {
        std::cerr << "Failed to create app: " << app_res.error_message() << std::endl;
        return 1;
    }

    auto app = app_res.unwrapped();
    auto run_res = app->run();
    if (!run_res) {
        std::cerr << "App error: " << run_res.error_message() << std::endl;
    }

    app->dispose();
    return 0;
}
