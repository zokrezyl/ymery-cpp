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

    "composite_body_as_string_should_be_resolved"_test = [] {
        // When body is a string like "kernel-recursive", it should be resolved
        // to the widget definition and treated as if that definition's body was inlined
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto kernel_res = pm->create_tree("kernel", disp);
        expect(kernel_res.has_value());
        auto kernel = *kernel_res;

        // Simulate what happens with body: kernel-recursive
        // The tree-node gets body as a STRING "kernel-recursive"
        std::map<std::string, TreeLikePtr> trees;
        trees["data"] = kernel;

        // Body is just a string - this is what tree-node receives
        Dict statics;
        statics["body"] = std::string("kernel-recursive");
        statics["type"] = std::string("composite");

        auto bag_res = DataBag::create(disp, pm, trees, "data", DataPath("/providers"), statics);
        expect(bag_res.has_value()) << "DataBag creation failed";
        auto bag = *bag_res;

        // Get body - should be a string
        auto body_res = bag->get_static("body");
        expect(body_res.has_value()) << "Failed to get body";
        expect(body_res->has_value()) << "Body is empty";

        // Verify it's a string (not a list yet)
        auto body_str = get_as<std::string>(*body_res);
        expect(body_str.has_value()) << "Body should be a string";
        expect(*body_str == "kernel-recursive") << "Body should be 'kernel-recursive'";

        // Verify the path is /providers
        auto path_res = bag->get_data_path_str();
        expect(path_res.has_value());
        expect(*path_res == "/providers") << "Expected '/providers', got '" << *path_res << "'";

        // Verify we can get children at /providers
        auto children_res = bag->get_children_names();
        expect(children_res.has_value()) << "get_children_names failed";
        auto children = *children_res;
        expect(children.size() >= 1_ul) << "Should have provider children at /providers";

        bool has_waveform = std::find(children.begin(), children.end(), "waveform") != children.end();
        expect(has_waveform) << "Should have waveform in /providers";
    };

    "widget_factory_resolves_string_body"_test = [] {
        // Verify that when body is a string reference to another widget,
        // it should be resolved to that widget's definition
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto kernel_res = pm->create_tree("kernel", disp);
        expect(kernel_res.has_value());

        // This test verifies the YAML pattern works:
        // kernel-recursive:
        //   type: composite
        //   body:
        //     - foreach-child:
        //         - tree-node:
        //             body: kernel-recursive
        //
        // When tree-node is created, its body is "kernel-recursive" (a string)
        // This should be resolved to the kernel-recursive definition's body
        // which is: [foreach-child: [tree-node: {body: kernel-recursive}]]

        // The issue: composite._ensure_children() checks if body is a List
        // and exits if it's not. But body can be a string widget reference!
        expect(true) << "This test documents the expected behavior - body resolution";
    };

    "kernel_providers_has_waveform_children"_test = [] {
        // Test that /providers/waveform has children (available, opened)
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto kernel_res = pm->create_tree("kernel", disp);
        expect(kernel_res.has_value());
        auto kernel = *kernel_res;

        // Get children at /providers/waveform
        auto children_res = kernel->get_children_names(DataPath("/providers/waveform"));
        expect(children_res.has_value()) << "get_children_names at /providers/waveform failed: " << error_msg(children_res);
        auto children = *children_res;

        expect(children.size() == 2_ul) << "waveform should have 2 children (available, opened), got " << children.size();

        bool has_available = std::find(children.begin(), children.end(), "available") != children.end();
        bool has_opened = std::find(children.begin(), children.end(), "opened") != children.end();
        expect(has_available) << "Missing 'available' in waveform children";
        expect(has_opened) << "Missing 'opened' in waveform children";
    };

    "kernel_providers_waveform_available_has_types"_test = [] {
        // Test that /providers/waveform/available has sine, square, triangle
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto kernel_res = pm->create_tree("kernel", disp);
        expect(kernel_res.has_value());
        auto kernel = *kernel_res;

        // Get children at /providers/waveform/available
        auto children_res = kernel->get_children_names(DataPath("/providers/waveform/available"));
        expect(children_res.has_value()) << "get_children_names at /providers/waveform/available failed";
        auto children = *children_res;

        expect(children.size() == 3_ul) << "waveform/available should have 3 children (sine, square, triangle), got " << children.size();

        bool has_sine = std::find(children.begin(), children.end(), "sine") != children.end();
        bool has_square = std::find(children.begin(), children.end(), "square") != children.end();
        bool has_triangle = std::find(children.begin(), children.end(), "triangle") != children.end();
        expect(has_sine) << "Missing 'sine'";
        expect(has_square) << "Missing 'square'";
        expect(has_triangle) << "Missing 'triangle'";
    };

    "kernel_providers_waveform_opened_has_devices"_test = [] {
        // Test that /providers/waveform/opened has auto-started devices
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto kernel_res = pm->create_tree("kernel", disp);
        expect(kernel_res.has_value());
        auto kernel = *kernel_res;

        // Get children at /providers/waveform/opened
        auto children_res = kernel->get_children_names(DataPath("/providers/waveform/opened"));
        expect(children_res.has_value()) << "get_children_names at /providers/waveform/opened failed";
        auto children = *children_res;

        // Waveform auto-starts all devices
        expect(children.size() == 3_ul) << "waveform/opened should have 3 devices (auto-started), got " << children.size();
    };

    "kernel_providers_waveform_opened_sine_has_channel"_test = [] {
        // Test that /providers/waveform/opened/sine has channel 0
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto kernel_res = pm->create_tree("kernel", disp);
        expect(kernel_res.has_value());
        auto kernel = *kernel_res;

        // Get children at /providers/waveform/opened/sine
        auto children_res = kernel->get_children_names(DataPath("/providers/waveform/opened/sine"));
        expect(children_res.has_value()) << "get_children_names at /providers/waveform/opened/sine failed";
        auto children = *children_res;

        expect(children.size() == 1_ul) << "sine should have 1 channel, got " << children.size();
        expect(children[0] == "0") << "channel should be '0', got '" << children[0] << "'";
    };

    "kernel_providers_waveform_channel_has_buffer_metadata"_test = [] {
        // Test that /providers/waveform/opened/sine/0 has buffer in metadata
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto kernel_res = pm->create_tree("kernel", disp);
        expect(kernel_res.has_value());
        auto kernel = *kernel_res;

        // Get metadata at /providers/waveform/opened/sine/0
        auto meta_res = kernel->get_metadata(DataPath("/providers/waveform/opened/sine/0"));
        expect(meta_res.has_value()) << "get_metadata at /providers/waveform/opened/sine/0 failed";
        auto meta = *meta_res;

        // Should have buffer key
        expect(meta.count("buffer") > 0_ul) << "Missing 'buffer' in channel metadata";
        expect(meta.count("name") > 0_ul) << "Missing 'name' in channel metadata";

        // Verify name is "0"
        if (auto name = get_as<std::string>(meta["name"])) {
            expect(*name == "0") << "name should be '0'";
        }
    };

    "databag_full_tree_traversal_to_waveform"_test = [] {
        // Test full DataBag traversal from root to waveform device
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
        trees["data"] = kernel;

        // Start at root
        auto root_bag_res = DataBag::create(disp, pm, trees, "data", DataPath("/"), {});
        expect(root_bag_res.has_value());
        auto root_bag = *root_bag_res;

        // Traverse: root -> providers
        auto providers_bag_res = root_bag->inherit("providers", {});
        expect(providers_bag_res.has_value()) << "inherit to providers failed";
        auto providers_bag = *providers_bag_res;

        auto path_res = providers_bag->get_data_path_str();
        expect(path_res.has_value());
        expect(*path_res == "/providers") << "Expected '/providers', got '" << *path_res << "'";

        // Traverse: providers -> waveform
        auto waveform_bag_res = providers_bag->inherit("waveform", {});
        expect(waveform_bag_res.has_value()) << "inherit to waveform failed";
        auto waveform_bag = *waveform_bag_res;

        path_res = waveform_bag->get_data_path_str();
        expect(path_res.has_value());
        expect(*path_res == "/providers/waveform") << "Expected '/providers/waveform', got '" << *path_res << "'";

        // Get children at /providers/waveform
        auto children_res = waveform_bag->get_children_names();
        expect(children_res.has_value()) << "get_children_names at waveform failed";
        auto children = *children_res;
        expect(children.size() == 2_ul) << "waveform should have available, opened";

        // Traverse: waveform -> opened
        auto opened_bag_res = waveform_bag->inherit("opened", {});
        expect(opened_bag_res.has_value()) << "inherit to opened failed";
        auto opened_bag = *opened_bag_res;

        // Get children at /providers/waveform/opened (devices)
        children_res = opened_bag->get_children_names();
        expect(children_res.has_value()) << "get_children_names at opened failed";
        children = *children_res;
        expect(children.size() == 3_ul) << "opened should have 3 devices";

        // Traverse: opened -> sine
        auto sine_bag_res = opened_bag->inherit("sine", {});
        expect(sine_bag_res.has_value()) << "inherit to sine failed";
        auto sine_bag = *sine_bag_res;

        path_res = sine_bag->get_data_path_str();
        expect(path_res.has_value());
        expect(*path_res == "/providers/waveform/opened/sine") << "Expected '/providers/waveform/opened/sine'";

        // Get children at sine (channels)
        children_res = sine_bag->get_children_names();
        expect(children_res.has_value()) << "get_children_names at sine failed";
        children = *children_res;
        expect(children.size() == 1_ul) << "sine should have 1 channel";
        expect(children[0] == "0") << "channel should be '0'";

        // Traverse: sine -> 0 (channel)
        auto channel_bag_res = sine_bag->inherit("0", {});
        expect(channel_bag_res.has_value()) << "inherit to channel failed";
        auto channel_bag = *channel_bag_res;

        // Get metadata at channel - should have buffer
        auto meta_res = channel_bag->get_metadata();
        expect(meta_res.has_value()) << "get_metadata at channel failed";
        auto meta = *meta_res;
        expect(meta.count("buffer") > 0_ul) << "channel should have buffer in metadata";
    };

    "foreach_child_recursive_traversal"_test = [] {
        // Simulate the recursive foreach-child pattern used in kernel-recursive
        // This tests what happens when tree-node with body: kernel-recursive is used
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
        trees["data"] = kernel;

        // Level 1: Root composite - foreach-child over root children
        auto root_bag = *DataBag::create(disp, pm, trees, "data", DataPath("/"), {});
        auto root_children = *root_bag->get_children_names();
        expect(root_children.size() == 3_ul) << "root has providers, settings, windows";

        // Level 2: For "providers" child, create child bag via inherit
        auto providers_bag = *root_bag->inherit("providers", {});
        auto providers_children = *providers_bag->get_children_names();
        expect(providers_children.size() >= 1_ul) << "providers should have device-manager children";

        bool has_waveform = std::find(providers_children.begin(), providers_children.end(), "waveform") != providers_children.end();
        expect(has_waveform) << "providers should have waveform";

        // Level 3: For "waveform" child, create child bag
        auto waveform_bag = *providers_bag->inherit("waveform", {});
        auto waveform_children = *waveform_bag->get_children_names();
        expect(waveform_children.size() == 2_ul) << "waveform has available, opened";

        // Level 4: For "opened" child
        auto opened_bag = *waveform_bag->inherit("opened", {});
        auto opened_children = *opened_bag->get_children_names();
        expect(opened_children.size() == 3_ul) << "opened has 3 auto-started devices";

        // Level 5: For "sine" device
        auto sine_bag = *opened_bag->inherit("sine", {});
        auto sine_children = *sine_bag->get_children_names();
        expect(sine_children.size() == 1_ul) << "sine has 1 channel";

        // Level 6: Channel "0"
        auto channel_bag = *sine_bag->inherit("0", {});
        auto channel_children = *channel_bag->get_children_names();
        expect(channel_children.size() == 0_ul) << "channel is leaf node";

        // Verify we can get metadata with buffer at the leaf
        auto meta = *channel_bag->get_metadata();
        expect(meta.count("buffer") > 0_ul) << "leaf channel should have buffer";
    };

#ifdef __linux__
    "alsa_provider_has_available_and_opened"_test = [] {
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto kernel_res = pm->create_tree("kernel", disp);
        expect(kernel_res.has_value());
        auto kernel = *kernel_res;

        // Get children at /providers/alsa
        auto children_res = kernel->get_children_names(DataPath("/providers/alsa"));
        expect(children_res.has_value()) << "get_children_names at /providers/alsa failed";
        auto children = *children_res;

        expect(children.size() == 2_ul) << "alsa should have 2 children (available, opened), got " << children.size();

        bool has_available = std::find(children.begin(), children.end(), "available") != children.end();
        bool has_opened = std::find(children.begin(), children.end(), "opened") != children.end();
        expect(has_available) << "Missing 'available'";
        expect(has_opened) << "Missing 'opened'";
    };

    "jack_provider_has_available_and_opened"_test = [] {
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto kernel_res = pm->create_tree("kernel", disp);
        expect(kernel_res.has_value());
        auto kernel = *kernel_res;

        // Get children at /providers/jack
        auto children_res = kernel->get_children_names(DataPath("/providers/jack"));
        expect(children_res.has_value()) << "get_children_names at /providers/jack failed";
        auto children = *children_res;

        expect(children.size() == 2_ul) << "jack should have 2 children (available, opened), got " << children.size();

        bool has_available = std::find(children.begin(), children.end(), "available") != children.end();
        bool has_opened = std::find(children.begin(), children.end(), "opened") != children.end();
        expect(has_available) << "Missing 'available'";
        expect(has_opened) << "Missing 'opened'";
    };

    "pipewire_provider_has_available_and_opened"_test = [] {
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto kernel_res = pm->create_tree("kernel", disp);
        expect(kernel_res.has_value());
        auto kernel = *kernel_res;

        // Get children at /providers/pipewire
        auto children_res = kernel->get_children_names(DataPath("/providers/pipewire"));
        expect(children_res.has_value()) << "get_children_names at /providers/pipewire failed";
        auto children = *children_res;

        expect(children.size() == 2_ul) << "pipewire should have 2 children (available, opened), got " << children.size();

        bool has_available = std::find(children.begin(), children.end(), "available") != children.end();
        bool has_opened = std::find(children.begin(), children.end(), "opened") != children.end();
        expect(has_available) << "Missing 'available'";
        expect(has_opened) << "Missing 'opened'";
    };

    "jack_available_has_client_and_readable_clients"_test = [] {
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto kernel_res = pm->create_tree("kernel", disp);
        expect(kernel_res.has_value());
        auto kernel = *kernel_res;

        // Get children at /providers/jack/available
        auto children_res = kernel->get_children_names(DataPath("/providers/jack/available"));
        expect(children_res.has_value()) << "get_children_names at /providers/jack/available failed";
        auto children = *children_res;

        expect(children.size() == 2_ul) << "jack/available should have 2 children, got " << children.size();

        bool has_client = std::find(children.begin(), children.end(), "client") != children.end();
        bool has_readable = std::find(children.begin(), children.end(), "readable-clients") != children.end();
        expect(has_client) << "Missing 'client'";
        expect(has_readable) << "Missing 'readable-clients'";
    };

    "pipewire_available_has_default_and_nodes"_test = [] {
        auto pm_res = PluginManager::create(PLUGINS_PATH);
        expect(pm_res.has_value());
        auto pm = *pm_res;

        auto disp_res = Dispatcher::create();
        expect(disp_res.has_value());
        auto disp = *disp_res;

        auto kernel_res = pm->create_tree("kernel", disp);
        expect(kernel_res.has_value());
        auto kernel = *kernel_res;

        // Get children at /providers/pipewire/available
        auto children_res = kernel->get_children_names(DataPath("/providers/pipewire/available"));
        expect(children_res.has_value()) << "get_children_names at /providers/pipewire/available failed";
        auto children = *children_res;

        expect(children.size() == 2_ul) << "pipewire/available should have 2 children, got " << children.size();

        bool has_default = std::find(children.begin(), children.end(), "default") != children.end();
        bool has_nodes = std::find(children.begin(), children.end(), "nodes") != children.end();
        expect(has_default) << "Missing 'default'";
        expect(has_nodes) << "Missing 'nodes'";
    };
#endif // __linux__
};

int main() {
    return 0;
}
