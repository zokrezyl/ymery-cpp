// imgui-node-editor plugin - thedmd/imgui-node-editor
#include "../../../plugin.hpp"
#include "node-editor.hpp"
#include "node.hpp"
#include "node-pin.hpp"

namespace ymery::plugins {

class ImguiNodeEditorPlugin : public Plugin {
public:
    const char* name() const override { return "imgui-node-editor"; }

    std::vector<std::string> widgets() const override {
        return {"node-editor", "node", "node-pin"};
    }

    Result<WidgetPtr> createWidget(
        const std::string& widget_name,
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) override {
        if (widget_name == "node-editor") {
            return NodeEditor::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "node") {
            return Node::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "node-pin") {
            return NodePin::create(widget_factory, dispatcher, ns, data_bag);
        }
        return Err<WidgetPtr>("Unknown widget: " + widget_name);
    }
};

} // namespace ymery::plugins

extern "C" void* create() {
    return static_cast<void*>(new ymery::plugins::ImguiNodeEditorPlugin());
}
