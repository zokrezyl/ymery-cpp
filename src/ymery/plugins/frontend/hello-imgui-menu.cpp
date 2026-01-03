// hello-imgui-menu widget plugin - menu bar contents for hello-imgui-main-window
#include "../../frontend/composite.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins {

class HelloImguiMenu : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<HelloImguiMenu>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("HelloImguiMenu::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _begin_container() override {
        // No container - children are rendered directly in menu bar context
        _container_open = true;
        return Ok();
    }

    Result<void> _end_container() override {
        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "hello-imgui-menu"; }
extern "C" const char* type() { return "widget"; }
extern "C" void* create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return static_cast<void*>(new ymery::Result<ymery::WidgetPtr>(ymery::plugins::HelloImguiMenu::create(wf, d, ns, db)));
}
