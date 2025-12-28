// Plugin manager unit tests
#include <boost/ut.hpp>
#include "ymery/plugin_manager.hpp"
#include "ymery/dispatcher.hpp"
#include "ymery/types.hpp"

using namespace boost::ut;
using namespace ymery;

// Tests run from build directory, plugins are in ./plugins
static const char* PLUGINS_PATH = "plugins";

suite plugin_manager_tests = [] {
    "plugin_manager_create"_test = [] {
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value()) << "PluginManager creation failed: " << error_msg(pm_res);
    };

    "plugin_manager_has_widget"_test = [] {
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        expect(pm->has_widget("text")) << "Missing 'text' widget";
        expect(pm->has_widget("button")) << "Missing 'button' widget";
        expect(pm->has_widget("implot")) << "Missing 'implot' widget";
        expect(pm->has_widget("implot-layer")) << "Missing 'implot-layer' widget";
    };

    "plugin_manager_has_tree"_test = [] {
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        expect(pm->has_tree("kernel")) << "Missing 'kernel' tree plugin";
        expect(pm->has_tree("waveform")) << "Missing 'waveform' tree plugin";
        expect(pm->has_tree("simple-data-tree")) << "Missing 'simple-data-tree' plugin";
    };

    "plugin_manager_get_children_names_device_manager"_test = [] {
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        // Should list device-manager plugins at /device-manager
        auto children_res = pm->get_children_names(DataPath("/device-manager"));
        expect(children_res.has_value()) << "get_children_names('/device-manager') failed: " << error_msg(children_res);

        auto children = *children_res;
        expect(children.size() > 0_ul) << "Expected device-manager plugins";

        // waveform and filesystem are always built
        bool has_waveform = std::find(children.begin(), children.end(), "waveform") != children.end();
        bool has_filesystem = std::find(children.begin(), children.end(), "filesystem") != children.end();

        expect(has_waveform) << "Missing 'waveform' in device-manager list";
        expect(has_filesystem) << "Missing 'filesystem' in device-manager list";

        // kernel should NOT be in device-manager list (it's in tree-like)
        bool has_kernel = std::find(children.begin(), children.end(), "kernel") != children.end();
        expect(!has_kernel) << "kernel should NOT be in device-manager list";
    };

    "plugin_manager_create_tree_kernel"_test = [] {
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto kernel_res = pm->create_tree("kernel", disp);
        expect(kernel_res.has_value()) << "create_tree('kernel') failed: " << error_msg(kernel_res);
    };

    "plugin_manager_create_tree_waveform"_test = [] {
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto waveform_res = pm->create_tree("waveform", disp);
        expect(waveform_res.has_value()) << "create_tree('waveform') failed: " << error_msg(waveform_res);
    };
};

int main() {
    return 0;
}
