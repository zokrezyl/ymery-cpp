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
    spdlog::debug("Composite::render start");

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
    spdlog::debug("Composite::render _container_open={}", _container_open);

    // Only render children if container is open
    if (_container_open) {
        // Ensure and render children
        if (auto children_res = _ensure_children(); !children_res) {
            spdlog::warn("Composite::render: _ensure_children failed: {}", error_msg(children_res));
        }

        spdlog::debug("Composite::render rendering {} children", _children.size());
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
    if (_children_initialized) {
        return Ok();
    }

    spdlog::debug("Composite::_ensure_children starting");

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

    spdlog::debug("Found {} body specs", children_list->size());

    // Create each child widget
    for (const auto& child_spec : *children_list) {
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
