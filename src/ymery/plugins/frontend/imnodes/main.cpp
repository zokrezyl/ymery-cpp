// imnodes plugin - Node graph editor
#include "../../../plugin.hpp"
#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"

#include "editor.hpp"
#include "node.hpp"
#include "input-attribute.hpp"
#include "output-attribute.hpp"
#include "link.hpp"

namespace ymery::plugins {

class ImnodesPlugin : public Plugin {
public:
    const char* name() const override { return "imnodes"; }

    std::vector<std::string> widgets() const override {
        return {
            "editor",
            "node",
            "input-attribute",
            "output-attribute",
            "link"
        };
    }

    Result<WidgetPtr> createWidget(
        const std::string& widget_name,
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) override {
        if (widget_name == "editor") {
            return imnodes::Editor::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "node") {
            return imnodes::Node::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "input-attribute") {
            return imnodes::InputAttribute::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "output-attribute") {
            return imnodes::OutputAttribute::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "link") {
            return imnodes::Link::create(widget_factory, dispatcher, ns, data_bag);
        }
        return Err<WidgetPtr>("Unknown widget: " + widget_name);
    }
};

} // namespace ymery::plugins

extern "C" void* create() {
    return static_cast<void*>(new ymery::plugins::ImnodesPlugin());
}
