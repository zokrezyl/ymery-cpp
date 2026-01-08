// implot3d plugin - 3D plotting
#include "../../../plugin.hpp"
#include "../../../frontend/widget.hpp"
#include "../../../frontend/widget_factory.hpp"

#include "plot3d.hpp"

namespace ymery::plugins {

class Implot3dPlugin : public Plugin {
public:
    const char* name() const override { return "implot3d"; }

    std::vector<std::string> widgets() const override {
        return { "plot3d" };
    }

    Result<WidgetPtr> createWidget(
        const std::string& widget_name,
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) override {
        if (widget_name == "plot3d") {
            return implot3d::Plot3D::create(widget_factory, dispatcher, ns, data_bag);
        }
        return Err<WidgetPtr>("Unknown widget: " + widget_name);
    }
};

} // namespace ymery::plugins

extern "C" void* create() {
    return static_cast<void*>(new ymery::plugins::Implot3dPlugin());
}
