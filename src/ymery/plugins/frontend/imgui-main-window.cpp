// imgui-main-window widget plugin - main window for classic ImGui apps
// In C++, the App class manages the window, so this just renders body children
#include "../../frontend/composite.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>
#include <spdlog/spdlog.h>

namespace ymery::plugins {

class ImguiMainWindow : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<ImguiMainWindow>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("ImguiMainWindow::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _begin_container() override {
        // ImguiMainWindow doesn't create a container - it renders directly
        // The App class already created the window
        _container_open = true;
        return Ok();
    }

    Result<void> _end_container() override {
        // Nothing to close
        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "imgui-main-window"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::ImguiMainWindow::create(wf, d, ns, db);
}
