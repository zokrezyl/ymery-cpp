// imgui-toggle plugin - Toggle switch controls
#include "../../../plugin.hpp"
#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"

#include "toggle.hpp"

namespace ymery::plugins {

class ImguiTogglePlugin : public Plugin {
public:
    const char* name() const override { return "imgui-toggle"; }

    std::vector<std::string> widgets() const override {
        return { "toggle" };
    }

    Result<WidgetPtr> createWidget(
        const std::string& widget_name,
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) override {
        if (widget_name == "toggle") {
            return imgui_toggle::Toggle::create(widget_factory, dispatcher, ns, data_bag);
        }
        return Err<WidgetPtr>("Unknown widget: " + widget_name);
    }
};

} // namespace ymery::plugins

extern "C" void* create() {
    return static_cast<void*>(new ymery::plugins::ImguiTogglePlugin());
}
