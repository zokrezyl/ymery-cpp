// Static plugin registry for web builds (no dlopen available)
// This file is only compiled for YMERY_WEB builds
//
// Plugins are compiled separately with unique symbol names via CMake

#include "static_plugins.hpp"
#include <vector>

#ifdef YMERY_WEB

namespace ymery {

// Forward declarations of plugin functions with unique names
// These are defined in the plugin object files compiled with -Dplugin_export_name=...

// Frontend widgets
extern "C" {
    const char* text_plugin_name();
    const char* text_plugin_type();
    Result<WidgetPtr> text_plugin_create(std::shared_ptr<WidgetFactory>, std::shared_ptr<Dispatcher>, const std::string&, std::shared_ptr<DataBag>);

    const char* button_plugin_name();
    const char* button_plugin_type();
    Result<WidgetPtr> button_plugin_create(std::shared_ptr<WidgetFactory>, std::shared_ptr<Dispatcher>, const std::string&, std::shared_ptr<DataBag>);

    const char* window_plugin_name();
    const char* window_plugin_type();
    Result<WidgetPtr> window_plugin_create(std::shared_ptr<WidgetFactory>, std::shared_ptr<Dispatcher>, const std::string&, std::shared_ptr<DataBag>);

    const char* column_plugin_name();
    const char* column_plugin_type();
    Result<WidgetPtr> column_plugin_create(std::shared_ptr<WidgetFactory>, std::shared_ptr<Dispatcher>, const std::string&, std::shared_ptr<DataBag>);

    const char* row_plugin_name();
    const char* row_plugin_type();
    Result<WidgetPtr> row_plugin_create(std::shared_ptr<WidgetFactory>, std::shared_ptr<Dispatcher>, const std::string&, std::shared_ptr<DataBag>);

    const char* imgui_main_window_plugin_name();
    const char* imgui_main_window_plugin_type();
    Result<WidgetPtr> imgui_main_window_plugin_create(std::shared_ptr<WidgetFactory>, std::shared_ptr<Dispatcher>, const std::string&, std::shared_ptr<DataBag>);

    const char* slider_int_plugin_name();
    const char* slider_int_plugin_type();
    Result<WidgetPtr> slider_int_plugin_create(std::shared_ptr<WidgetFactory>, std::shared_ptr<Dispatcher>, const std::string&, std::shared_ptr<DataBag>);

    const char* slider_float_plugin_name();
    const char* slider_float_plugin_type();
    Result<WidgetPtr> slider_float_plugin_create(std::shared_ptr<WidgetFactory>, std::shared_ptr<Dispatcher>, const std::string&, std::shared_ptr<DataBag>);

    const char* checkbox_plugin_name();
    const char* checkbox_plugin_type();
    Result<WidgetPtr> checkbox_plugin_create(std::shared_ptr<WidgetFactory>, std::shared_ptr<Dispatcher>, const std::string&, std::shared_ptr<DataBag>);

    const char* input_text_plugin_name();
    const char* input_text_plugin_type();
    Result<WidgetPtr> input_text_plugin_create(std::shared_ptr<WidgetFactory>, std::shared_ptr<Dispatcher>, const std::string&, std::shared_ptr<DataBag>);

    const char* combo_plugin_name();
    const char* combo_plugin_type();
    Result<WidgetPtr> combo_plugin_create(std::shared_ptr<WidgetFactory>, std::shared_ptr<Dispatcher>, const std::string&, std::shared_ptr<DataBag>);

    const char* separator_plugin_name();
    const char* separator_plugin_type();
    Result<WidgetPtr> separator_plugin_create(std::shared_ptr<WidgetFactory>, std::shared_ptr<Dispatcher>, const std::string&, std::shared_ptr<DataBag>);

    const char* spacing_plugin_name();
    const char* spacing_plugin_type();
    Result<WidgetPtr> spacing_plugin_create(std::shared_ptr<WidgetFactory>, std::shared_ptr<Dispatcher>, const std::string&, std::shared_ptr<DataBag>);

    const char* tree_node_plugin_name();
    const char* tree_node_plugin_type();
    Result<WidgetPtr> tree_node_plugin_create(std::shared_ptr<WidgetFactory>, std::shared_ptr<Dispatcher>, const std::string&, std::shared_ptr<DataBag>);

    const char* group_plugin_name();
    const char* group_plugin_type();
    Result<WidgetPtr> group_plugin_create(std::shared_ptr<WidgetFactory>, std::shared_ptr<Dispatcher>, const std::string&, std::shared_ptr<DataBag>);

    // Backend tree-like plugins
    const char* simple_data_tree_plugin_name();
    const char* simple_data_tree_plugin_type();
    Result<TreeLikePtr> simple_data_tree_plugin_create(std::shared_ptr<Dispatcher>, std::shared_ptr<PluginManager>);

    const char* data_tree_plugin_name();
    const char* data_tree_plugin_type();
    Result<TreeLikePtr> data_tree_plugin_create(std::shared_ptr<Dispatcher>, std::shared_ptr<PluginManager>);
}

const std::vector<StaticPlugin>& get_static_plugins() {
    static std::vector<StaticPlugin> plugins = {
        // Frontend widgets - minimal set for demos
        {text_plugin_name, text_plugin_type, reinterpret_cast<void*>(text_plugin_create)},
        {button_plugin_name, button_plugin_type, reinterpret_cast<void*>(button_plugin_create)},
        {window_plugin_name, window_plugin_type, reinterpret_cast<void*>(window_plugin_create)},
        {column_plugin_name, column_plugin_type, reinterpret_cast<void*>(column_plugin_create)},
        {row_plugin_name, row_plugin_type, reinterpret_cast<void*>(row_plugin_create)},
        {imgui_main_window_plugin_name, imgui_main_window_plugin_type, reinterpret_cast<void*>(imgui_main_window_plugin_create)},
        {slider_int_plugin_name, slider_int_plugin_type, reinterpret_cast<void*>(slider_int_plugin_create)},
        {slider_float_plugin_name, slider_float_plugin_type, reinterpret_cast<void*>(slider_float_plugin_create)},
        {checkbox_plugin_name, checkbox_plugin_type, reinterpret_cast<void*>(checkbox_plugin_create)},
        {input_text_plugin_name, input_text_plugin_type, reinterpret_cast<void*>(input_text_plugin_create)},
        {combo_plugin_name, combo_plugin_type, reinterpret_cast<void*>(combo_plugin_create)},
        {separator_plugin_name, separator_plugin_type, reinterpret_cast<void*>(separator_plugin_create)},
        {spacing_plugin_name, spacing_plugin_type, reinterpret_cast<void*>(spacing_plugin_create)},
        {tree_node_plugin_name, tree_node_plugin_type, reinterpret_cast<void*>(tree_node_plugin_create)},
        {group_plugin_name, group_plugin_type, reinterpret_cast<void*>(group_plugin_create)},
        // Backend tree-like
        {simple_data_tree_plugin_name, simple_data_tree_plugin_type, reinterpret_cast<void*>(simple_data_tree_plugin_create)},
        {data_tree_plugin_name, data_tree_plugin_type, reinterpret_cast<void*>(data_tree_plugin_create)},
    };
    return plugins;
}

} // namespace ymery

#else // !YMERY_WEB

// For non-web builds, return empty vector (plugins are loaded dynamically)
#include "static_plugins.hpp"

namespace ymery {
const std::vector<StaticPlugin>& get_static_plugins() {
    static std::vector<StaticPlugin> empty;
    return empty;
}
}

#endif // YMERY_WEB
