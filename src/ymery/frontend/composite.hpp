#pragma once

#include "widget.hpp"
#include <vector>

namespace ymery {

// Composite - widget that contains multiple children
class Composite : public Widget {
public:
    Composite() = default;
    virtual ~Composite() = default;

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
};

} // namespace ymery
