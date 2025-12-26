// main-menu-bar widget plugin - creates menu bar at top of screen
#include "../../frontend/composite.hpp"
#include "../../types.hpp"
#include <imgui.h>

namespace ymery::plugins {

class MainMenuBar : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<MainMenuBar>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("MainMenuBar::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        _is_body_activated = ImGui::BeginMainMenuBar();
        return Ok();
    }

    Result<void> _post_render_head() override {
        if (_is_body_activated) {
            ImGui::EndMainMenuBar();
        }
        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "main-menu-bar"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::MainMenuBar::create(wf, d, ns, db);
}
