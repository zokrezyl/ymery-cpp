// hello-imgui-app-menu-items widget plugin - app menu items for hello-imgui-main-window
#include "../../frontend/composite.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins {

class HelloImguiAppMenuItems : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<HelloImguiAppMenuItems>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("HelloImguiAppMenuItems::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _begin_container() override {
        // No container - children are rendered directly
        _container_open = true;
        return Ok();
    }

    Result<void> _end_container() override {
        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "hello-imgui-app-menu-items"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::HelloImguiAppMenuItems::create(wf, d, ns, db);
}
