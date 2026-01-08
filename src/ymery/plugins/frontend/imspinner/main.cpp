// imspinner plugin - Loading spinners from imspinner library
#include "../../../plugin.hpp"
#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"

// Widget headers
#include "spinner.hpp"

namespace ymery::plugins {

class ImspinnerPlugin : public Plugin {
public:
    const char* name() const override { return "imspinner"; }

    std::vector<std::string> widgets() const override {
        return {
            "spinner"
        };
    }

    Result<WidgetPtr> createWidget(
        const std::string& widget_name,
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) override {
        if (widget_name == "spinner") {
            return imspinner::Spinner::create(widget_factory, dispatcher, ns, data_bag);
        }
        return Err<WidgetPtr>("Unknown widget: " + widget_name);
    }
};

} // namespace ymery::plugins

extern "C" void* create() {
    return static_cast<void*>(new ymery::plugins::ImspinnerPlugin());
}
