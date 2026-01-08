// nodes plugin
#include "../../../plugin.hpp"
#include "nodes.hpp"

namespace ymery::plugins {

class NodesPlugin : public Plugin {
public:
    const char* name() const override { return "nodes"; }

    std::vector<std::string> widgets() const override {
        return {"nodes"};
    }

    Result<WidgetPtr> createWidget(
        const std::string& widget_name,
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) override {
        if (widget_name == "nodes") {
            return NodesWidget::create(widget_factory, dispatcher, ns, data_bag);
        }
        return Err<WidgetPtr>("Unknown widget: " + widget_name);
    }
};

} // namespace ymery::plugins

extern "C" void* create() {
    return static_cast<void*>(new ymery::plugins::NodesPlugin());
}
