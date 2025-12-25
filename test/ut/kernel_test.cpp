// Kernel plugin unit tests
#include <boost/ut.hpp>
#include "ymery/plugin_manager.hpp"
#include "ymery/dispatcher.hpp"
#include "ymery/types.hpp"

using namespace boost::ut;
using namespace ymery;

// Tests run from build directory, plugins are in ./plugins
static const char* PLUGINS_PATH = "plugins";

// Test helper to create plugin manager with test plugins path
auto create_test_plugin_manager() {
    return PluginManager::create(PLUGINS_PATH);
}

suite kernel_tests = [] {
    "kernel_get_children_names_root"_test = [] {
        // Setup: create plugin manager and kernel
        auto pm_res = create_test_plugin_manager();
        expect(pm_res.has_value()) << "PluginManager creation failed";
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value()) << "Dispatcher creation failed";
        auto disp = *disp_res;

        // Create kernel tree
        auto kernel_res = pm->create_tree("kernel", disp);
        expect(kernel_res.has_value()) << "Kernel creation failed: " << error_msg(kernel_res);
        auto kernel = *kernel_res;

        // Test: get_children_names at root should return ["providers", "settings", "windows"]
        auto children_res = kernel->get_children_names(DataPath("/"));
        expect(children_res.has_value()) << "get_children_names('/') failed";

        auto children = *children_res;
        expect(children.size() == 3_ul) << "Expected 3 children at root, got " << children.size();

        // Check that providers, settings, windows are present
        bool has_providers = std::find(children.begin(), children.end(), "providers") != children.end();
        bool has_settings = std::find(children.begin(), children.end(), "settings") != children.end();
        bool has_windows = std::find(children.begin(), children.end(), "windows") != children.end();

        expect(has_providers) << "Missing 'providers' in root children";
        expect(has_settings) << "Missing 'settings' in root children";
        expect(has_windows) << "Missing 'windows' in root children";
    };

    "kernel_get_children_names_providers"_test = [] {
        auto pm_res = create_test_plugin_manager();
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto kernel_res = pm->create_tree("kernel", disp);
        expect(kernel_res.has_value()) << "Kernel creation failed: " << error_msg(kernel_res);
        auto kernel = *kernel_res;

        // Test: get_children_names at /providers should return available providers
        auto children_res = kernel->get_children_names(DataPath("/providers"));
        expect(children_res.has_value()) << "get_children_names('/providers') failed: " << error_msg(children_res);

        auto children = *children_res;
        expect(children.size() > 0_ul) << "Expected at least 1 provider, got 0";

        // Should have waveform provider at minimum
        bool has_waveform = std::find(children.begin(), children.end(), "waveform") != children.end();
        expect(has_waveform) << "Missing 'waveform' in providers list. Got: " << [&]{
            std::string s;
            for(const auto& c : children) s += c + ", ";
            return s;
        }();
    };

    "kernel_get_children_names_waveform_available"_test = [] {
        auto pm_res = create_test_plugin_manager();
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto kernel_res = pm->create_tree("kernel", disp);
        expect(kernel_res.has_value());
        auto kernel = *kernel_res;

        // Test: /providers/waveform should have "available" and "opened"
        auto children_res = kernel->get_children_names(DataPath("/providers/waveform"));
        expect(children_res.has_value()) << "get_children_names('/providers/waveform') failed: " << error_msg(children_res);

        auto children = *children_res;
        bool has_available = std::find(children.begin(), children.end(), "available") != children.end();
        bool has_opened = std::find(children.begin(), children.end(), "opened") != children.end();

        expect(has_available) << "Missing 'available' in /providers/waveform";
        expect(has_opened) << "Missing 'opened' in /providers/waveform";
    };

    "kernel_get_children_names_waveform_available_types"_test = [] {
        auto pm_res = create_test_plugin_manager();
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto kernel_res = pm->create_tree("kernel", disp);
        expect(kernel_res.has_value());
        auto kernel = *kernel_res;

        // Test: /providers/waveform/available should have sine, square, triangle
        auto children_res = kernel->get_children_names(DataPath("/providers/waveform/available"));
        expect(children_res.has_value()) << "get_children_names failed: " << error_msg(children_res);

        auto children = *children_res;
        bool has_sine = std::find(children.begin(), children.end(), "sine") != children.end();
        bool has_square = std::find(children.begin(), children.end(), "square") != children.end();
        bool has_triangle = std::find(children.begin(), children.end(), "triangle") != children.end();

        expect(has_sine) << "Missing 'sine' in /providers/waveform/available";
        expect(has_square) << "Missing 'square' in /providers/waveform/available";
        expect(has_triangle) << "Missing 'triangle' in /providers/waveform/available";
    };

    "kernel_get_metadata_root"_test = [] {
        auto pm_res = create_test_plugin_manager();
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto kernel_res = pm->create_tree("kernel", disp);
        expect(kernel_res.has_value());
        auto kernel = *kernel_res;

        // Test: metadata at root should have name, label, type
        auto meta_res = kernel->get_metadata(DataPath("/"));
        expect(meta_res.has_value()) << "get_metadata('/') failed";

        auto meta = *meta_res;
        expect(meta.find("name") != meta.end()) << "Missing 'name' in root metadata";
        expect(meta.find("type") != meta.end()) << "Missing 'type' in root metadata";
    };

    "kernel_get_buffer_from_opened_waveform"_test = [] {
        auto pm_res = create_test_plugin_manager();
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto kernel_res = pm->create_tree("kernel", disp);
        expect(kernel_res.has_value());
        auto kernel = *kernel_res;

        // Test: /providers/waveform/opened/sine/0/buffer should return a buffer
        // (waveform auto-starts devices in create())
        auto buffer_res = kernel->get(DataPath("/providers/waveform/opened/sine/0/buffer"));
        expect(buffer_res.has_value()) << "get('/providers/waveform/opened/sine/0/buffer') failed: " << error_msg(buffer_res);

        auto buffer = *buffer_res;
        expect(buffer.has_value()) << "Buffer value is empty";
    };
};

int main() {
    return 0;
}
