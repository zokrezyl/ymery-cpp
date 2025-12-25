#pragma once

#include "../result.hpp"
#include "../types.hpp"
#include "../data_bag.hpp"
#include "../dispatcher.hpp"
#include <map>
#include <memory>
#include <vector>
#include <functional>
#include <atomic>

namespace ymery {

class WidgetFactory;

// Widget - base class for all UI components
class Widget : public std::enable_shared_from_this<Widget> {
public:
    Widget() = default;
    virtual ~Widget() = default;

    // Factory method
    static Result<std::shared_ptr<Widget>> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    );

    // Lifecycle
    virtual Result<void> init();
    virtual Result<void> dispose();

    // Main render method
    virtual Result<void> render();

    // Event handling
    virtual Result<void> handle_event(const Dict& event);

    // Accessors
    std::shared_ptr<DataBag> data_bag() const { return _data_bag; }

protected:
    // Overridable rendering methods
    virtual Result<void> _pre_render_head();
    virtual Result<void> _post_render_head();

    // Body management
    virtual Result<void> _ensure_body();

    // Style management
    virtual Result<void> _push_styles();
    virtual Result<void> _pop_styles();

    // Event detection and execution
    virtual Result<void> _detect_and_execute_events();
    virtual Result<void> _execute_event_commands(const std::string& event_name);
    virtual Result<void> _execute_event_command(const Dict& command);

    // Protected members - underscore prefix
    std::shared_ptr<WidgetFactory> _widget_factory;
    std::shared_ptr<Dispatcher> _dispatcher;
    std::shared_ptr<DataBag> _data_bag;
    std::string _namespace;

    std::shared_ptr<Widget> _body;
    bool _is_body_activated = false;

    std::map<std::string, std::vector<Dict>> _event_handlers;
    std::vector<std::pair<int, float>> _pushed_colors;
    std::vector<std::pair<int, float>> _pushed_vars;

    // Unique ID for ImGui
    std::string _uid = std::to_string(++_uid_counter);
    static inline std::atomic<int> _uid_counter{0};
};

using WidgetPtr = std::shared_ptr<Widget>;

} // namespace ymery
