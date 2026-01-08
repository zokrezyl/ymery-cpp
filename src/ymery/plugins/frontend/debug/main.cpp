// debug plugin - debugging/logging widgets
#include "../../../plugin.hpp"
#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"

#include "logs-view.hpp"
#include "errors-view.hpp"

namespace ymery::plugins {

class DebugPlugin : public Plugin {
public:
    const char* name() const override { return "debug"; }

    std::vector<std::string> widgets() const override {
        return { "logs-view", "errors-view" };
    }

    Result<WidgetPtr> createWidget(
        const std::string& widget_name,
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) override {
        if (widget_name == "logs-view") {
            return debug::LogsView::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "errors-view") {
            return debug::ErrorsView::create(widget_factory, dispatcher, ns, data_bag);
        }
        return Err<WidgetPtr>("Unknown widget: " + widget_name);
    }
};

} // namespace ymery::plugins

extern "C" void* create() {
    return static_cast<void*>(new ymery::plugins::DebugPlugin());
}
