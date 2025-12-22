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
        spdlog::error("WidgetFactory::create_widget: failed to parse spec");
        return Err<WidgetPtr>(
            "WidgetFactory::create_widget: failed to parse spec", parse_res);
    }

    auto [widget_name, inline_props] = *parse_res;
    spdlog::debug("Creating widget: {}", widget_name);

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

    // Create DataBag for this widget
    auto data_bag_res = _create_data_bag(parent_data_bag, widget_def, namespace_);
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

    // Try to create from plugin manager
    spdlog::debug("Looking up widget type '{}' in plugin manager", widget_type);
    if (_plugin_manager->has_widget(widget_type)) {
        spdlog::debug("Found '{}' in plugin manager, creating", widget_type);
        auto res = _plugin_manager->create_widget(
            widget_type,
            shared_from_this(),
            _dispatcher,
            namespace_,
            data_bag
        );
        if (res) {
            spdlog::debug("Widget '{}' created successfully", widget_type);
        } else {
            spdlog::error("Failed to create widget '{}': {}", widget_type, error_msg(res));
        }
        return res;
    }

    // Fall back to base Widget
    spdlog::debug("Widget type '{}' not in plugin manager, using base Widget", widget_type);
    return Widget::create(
        shared_from_this(),
        _dispatcher,
        namespace_,
        data_bag
    );
}

Result<WidgetPtr> WidgetFactory::create_root_widget() {
    spdlog::info("WidgetFactory::create_root_widget");
    const Dict& app_config = _lang->app_config();
    spdlog::info("App config has {} keys", app_config.size());

    // Debug: print all keys in app_config
    for (const auto& [key, val] : app_config) {
        spdlog::info("App config key: '{}'", key);
    }

    // Get root widget name from app config (Python pattern: app.widget)
    std::string widget_name;
    auto widget_it = app_config.find("widget");
    if (widget_it != app_config.end()) {
        if (auto name = get_as<std::string>(widget_it->second)) {
            widget_name = *name;
            spdlog::info("Found 'widget' in app config: {}", widget_name);
        }
    }

    // Fallback: look for 'body' key (old pattern)
    if (widget_name.empty()) {
        auto body_it = app_config.find("body");
        if (body_it != app_config.end()) {
            spdlog::debug("Found 'body' in app config (fallback)");
            // Create root data bag
            std::map<std::string, TreeLikePtr> data_trees;
            data_trees["data"] = _data_tree;

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
        spdlog::error("No 'widget' or 'body' in app config");
        return Err<WidgetPtr>(
            "WidgetFactory::create_root_widget: no 'widget' or 'body' in app config");
    }

    // Create root data bag
    std::map<std::string, TreeLikePtr> data_trees;
    data_trees["data"] = _data_tree;

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

    // Dict spec: {button: {label: "Click"}}
    if (auto spec_dict = get_as<Dict>(spec)) {
        if (spec_dict->empty()) {
            return Err<std::pair<std::string, Dict>>(
                "WidgetFactory::_parse_widget_spec: empty dict spec");
        }

        // First key is the widget name
        auto it = spec_dict->begin();
        std::string name = it->first;
        if (name.find('.') == std::string::npos) {
            name = namespace_ + "." + name;
        }

        // Value is the inline props
        if (auto props = get_as<Dict>(it->second)) {
            inline_props = *props;
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

    return Err<Dict>(
        "WidgetFactory::_resolve_widget_definition: widget '" + full_name + "' not found");
}

Result<std::shared_ptr<DataBag>> WidgetFactory::_create_data_bag(
    std::shared_ptr<DataBag> parent,
    const Dict& widget_def,
    const std::string& namespace_
) {
    // Get parent path or root
    DataPath data_path = DataPath::root();
    if (parent) {
        auto path_res = parent->get_data_path();
        if (path_res) {
            data_path = *path_res;
        }
    }

    // Check for data-path in definition
    auto dp_it = widget_def.find("data-path");
    if (dp_it != widget_def.end()) {
        if (auto path_str = get_as<std::string>(dp_it->second)) {
            // Relative path
            if (path_str->empty() || (*path_str)[0] != '/') {
                data_path = data_path / *path_str;
            } else {
                // Absolute path
                data_path = DataPath::parse(*path_str);
            }
        }
    }

    // Create statics from widget definition
    Dict statics;
    for (const auto& [key, value] : widget_def) {
        if (key != "data-path" && key != "type") {
            statics[key] = value;
        }
    }

    // Build data trees map
    std::map<std::string, TreeLikePtr> data_trees;
    data_trees["data"] = _data_tree;

    return DataBag::create(
        _dispatcher,
        _plugin_manager,
        data_trees,
        "data",
        data_path,
        statics
    );
}

} // namespace ymery
