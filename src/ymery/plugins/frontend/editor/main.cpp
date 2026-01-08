// editor plugin - layout editor widgets
#include "../../../plugin.hpp"
#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"

#include "widget-tree.hpp"
#include "editor.hpp"
#include "preview.hpp"
#include "properties.hpp"
#include "code-preview.hpp"
#include "data-editor.hpp"

namespace ymery::plugins {

class EditorPlugin : public Plugin {
public:
    const char* name() const override { return "editor"; }

    std::vector<std::string> widgets() const override {
        return {
            "widget-tree",
            "editor",
            "preview",
            "properties",
            "code-preview",
            "data-editor"
        };
    }

    Result<WidgetPtr> createWidget(
        const std::string& widget_name,
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) override {
        if (widget_name == "widget-tree") {
            return editor::WidgetTree::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "editor") {
            return editor::Editor::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "preview") {
            return editor::Preview::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "properties") {
            return editor::Properties::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "code-preview") {
            return editor::CodePreview::create(widget_factory, dispatcher, ns, data_bag);
        }
        if (widget_name == "data-editor") {
            return editor::DataEditor::create(widget_factory, dispatcher, ns, data_bag);
        }
        return Err<WidgetPtr>("Unknown widget: " + widget_name);
    }
};

} // namespace ymery::plugins

extern "C" void* create() {
    return static_cast<void*>(new ymery::plugins::EditorPlugin());
}
