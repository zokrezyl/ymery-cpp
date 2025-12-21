#pragma once

#include "../result.hpp"
#include "../types.hpp"
#include "../data_bag.hpp"
#include "../dispatcher.hpp"
#include "../lang.hpp"
#include "widget.hpp"
#include <map>
#include <memory>

namespace ymery {

class PluginManager;

// WidgetFactory - creates widgets from YAML specs
class WidgetFactory : public std::enable_shared_from_this<WidgetFactory> {
public:
    // Factory method
    static Result<std::shared_ptr<WidgetFactory>> create(
        std::shared_ptr<Lang> lang,
        std::shared_ptr<Dispatcher> dispatcher,
        std::shared_ptr<TreeLike> data_tree,
        std::shared_ptr<PluginManager> plugin_manager
    );

    // Create a widget from spec
    Result<WidgetPtr> create_widget(
        std::shared_ptr<DataBag> parent_data_bag,
        const Value& spec,
        const std::string& namespace_
    );

    // Create root widget from app config
    Result<WidgetPtr> create_root_widget();

    // Lifecycle
    Result<void> init();

    // Access
    std::shared_ptr<Lang> lang() const { return _lang; }
    std::shared_ptr<Dispatcher> dispatcher() const { return _dispatcher; }
    std::shared_ptr<TreeLike> data_tree() const { return _data_tree; }
    std::shared_ptr<PluginManager> plugin_manager() const { return _plugin_manager; }

private:
    WidgetFactory() = default;

    // Parse widget spec string like "app.Button" or "@ref"
    Result<std::pair<std::string, Dict>> _parse_widget_spec(
        const Value& spec,
        const std::string& namespace_
    );

    // Resolve widget definition from name
    Result<Dict> _resolve_widget_definition(const std::string& full_name);

    // Create DataBag for widget
    Result<std::shared_ptr<DataBag>> _create_data_bag(
        std::shared_ptr<DataBag> parent,
        const Dict& widget_def,
        const std::string& namespace_
    );

    std::shared_ptr<Lang> _lang;
    std::shared_ptr<Dispatcher> _dispatcher;
    std::shared_ptr<TreeLike> _data_tree;
    std::shared_ptr<PluginManager> _plugin_manager;

    // Widget cache by uid
    std::map<std::string, std::weak_ptr<Widget>> _widget_cache;
};

using WidgetFactoryPtr = std::shared_ptr<WidgetFactory>;

} // namespace ymery
