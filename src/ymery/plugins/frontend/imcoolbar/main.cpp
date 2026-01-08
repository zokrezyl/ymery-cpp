// imcoolbar plugin - macOS-style dock bar
#include "../../../plugin.hpp"
#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"

#include "coolbar.hpp"

namespace ymery::plugins {

class ImcoolbarPlugin : public Plugin {
public:
    const char* name() const override { return "imcoolbar"; }

    std::vector<std::string> widgets() const override {
        return { "coolbar" };
    }

    Result<WidgetPtr> createWidget(
        const std::string& widget_name,
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) override {
        if (widget_name == "coolbar") {
            return imcoolbar::CoolBar::create(widget_factory, dispatcher, ns, data_bag);
        }
        return Err<WidgetPtr>("Unknown widget: " + widget_name);
    }
};

} // namespace ymery::plugins

extern "C" void* create() {
    return static_cast<void*>(new ymery::plugins::ImcoolbarPlugin());
}
