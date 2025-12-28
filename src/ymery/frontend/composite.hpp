#pragma once

#include "widget.hpp"
#include <vector>

namespace ymery {

// Composite - widget that contains multiple children
class Composite : public Widget {
public:
    Composite() = default;
    virtual ~Composite() = default;

    // Factory method
    static Result<std::shared_ptr<Composite>> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    );

    // Lifecycle
    Result<void> init() override;
    Result<void> dispose() override;

    // Rendering
    Result<void> render() override;

protected:
    // Override to provide container behavior
    virtual Result<void> _begin_container();
    virtual Result<void> _end_container();

    // Child management
    Result<void> _ensure_children();
    virtual Result<void> _render_children();

    std::vector<WidgetPtr> _children;
    bool _children_initialized = false;
    bool _container_open = true;

    // Cache for foreach-child to detect when data changes
    std::vector<std::string> _foreach_child_names;
};

} // namespace ymery
