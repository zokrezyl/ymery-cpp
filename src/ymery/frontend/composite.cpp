#include "composite.hpp"
#include "widget_factory.hpp"
#include <imgui.h>
#include <spdlog/spdlog.h>

namespace ymery {

Result<std::shared_ptr<Composite>> Composite::create(
    std::shared_ptr<WidgetFactory> widget_factory,
    std::shared_ptr<Dispatcher> dispatcher,
    const std::string& ns,
    std::shared_ptr<DataBag> data_bag
) {
    auto composite = std::make_shared<Composite>();
    composite->_widget_factory = widget_factory;
    composite->_dispatcher = dispatcher;
    composite->_namespace = ns;
    composite->_data_bag = data_bag;

    if (auto res = composite->init(); !res) {
        return Err<std::shared_ptr<Composite>>("Composite::create: init failed", res);
    }
    return composite;
}

Result<void> Composite::init() {
    if (auto res = Widget::init(); !res) {
        return res;
    }
    return Ok();
}

Result<void> Composite::dispose() {
    for (auto& child : _children) {
        if (child) {
            child->dispose();
        }
    }
    _children.clear();
    _children_initialized = false;

    return Widget::dispose();
}

Result<void> Composite::render() {
    // Push styles
    if (auto style_res = _push_styles(); !style_res) {
        spdlog::warn("Composite::render: _push_styles failed");
    }

    // Pre-render
    if (auto pre_res = _pre_render_head(); !pre_res) {
        _pop_styles();
        return Err<void>("Composite::render: _pre_render_head failed", pre_res);
    }

    // Detect and execute events
    _detect_and_execute_events();

    // Begin container
    if (auto begin_res = _begin_container(); !begin_res) {
        _pop_styles();
        return Err<void>("Composite::render: _begin_container failed", begin_res);
    }

    // Only render children if container is open
    if (_container_open) {
        // Ensure and render children
        if (auto children_res = _ensure_children(); !children_res) {
            spdlog::warn("Composite::render: _ensure_children failed: {}", error_msg(children_res));
        }

        if (auto render_children_res = _render_children(); !render_children_res) {
            spdlog::warn("Composite::render: _render_children failed");
        }
    }

    // End container
    if (auto end_res = _end_container(); !end_res) {
        spdlog::warn("Composite::render: _end_container failed");
    }

    // Post-render
    if (auto post_res = _post_render_head(); !post_res) {
        spdlog::warn("Composite::render: _post_render_head failed");
    }

    // Pop styles
    _pop_styles();

    return Ok();
}

Result<void> Composite::_begin_container() {
    _container_open = true;
    return Ok();
}

Result<void> Composite::_end_container() {
    return Ok();
}

Result<void> Composite::_ensure_children() {
    // For foreach-child, we need to rebuild children each frame to handle dynamic data
    // For static children, only initialize once

    // Get body spec from statics
    auto body_res = _data_bag->get_static("body");
    if (!body_res) {
        spdlog::debug("No 'body' static found");
        _children_initialized = true;
        return Ok();
    }

    auto body_val = *body_res;
    if (!body_val.has_value()) {
        spdlog::debug("'body' has no value");
        _children_initialized = true;
        return Ok();
    }

    // Body should be a list
    auto children_list = get_as<List>(body_val);
    if (!children_list) {
        spdlog::warn("'body' is not a list");
        _children_initialized = true;
        return Ok();
    }

    // Check if any body item is foreach-child
    bool has_foreach_child = false;
    for (const auto& item : *children_list) {
        if (auto dict = get_as<Dict>(item)) {
            if (dict->find("foreach-child") != dict->end()) {
                has_foreach_child = true;
                break;
            }
        }
    }

    // If no foreach-child and already initialized, skip
    if (!has_foreach_child && _children_initialized) {
        return Ok();
    }

    // For foreach-child, clear and rebuild children each frame
    if (has_foreach_child) {
        for (auto& child : _children) {
            if (child) child->dispose();
        }
        _children.clear();
    }

    spdlog::info("Composite::_ensure_children: {} body specs, has_foreach_child={}", children_list->size(), has_foreach_child);

    // Create each child widget
    for (const auto& child_spec : *children_list) {
        // Check for foreach-child
        if (auto dict = get_as<Dict>(child_spec)) {
            spdlog::info("Composite: body item is dict with {} keys", dict->size());
            for (const auto& [k, v] : *dict) {
                spdlog::info("  key: '{}'", k);
            }
            auto foreach_it = dict->find("foreach-child");
            if (foreach_it != dict->end()) {
                spdlog::info("foreach-child: FOUND! Getting children from data bag");
                // Get data path for debugging
                if (auto path_res = _data_bag->get_data_path_str(); path_res) {
                    spdlog::info("foreach-child: current data path = '{}'", *path_res);
                }
                // Get children names from data tree
                auto children_res = _data_bag->get_children_names();
                if (!children_res) {
                    spdlog::warn("foreach-child: failed to get children names: {}", error_msg(children_res));
                    continue;
                }
                auto child_names = *children_res;
                spdlog::info("foreach-child: found {} children", child_names.size());

                // Get the widget spec inside foreach-child
                auto foreach_val = foreach_it->second;
                Value widget_spec;
                if (auto list = get_as<List>(foreach_val)) {
                    if (!list->empty()) {
                        widget_spec = (*list)[0];
                    }
                } else {
                    widget_spec = foreach_val;
                }

                // Create a widget for each child
                for (const auto& child_name : child_names) {
                    spdlog::info("foreach-child: creating widget for '{}'", child_name);

                    // Create child DataBag with data-path set to the child name
                    auto child_bag_res = _data_bag->inherit(child_name, {});
                    if (!child_bag_res) {
                        spdlog::error("foreach-child: failed to inherit DataBag for '{}': {}", child_name, error_msg(child_bag_res));
                        continue;
                    }
                    auto child_bag = *child_bag_res;

                    // Create the widget with the child's data bag
                    auto widget_res = _widget_factory->create_widget(child_bag, widget_spec, _namespace);
                    if (!widget_res) {
                        spdlog::error("foreach-child: failed to create widget for '{}': {}", child_name, error_msg(widget_res));
                        continue;
                    }
                    _children.push_back(*widget_res);
                }
                continue;
            }
        }

        // Regular widget creation
        auto widget_res = _widget_factory->create_widget(_data_bag, child_spec, _namespace);
        if (!widget_res) {
            spdlog::error("Failed to create child widget: {}", error_msg(widget_res));
            continue;
        }
        _children.push_back(*widget_res);
        spdlog::debug("Created child widget");
    }

    spdlog::debug("Created {} children", _children.size());
    _children_initialized = true;
    return Ok();
}

Result<void> Composite::_render_children() {
    for (auto& child : _children) {
        if (child) {
            if (auto res = child->render(); !res) {
                // Log but continue
            }
        }
    }
    return Ok();
}

} // namespace ymery
