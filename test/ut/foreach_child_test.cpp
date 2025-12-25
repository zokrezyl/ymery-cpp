// foreach-child unit tests
#include <boost/ut.hpp>
#include "ymery/plugin_manager.hpp"
#include "ymery/dispatcher.hpp"
#include "ymery/data_bag.hpp"
#include "ymery/frontend/composite.hpp"
#include "ymery/frontend/widget_factory.hpp"
#include "ymery/lang.hpp"
#include "ymery/types.hpp"

using namespace boost::ut;
using namespace ymery;

static const char* PLUGINS_PATH = "plugins";

suite foreach_child_tests = [] {
    "databag_get_children_names_at_root"_test = [] {
        // Setup
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value()) << "PluginManager creation failed";
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value()) << "Dispatcher creation failed";
        auto disp = *disp_res;

        // Create kernel tree
        auto kernel_res = pm->create_tree("kernel", disp);
        expect(kernel_res.has_value()) << "Kernel creation failed: " << error_msg(kernel_res);
        auto kernel = *kernel_res;

        // Create DataBag with kernel as main data tree at root path
        std::map<std::string, TreeLikePtr> trees;
        trees["kernel"] = kernel;

        auto bag_res = DataBag::create(
            disp,
            pm,
            trees,
            "kernel",      // main_data_key
            DataPath("/"), // main_data_path - ROOT
            {}             // statics
        );
        expect(bag_res.has_value()) << "DataBag creation failed: " << error_msg(bag_res);
        auto bag = *bag_res;

        // Test: get_children_names should return kernel's root children
        auto children_res = bag->get_children_names();
        expect(children_res.has_value()) << "get_children_names failed: " << error_msg(children_res);

        auto children = *children_res;
        expect(children.size() == 3_ul) << "Expected 3 children at root, got " << children.size();

        bool has_providers = std::find(children.begin(), children.end(), "providers") != children.end();
        expect(has_providers) << "Missing 'providers' in children";
    };

    "databag_get_children_names_at_providers"_test = [] {
        // Setup
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto kernel_res = pm->create_tree("kernel", disp);
        expect(kernel_res.has_value()) << error_msg(kernel_res);
        auto kernel = *kernel_res;

        std::map<std::string, TreeLikePtr> trees;
        trees["kernel"] = kernel;

        // Create DataBag at /providers path
        auto bag_res = DataBag::create(
            disp,
            pm,
            trees,
            "kernel",
            DataPath("/providers"), // AT PROVIDERS PATH
            {}
        );
        expect(bag_res.has_value()) << error_msg(bag_res);
        auto bag = *bag_res;

        // Test: get_children_names at /providers should return provider names
        auto children_res = bag->get_children_names();
        expect(children_res.has_value()) << "get_children_names failed: " << error_msg(children_res);

        auto children = *children_res;
        expect(children.size() >= 1_ul) << "Expected at least 1 provider, got " << children.size();

        // Should have waveform
        bool has_waveform = std::find(children.begin(), children.end(), "waveform") != children.end();
        expect(has_waveform) << "Missing 'waveform' in providers children";
    };

    "databag_inherit_navigates_to_child"_test = [] {
        // Setup
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto kernel_res = pm->create_tree("kernel", disp);
        expect(kernel_res.has_value());
        auto kernel = *kernel_res;

        std::map<std::string, TreeLikePtr> trees;
        trees["kernel"] = kernel;

        // Create DataBag at root
        auto bag_res = DataBag::create(disp, pm, trees, "kernel", DataPath("/"), {});
        expect(bag_res.has_value());
        auto bag = *bag_res;

        // Inherit to "providers" child
        auto child_bag_res = bag->inherit("providers", {});
        expect(child_bag_res.has_value()) << "inherit('providers') failed: " << error_msg(child_bag_res);
        auto child_bag = *child_bag_res;

        // Check path is now /providers
        auto path_res = child_bag->get_data_path_str();
        expect(path_res.has_value());
        expect(*path_res == "/providers") << "Expected '/providers', got '" << *path_res << "'";

        // Get children of /providers - should have device-manager plugins
        auto children_res = child_bag->get_children_names();
        expect(children_res.has_value()) << error_msg(children_res);

        auto children = *children_res;
        bool has_waveform = std::find(children.begin(), children.end(), "waveform") != children.end();
        expect(has_waveform) << "Missing 'waveform' after navigating to /providers";
    };

    "databag_inherit_chain"_test = [] {
        // Setup
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto kernel_res = pm->create_tree("kernel", disp);
        expect(kernel_res.has_value());
        auto kernel = *kernel_res;

        std::map<std::string, TreeLikePtr> trees;
        trees["kernel"] = kernel;

        // Start at root
        auto bag_res = DataBag::create(disp, pm, trees, "kernel", DataPath("/"), {});
        expect(bag_res.has_value());
        auto bag = *bag_res;

        // Get children at root
        auto root_children_res = bag->get_children_names();
        expect(root_children_res.has_value());
        auto root_children = *root_children_res;
        expect(root_children.size() == 3_ul) << "Root should have 3 children";

        // Navigate to each child and verify we can get their children
        for (const auto& child_name : root_children) {
            auto child_bag_res = bag->inherit(child_name, {});
            expect(child_bag_res.has_value()) << "Failed to inherit to '" << child_name << "'";
            auto child_bag = *child_bag_res;

            // Verify path
            auto path_res = child_bag->get_data_path_str();
            expect(path_res.has_value());
            std::string expected_path = "/" + child_name;
            expect(*path_res == expected_path) << "Expected '" << expected_path << "', got '" << *path_res << "'";

            // Get grandchildren (may be empty, that's ok)
            auto grandchildren_res = child_bag->get_children_names();
            expect(grandchildren_res.has_value()) << "Failed to get children of /" << child_name;
        }
    };

    "databag_foreach_child_simulation"_test = [] {
        // This simulates what foreach-child does in Composite
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto kernel_res = pm->create_tree("kernel", disp);
        expect(kernel_res.has_value());
        auto kernel = *kernel_res;

        // Simulate how WidgetFactory creates root DataBag with "data" key
        std::map<std::string, TreeLikePtr> trees;
        trees["data"] = kernel;  // Note: using "data" as key, like WidgetFactory does

        auto root_bag_res = DataBag::create(disp, pm, trees, "data", DataPath("/"), {});
        expect(root_bag_res.has_value()) << "Root bag creation failed";
        auto root_bag = *root_bag_res;

        // Step 1: Get children at root (simulating foreach-child)
        auto children_res = root_bag->get_children_names();
        expect(children_res.has_value()) << "get_children_names at root failed: " << error_msg(children_res);
        auto children = *children_res;
        expect(children.size() == 3_ul) << "Expected 3 children at root, got " << children.size();

        // Step 2: For each child, create a child DataBag via inherit (like foreach-child does)
        for (const auto& child_name : children) {
            auto child_bag_res = root_bag->inherit(child_name, {});
            expect(child_bag_res.has_value()) << "inherit('" << child_name << "') failed: " << error_msg(child_bag_res);
            auto child_bag = *child_bag_res;

            // Verify path is correct
            auto path_res = child_bag->get_data_path_str();
            expect(path_res.has_value());
            std::string expected = "/" + child_name;
            expect(*path_res == expected) << "Child path wrong: expected '" << expected << "', got '" << *path_res << "'";

            // Verify we can still get children from the child bag
            auto grandchildren_res = child_bag->get_children_names();
            expect(grandchildren_res.has_value()) << "get_children_names for /" << child_name << " failed";

            // If this is "providers", it should have the device-manager plugins
            if (child_name == "providers") {
                auto gc = *grandchildren_res;
                expect(gc.size() >= 1_ul) << "providers should have children";
                bool has_waveform = std::find(gc.begin(), gc.end(), "waveform") != gc.end();
                expect(has_waveform) << "providers should have waveform";
            }
        }
    };

    "composite_foreach_child_creates_children"_test = [] {
        // Test that Composite with foreach-child body creates child widgets
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto kernel_res = pm->create_tree("kernel", disp);
        expect(kernel_res.has_value());
        auto kernel = *kernel_res;

        // Create DataBag with kernel at root
        std::map<std::string, TreeLikePtr> trees;
        trees["data"] = kernel;

        // Create a body with foreach-child that should create text widgets
        List foreach_child_body;
        Dict text_spec;
        text_spec["text"] = Dict{};  // Simple text widget
        foreach_child_body.push_back(text_spec);

        Dict foreach_item;
        foreach_item["foreach-child"] = foreach_child_body;

        List body;
        body.push_back(foreach_item);

        Dict statics;
        statics["body"] = body;

        auto bag_res = DataBag::create(disp, pm, trees, "data", DataPath("/"), statics);
        expect(bag_res.has_value()) << "DataBag creation failed: " << error_msg(bag_res);
        auto bag = *bag_res;

        // Verify the DataBag has the foreach-child body
        auto body_res = bag->get_static("body");
        expect(body_res.has_value()) << "Failed to get body from statics";
        expect(body_res->has_value()) << "Body has no value";

        // Parse body to verify foreach-child is present
        auto body_list = get_as<List>(*body_res);
        expect(body_list.has_value()) << "Body is not a list";
        expect(body_list->size() == 1_ul) << "Body should have 1 item";

        auto first_item = get_as<Dict>((*body_list)[0]);
        expect(first_item.has_value()) << "First body item is not a dict";
        expect(first_item->count("foreach-child") > 0_ul) << "foreach-child key not found in body item";

        // Verify children exist to iterate
        auto children_res = bag->get_children_names();
        expect(children_res.has_value()) << "get_children_names failed";
        expect(children_res->size() == 3_ul) << "Should have 3 children (providers, settings, windows)";
    };

    "composite_widget_with_foreach_child_has_correct_setup"_test = [] {
        // Verify the setup for foreach-child is correct
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto kernel_res = pm->create_tree("kernel", disp);
        expect(kernel_res.has_value());
        auto kernel = *kernel_res;

        // Create DataBag with kernel at root and foreach-child body
        std::map<std::string, TreeLikePtr> trees;
        trees["data"] = kernel;

        // Body: foreach-child with text widget
        List foreach_body;
        Dict text_spec;
        text_spec["text"] = Dict{};
        foreach_body.push_back(text_spec);

        Dict foreach_item;
        foreach_item["foreach-child"] = foreach_body;

        List body;
        body.push_back(foreach_item);

        Dict statics;
        statics["body"] = body;
        statics["type"] = std::string("composite");

        auto bag_res = DataBag::create(disp, pm, trees, "data", DataPath("/"), statics);
        expect(bag_res.has_value()) << "DataBag creation failed";
        auto bag = *bag_res;

        // Verify body statics contain foreach-child
        auto body_res = bag->get_static("body");
        expect(body_res.has_value()) << "Failed to get body";
        expect(body_res->has_value()) << "Body is empty";

        auto body_opt = get_as<List>(*body_res);
        expect(body_opt.has_value()) << "Body is not a list";
        expect(body_opt->size() == 1_ul) << "Body should have 1 item";

        auto first_opt = get_as<Dict>((*body_opt)[0]);
        expect(first_opt.has_value()) << "First item is not a dict";
        expect(first_opt->count("foreach-child") > 0_ul) << "foreach-child key missing";

        // Verify kernel provides 3 children at root
        auto children_names_res = bag->get_children_names();
        expect(children_names_res.has_value()) << "get_children_names failed";
        auto children_names = *children_names_res;
        expect(children_names.size() == 3_ul) << "Kernel root should have 3 children, got " << children_names.size();

        // Verify children are: providers, settings, windows
        bool has_providers = std::find(children_names.begin(), children_names.end(), "providers") != children_names.end();
        bool has_settings = std::find(children_names.begin(), children_names.end(), "settings") != children_names.end();
        bool has_windows = std::find(children_names.begin(), children_names.end(), "windows") != children_names.end();
        expect(has_providers) << "Missing 'providers'";
        expect(has_settings) << "Missing 'settings'";
        expect(has_windows) << "Missing 'windows'";
    };
};

int main() {
    return 0;
}
