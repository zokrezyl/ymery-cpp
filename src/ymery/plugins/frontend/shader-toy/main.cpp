// shader-toy plugin
#include "../../../plugin.hpp"
#include "shader-toy.hpp"

namespace ymery::plugins {

class ShaderToyPlugin : public Plugin {
public:
    const char* name() const override { return "shader-toy"; }

    std::vector<std::string> widgets() const override {
        return {"shader-toy"};
    }

    Result<WidgetPtr> createWidget(
        const std::string& widget_name,
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) override {
        if (widget_name == "shader-toy") {
#ifdef YMERY_USE_WEBGPU
            return ShaderToy::create(widget_factory, dispatcher, ns, data_bag);
#else
            return ShaderToyStub::create(widget_factory, dispatcher, ns, data_bag);
#endif
        }
        return Err<WidgetPtr>("Unknown widget: " + widget_name);
    }
};

} // namespace ymery::plugins

extern "C" void* create() {
    return static_cast<void*>(new ymery::plugins::ShaderToyPlugin());
}
