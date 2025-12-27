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

    // Debug: check if body has foreach-child
    if (data_bag) {
        auto body_res = data_bag->get_static("body");
        if (body_res && body_res->has_value()) {
            if (auto body_list = get_as<List>(*body_res)) {
                spdlog::info("Composite::create: body has {} items", body_list->size());
                for (const auto& item : *body_list) {
                    if (auto dict = get_as<Dict>(item)) {
                        for (const auto& [k, v] : *dict) {
                            spdlog::info("  body item key: '{}'", k);
                        }
                    }
                }
            }
        }
    }

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
    // Clear errors from previous render cycle
    _error_messages.clear();

    // Push styles - continue even on error
    if (auto res = _push_styles(); !res) {
        _handle_error(Err<void>("Composite::render: _push_styles failed", res));
    }

    // Pre-render - continue even on error
    if (_error_messages.empty()) {
        if (auto res = _pre_render_head(); !res) {
            _handle_error(Err<void>("Composite::render: _pre_render_head failed", res));
        }
    }

    // Detect and execute events - continue even on error
    if (_error_messages.empty()) {
        if (auto res = _detect_and_execute_events(); !res) {
            _handle_error(Err<void>("Composite::render: _detect_and_execute_events failed", res));
        }
    }

    // Begin container - continue even on error
    if (_error_messages.empty()) {
        if (auto res = _begin_container(); !res) {
            _handle_error(Err<void>("Composite::render: _begin_container failed", res));
        }
    }

    // Only render children if container is open
    if (_container_open) {
        // Ensure children - continue even on error
        if (auto res = _ensure_children(); !res) {
            _handle_error(Err<void>("Composite::render: _ensure_children failed", res));
        }

        // Render children - continue even on error
        if (auto res = _render_children(); !res) {
            _handle_error(Err<void>("Composite::render: _render_children failed", res));
        }
    }

    // End container - continue even on error
    if (auto res = _end_container(); !res) {
        _handle_error(Err<void>("Composite::render: _end_container failed", res));
    }

    // Post-render - continue even on error
    if (auto res = _post_render_head(); !res) {
        _handle_error(Err<void>("Composite::render: _post_render_head failed", res));
    }

    // Pop styles - always try to cleanup
    if (auto res = _pop_styles(); !res) {
        _handle_error(Err<void>("Composite::render: _pop_styles failed", res));
    }

    // Render accumulated errors in-widget
    return _render_errors();
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

    // Normalize body to list (like Python composite.py lines 104-109)
    List body_list;
    if (auto children_list = get_as<List>(body_val)) {
        body_list = *children_list;
    } else if (auto body_str = get_as<std::string>(body_val)) {
        // Body is a string - convert to single-item list
        spdlog::info("Composite: body is string '{}', converting to list", *body_str);
        body_list.push_back(body_val);
    } else if (auto body_dict = get_as<Dict>(body_val)) {
        // Body is a dict - convert to single-item list
        spdlog::info("Composite: body is dict, converting to list");
        body_list.push_back(body_val);
    } else {
        spdlog::warn("'body' is not a list, string, or dict");
        _children_initialized = true;
        return Ok();
    }
    auto children_list = &body_list;

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

                // Create a widget for each child - like Python, add data-path to widget spec
                for (const auto& child_name : child_names) {
                    spdlog::info("foreach-child: creating widget for '{}'", child_name);

                    // Clone widget_spec and add data-path (like Python composite.py lines 174-187)
                    Value child_spec;
                    if (auto spec_dict = get_as<Dict>(widget_spec)) {
                        Dict new_spec = *spec_dict;
                        // Find the widget key and add data-path to its statics
                        for (auto& [wkey, wval] : new_spec) {
                            if (auto inner_dict = get_as<Dict>(wval)) {
                                Dict new_inner = *inner_dict;
                                new_inner["data-path"] = child_name;
                                new_spec[wkey] = new_inner;
                            } else {
                                Dict new_inner;
                                new_inner["data-path"] = child_name;
                                new_spec[wkey] = new_inner;
                            }
                            break;  // Only modify first key
                        }
                        child_spec = new_spec;
                    } else if (auto spec_str = get_as<std::string>(widget_spec)) {
                        // String spec like "tree-node" -> {tree-node: {data-path: child_name}}
                        Dict new_spec;
                        Dict inner;
                        inner["data-path"] = child_name;
                        new_spec[*spec_str] = inner;
                        child_spec = new_spec;
                    } else {
                        child_spec = widget_spec;
                    }

                    // Create the widget with PARENT data bag - factory handles navigation
                    auto widget_res = _widget_factory->create_widget(_data_bag, child_spec, _namespace);
                    if (!widget_res) {
                        _handle_error(Err<void>("Composite::_ensure_children: foreach-child failed to create widget for '" + child_name + "'", widget_res));
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
            _handle_error(Err<void>("Composite::_ensure_children: failed to create child widget", widget_res));
            continue;
        }
        _children.push_back(*widget_res);
        spdlog::debug("Composite::_ensure_children: created child widget");
    }

    spdlog::debug("Created {} children", _children.size());
    _children_initialized = true;
    return Ok();
}

Result<void> Composite::_render_children() {
    for (auto& child : _children) {
        if (child) {
            if (auto res = child->render(); !res) {
                // Accumulate error but continue rendering other children
                _handle_error(Err<void>("Composite::_render_children: child render failed", res));
            }
        }
    }
    return Ok();
}

} // namespace ymery
