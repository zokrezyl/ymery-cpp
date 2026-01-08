// node-graph plugin
#include "../../../plugin.hpp"
#include "node-graph.hpp"

namespace ymery::plugins {

class NodeGraphPlugin : public Plugin {
public:
    const char* name() const override { return "node-graph"; }

    std::vector<std::string> widgets() const override {
        return {"node-graph"};
    }

    Result<WidgetPtr> createWidget(
        const std::string& widget_name,
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) override {
        if (widget_name == "node-graph") {
            return NodeGraphWidget::create(widget_factory, dispatcher, ns, data_bag);
        }
        return Err<WidgetPtr>("Unknown widget: " + widget_name);
    }
};

} // namespace ymery::plugins

extern "C" void* create() {
    return static_cast<void*>(new ymery::plugins::NodeGraphPlugin());
}
