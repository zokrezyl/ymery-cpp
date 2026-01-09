#include "widget_factory.hpp"
#include "composite.hpp"
#include "../plugin_manager.hpp"
#include <spdlog/spdlog.h>

namespace ymery {

Result<std::shared_ptr<WidgetFactory>> WidgetFactory::create(
    std::shared_ptr<Lang> lang,
    std::shared_ptr<Dispatcher> dispatcher,
    std::shared_ptr<TreeLike> data_tree,
    std::shared_ptr<PluginManager> plugin_manager
) {
    auto factory = std::shared_ptr<WidgetFactory>(new WidgetFactory());
    factory->_lang = lang;
    factory->_dispatcher = dispatcher;
    factory->_data_tree = data_tree;
    factory->_plugin_manager = plugin_manager;

    if (auto res = factory->init(); !res) {
        return Err<std::shared_ptr<WidgetFactory>>(
            "WidgetFactory::create: init failed", res);
    }

    return factory;
}

Result<void> WidgetFactory::init() {
    return Ok();
}

void WidgetFactory::dispose() {
    spdlog::debug("WidgetFactory::dispose");

    // Clear widget cache first
    _widget_cache.clear();

    // Clear data trees
    _data_trees.clear();

    // Release references to avoid circular dependencies on destruction
    _plugin_manager.reset();
    _data_tree.reset();
    _dispatcher.reset();
    _lang.reset();
}

Result<WidgetPtr> WidgetFactory::create_widget(
    std::shared_ptr<DataBag> parent_data_bag,
    const Value& spec,
    const std::string& namespace_
) {
    // Check if spec is a List - create Composite widget
    if (auto spec_list = get_as<List>(spec)) {
        spdlog::debug("Creating Composite for list of {} widgets", spec_list->size());

        // Create a data bag with the body list as statics
        Dict statics;
        statics["body"] = spec;

        std::map<std::string, TreeLikePtr> data_trees;
        data_trees["data"] = _data_tree;

        DataPath data_path = DataPath::root();
        if (parent_data_bag) {
            if (auto path_res = parent_data_bag->get_data_path()) {
                data_path = *path_res;
            }
        }

        auto data_bag_res = DataBag::create(
            _dispatcher,
            _plugin_manager,
            data_trees,
            "data",
            data_path,
            statics
        );
        if (!data_bag_res) {
            return Err<WidgetPtr>("WidgetFactory::create_widget: failed to create data bag for composite", data_bag_res);
        }

        return Composite::create(shared_from_this(), _dispatcher, namespace_, *data_bag_res);
    }

    // Parse the spec to get widget name and inline props
    auto parse_res = _parse_widget_spec(spec, namespace_);
    if (!parse_res) {
        return Err<WidgetPtr>(
            "WidgetFactory::create_widget: failed to parse spec", parse_res);
    }

    auto [widget_name, inline_props] = *parse_res;
    spdlog::debug("Creating widget: {}", widget_name);

    // Check if widget is defined in YAML first (before checking plugins)
    const auto& yaml_defs = _lang->widget_definitions();
    bool is_yaml_widget = yaml_defs.find(widget_name) != yaml_defs.end();

    // Resolve widget definition
    auto def_res = _resolve_widget_definition(widget_name);
    if (!def_res) {
        return Err<WidgetPtr>(
            "WidgetFactory::create_widget: failed to resolve '" + widget_name + "'", def_res);
    }

    Dict widget_def = *def_res;

    // Merge inline props into definition
    for (const auto& [key, value] : inline_props) {
        widget_def[key] = value;
    }

    // Extract namespace from widget_name ONLY for YAML-defined widgets
    // Plugin widgets (e.g., imgui.button) should NOT change the namespace
    // e.g., "shared.imgui.demo" -> namespace "shared.imgui"
    std::string child_namespace = namespace_;
    if (is_yaml_widget) {
        size_t last_dot = widget_name.rfind('.');
        if (last_dot != std::string::npos) {
            child_namespace = widget_name.substr(0, last_dot);
        }
    }

    // Create DataBag for this widget
    auto data_bag_res = _create_data_bag(parent_data_bag, widget_def, child_namespace);
    if (!data_bag_res) {
        return Err<WidgetPtr>(
            "WidgetFactory::create_widget: failed to create data bag", data_bag_res);
    }

    auto data_bag = *data_bag_res;

    // Get widget type from definition
    std::string widget_type = "widget"; // default
    auto type_it = widget_def.find("type");
    if (type_it != widget_def.end()) {
        if (auto t = get_as<std::string>(type_it->second)) {
            widget_type = *t;
        }
    }

    // Extract base type (after dot if present, e.g., "imgui.composite" -> "composite")
    std::string base_type = widget_type;
    size_t dot_pos = widget_type.rfind('.');
    if (dot_pos != std::string::npos) {
        base_type = widget_type.substr(dot_pos + 1);
    }

    // Handle built-in types first
    if (base_type == "composite") {
        spdlog::debug("Creating built-in Composite widget with namespace '{}'", child_namespace);
        return Composite::create(shared_from_this(), _dispatcher, child_namespace, data_bag);
    }

    // Create widget from plugin manager - let it fail if widget type unknown
    spdlog::debug("Creating widget type '{}' from plugin manager", widget_type);
    auto res = _plugin_manager->create_widget(
        widget_type,
        shared_from_this(),
        _dispatcher,
        child_namespace,
        data_bag
    );
    if (!res) {
        return Err<WidgetPtr>("WidgetFactory::create_widget: unknown widget type '" + widget_type + "'", res);
    }
    return res;
}

Result<WidgetPtr> WidgetFactory::create_root_widget() {
    spdlog::debug("WidgetFactory::create_root_widget");
    const Dict& app_config = _lang->app_config();
    spdlog::debug("App config has {} keys", app_config.size());

    // Debug: print all keys in app_config
    for (const auto& [key, val] : app_config) {
        spdlog::debug("App config key: '{}'", key);
    }

    // Simple data trees map with just the main data tree
    std::map<std::string, TreeLikePtr> data_trees;
    data_trees["data"] = _data_tree;

    // Get root widget name from app config (e.g., app.root-widget: app.main-window)
    std::string widget_name;
    auto widget_it = app_config.find("root-widget");
    if (widget_it != app_config.end()) {
        if (auto name = get_as<std::string>(widget_it->second)) {
            widget_name = *name;
            spdlog::debug("Found 'root-widget' in app config: {}", widget_name);
        }
    }

    // Fallback: look for 'body' key (old pattern)
    if (widget_name.empty()) {
        auto body_it = app_config.find("body");
        if (body_it != app_config.end()) {
            spdlog::debug("Found 'body' in app config (fallback)");
            // Create root data bag
            auto root_data_bag_res = DataBag::create(
                _dispatcher,
                _plugin_manager,
                data_trees,
                "data",
                DataPath::root(),
                {}  // empty statics
            );
            if (!root_data_bag_res) {
                return Err<WidgetPtr>(
                    "WidgetFactory::create_root_widget: failed to create root data bag",
                    root_data_bag_res);
            }
            return create_widget(*root_data_bag_res, body_it->second, "app");
        }
    }

    if (widget_name.empty()) {
        return Err<WidgetPtr>(
            "WidgetFactory::create_root_widget: no 'widget' or 'body' in app config");
    }

    auto root_data_bag_res = DataBag::create(
        _dispatcher,
        _plugin_manager,
        data_trees,
        "data",
        DataPath::root(),
        {}  // empty statics
    );
    if (!root_data_bag_res) {
        return Err<WidgetPtr>(
            "WidgetFactory::create_root_widget: failed to create root data bag",
            root_data_bag_res);
    }

    return create_widget(*root_data_bag_res, Value(widget_name), "app");
}

Result<std::pair<std::string, Dict>> WidgetFactory::_parse_widget_spec(
    const Value& spec,
    const std::string& namespace_
) {
    Dict inline_props;

    // String spec: "app.button" or just "button"
    if (auto spec_str = get_as<std::string>(spec)) {
        std::string name = *spec_str;
        // If no namespace prefix, add current namespace
        if (name.find('.') == std::string::npos) {
            name = namespace_ + "." + name;
        }
        return Ok(std::make_pair(name, inline_props));
    }

    // Dict spec: {button: {label: "Click"}} or {button: {label: "Click"}, data-path: "/foo"}
    if (auto spec_dict = get_as<Dict>(spec)) {
        if (spec_dict->empty()) {
            return Err<std::pair<std::string, Dict>>(
                "WidgetFactory::_parse_widget_spec: empty dict spec");
        }

        // Find the widget key (first key that is not "data-path")
        std::string name;
        for (const auto& [key, value] : *spec_dict) {
            if (key != "data-path") {
                name = key;
                if (name.find('.') == std::string::npos) {
                    name = namespace_ + "." + name;
                }
                // Value is the inline props
                if (auto props = get_as<Dict>(value)) {
                    inline_props = *props;
                }
                break;
            }
        }

        if (name.empty()) {
            return Err<std::pair<std::string, Dict>>(
                "WidgetFactory::_parse_widget_spec: no widget key found in dict spec");
        }

        // Check for data-path sibling key and add to inline_props
        auto dp_it = spec_dict->find("data-path");
        if (dp_it != spec_dict->end()) {
            inline_props["data-path"] = dp_it->second;
            spdlog::debug("_parse_widget_spec: found sibling data-path for '{}'", name);
        }

        return Ok(std::make_pair(name, inline_props));
    }

    return Err<std::pair<std::string, Dict>>(
        "WidgetFactory::_parse_widget_spec: invalid spec type");
}

Result<Dict> WidgetFactory::_resolve_widget_definition(const std::string& full_name) {
    const auto& defs = _lang->widget_definitions();
    auto it = defs.find(full_name);
    if (it != defs.end()) {
        return Ok(it->second);
    }

    // Not in YAML definitions - check if it's a plugin widget
    // For new-style plugins, full_name is "plugin.widget" (e.g., "imgui.text")
    // For legacy, it could be "namespace.widget" (e.g., "app.button")

    // First try the full name as plugin.widget format
    if (_plugin_manager->has_widget(full_name)) {
        Dict def;
        def["type"] = full_name;  // Keep full name for routing
        spdlog::debug("WidgetFactory::_resolve_widget_definition: using plugin widget '{}'", full_name);
        return Ok(def);
    }

    // Legacy fallback: extract widget name without namespace
    size_t dot_pos = full_name.rfind('.');
    if (dot_pos != std::string::npos) {
        std::string widget_name = full_name.substr(dot_pos + 1);
        if (_plugin_manager->has_widget(widget_name)) {
            Dict def;
            def["type"] = widget_name;
            spdlog::debug("WidgetFactory::_resolve_widget_definition: using legacy plugin widget '{}' directly", widget_name);
            return Ok(def);
        }
    }

    return Err<Dict>(
        "WidgetFactory::_resolve_widget_definition: widget '" + full_name + "' not found in YAML definitions or plugins");
}

Result<std::shared_ptr<DataBag>> WidgetFactory::_create_data_bag(
    std::shared_ptr<DataBag> parent,
    const Dict& widget_def,
    const std::string& namespace_
) {
    // Debug: show widget_def keys
    std::string keys_str;
    for (const auto& [k, _] : widget_def) {
        if (!keys_str.empty()) keys_str += ", ";
        keys_str += k;
    }
    spdlog::debug("_create_data_bag: widget_def keys=[{}]", keys_str);

    // Create statics from widget definition (excluding data-path)
    Dict statics;
    for (const auto& [key, value] : widget_def) {
        if (key != "data-path") {
            // Strip plugin prefix from type (e.g., "imgui.button" -> "button")
            // so widgets can check type without knowing the plugin namespace
            if (key == "type") {
                if (auto type_str = get_as<std::string>(value)) {
                    size_t dot = type_str->find('.');
                    if (dot != std::string::npos) {
                        statics[key] = type_str->substr(dot + 1);
                    } else {
                        statics[key] = value;
                    }
                } else {
                    statics[key] = value;
                }
            } else {
                statics[key] = value;
            }
        }
    }

    // If we have a parent, use inherit() to properly copy data trees and navigate
    if (parent) {
        // Get data-path from definition
        std::string data_path_spec;
        auto dp_it = widget_def.find("data-path");
        if (dp_it != widget_def.end()) {
            if (auto path_str = get_as<std::string>(dp_it->second)) {
                data_path_spec = *path_str;
                spdlog::debug("_create_data_bag: inheriting with data-path='{}'", data_path_spec);
            }
        }

        // Use parent's inherit() to copy data trees and navigate to child path
        return parent->inherit(data_path_spec, statics);
    }

    // No parent - create fresh DataBag with factory's data tree
    std::map<std::string, TreeLikePtr> data_trees;
    data_trees["data"] = _data_tree;

    return DataBag::create(
        _dispatcher,
        _plugin_manager,
        data_trees,
        "data",
        DataPath::root(),
        statics
    );
}

} // namespace ymery
