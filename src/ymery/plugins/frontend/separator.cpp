// separator widget plugin
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include "../../plugin_export.hpp"
#include <imgui.h>

namespace ymery::plugins {

class Separator : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Separator>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Separator::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        ImGui::Separator();
        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* PLUGIN_EXPORT_NAME() { return "separator"; }
extern "C" const char* PLUGIN_EXPORT_TYPE() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> PLUGIN_EXPORT_CREATE(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::Separator::create(wf, d, ns, db);
}
