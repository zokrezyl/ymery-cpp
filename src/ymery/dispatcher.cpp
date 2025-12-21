#include "dispatcher.hpp"

namespace ymery {

Result<std::shared_ptr<Dispatcher>> Dispatcher::create() {
    auto dispatcher = std::shared_ptr<Dispatcher>(new Dispatcher());
    if (auto res = dispatcher->init(); !res) {
        return Err<std::shared_ptr<Dispatcher>>("Dispatcher::create: init failed", res);
    }
    return dispatcher;
}

Result<void> Dispatcher::register_event_handler(const std::string& key, EventHandler handler) {
    _event_handlers[key].push_back(std::move(handler));
    return Ok();
}

Result<void> Dispatcher::unregister_event_handler(const std::string& key) {
    _event_handlers.erase(key);
    return Ok();
}

Result<void> Dispatcher::dispatch_event(const Dict& event) {
    // Extract source and name from event
    std::string source, name;

    auto source_it = event.find("source");
    if (source_it != event.end()) {
        if (auto s = get_as<std::string>(source_it->second)) {
            source = *s;
        }
    }

    auto name_it = event.find("name");
    if (name_it != event.end()) {
        if (auto n = get_as<std::string>(name_it->second)) {
            name = *n;
        }
    }

    // Build key
    std::string key = source + "/" + name;

    // Find and call handlers
    auto it = _event_handlers.find(key);
    if (it != _event_handlers.end()) {
        for (auto& handler : it->second) {
            auto res = handler(event);
            if (!res) {
                // Log error but continue
            }
        }
    }

    // Also try wildcard handlers
    auto wild_it = _event_handlers.find("*/" + name);
    if (wild_it != _event_handlers.end()) {
        for (auto& handler : wild_it->second) {
            handler(event);
        }
    }

    return Ok();
}

Result<void> Dispatcher::register_action_handler(ActionHandler handler) {
    _action_handlers.push_back(std::move(handler));
    return Ok();
}

Result<void> Dispatcher::dispatch_action(const Dict& action) {
    for (auto& handler : _action_handlers) {
        auto res = handler(action);
        if (res) {
            return res; // First handler that succeeds
        }
    }
    return Err<void>("Dispatcher::dispatch_action: no handler responded");
}

} // namespace ymery
