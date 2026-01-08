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

        // New consolidated plugin naming: plugin.widget
        expect(pm->has_widget("imgui.text")) << "Missing 'imgui.text' widget";
        expect(pm->has_widget("imgui.button")) << "Missing 'imgui.button' widget";
        expect(pm->has_widget("implot.plot")) << "Missing 'implot.plot' widget";
        expect(pm->has_widget("implot.implot-layer")) << "Missing 'implot.implot-layer' widget";
    };

    "plugin_manager_has_tree"_test = [] {
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        expect(pm->has_tree("kernel")) << "Missing 'kernel' tree plugin";
        expect(pm->has_tree("waveform")) << "Missing 'waveform' tree plugin";
        expect(pm->has_tree("simple-data-tree")) << "Missing 'simple-data-tree' plugin";
    };

    "plugin_manager_lazy_loading"_test = [] {
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        // With lazy loading, plugins are loaded on first use
        // Test that kernel plugin can be loaded lazily
        auto kernel_res = pm->create_tree("kernel", disp);
        expect(kernel_res.has_value()) << "create_tree('kernel') failed: " << error_msg(kernel_res);

        // After loading kernel, it should appear in tree-like category
        auto children_res = pm->get_children_names(DataPath("/tree-like"));
        expect(children_res.has_value()) << "get_children_names('/tree-like') failed: " << error_msg(children_res);

        auto children = *children_res;
        bool has_kernel = std::find(children.begin(), children.end(), "kernel") != children.end();
        expect(has_kernel) << "kernel should be in tree-like list after loading";
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
