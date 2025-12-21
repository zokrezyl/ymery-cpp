#pragma once

#include "result.hpp"
#include "types.hpp"
#include "object.hpp"
#include <map>
#include <vector>
#include <functional>
#include <memory>

namespace ymery {

// Event handler callback type
using EventHandler = std::function<Result<void>(const Dict&)>;

// Dispatcher - pub/sub for events and actions
class Dispatcher : public Object {
public:
    static Result<std::shared_ptr<Dispatcher>> create();

    // Event handlers (fire-and-forget, key = "source/name")
    Result<void> register_event_handler(const std::string& key, EventHandler handler);
    Result<void> unregister_event_handler(const std::string& key);
    Result<void> dispatch_event(const Dict& event);

    // Action handlers (require responder)
    using ActionHandler = std::function<Result<void>(const Dict&)>;
    Result<void> register_action_handler(ActionHandler handler);
    Result<void> dispatch_action(const Dict& action);

private:
    Dispatcher() = default;

    std::map<std::string, std::vector<EventHandler>> _event_handlers;
    std::vector<ActionHandler> _action_handlers;
};

using DispatcherPtr = std::shared_ptr<Dispatcher>;

} // namespace ymery
