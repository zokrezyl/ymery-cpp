// spinners-demo widget plugin - ImSpinner demo showcase
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>
#include <imspinner.h>

namespace ymery::plugins {

class SpinnersDemo : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<SpinnersDemo>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("SpinnersDemo::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        ImSpinner::demoSpinners();
        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "spinners-demo"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::SpinnersDemo::create(wf, d, ns, db);
}
