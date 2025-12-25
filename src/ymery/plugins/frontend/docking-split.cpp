// docking-split widget plugin - configuration widget for dock space splits
// This widget doesn't render anything - it's read by docking-main-window to set up layout
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include "../../types.hpp"
#include <imgui.h>

namespace ymery::plugins {

class DockingSplit : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<DockingSplit>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("DockingSplit::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        // DockingSplit doesn't render - it's configuration only
        // The parent docking-main-window reads our properties
        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "docking-split"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::DockingSplit::create(wf, d, ns, db);
}
