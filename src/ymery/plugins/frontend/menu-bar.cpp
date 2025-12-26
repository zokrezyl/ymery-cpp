// menu-bar widget plugin - container for menus (requires MenuBar flag on parent)
#include "../../frontend/composite.hpp"
#include "../../types.hpp"
#include <imgui.h>

namespace ymery::plugins {

class MenuBar : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<MenuBar>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("MenuBar::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        _is_body_activated = ImGui::BeginMenuBar();
        return Ok();
    }

    Result<void> _post_render_head() override {
        if (_is_body_activated) {
            ImGui::EndMenuBar();
        }
        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "menu-bar"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::MenuBar::create(wf, d, ns, db);
}
