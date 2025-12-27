// indent widget plugin - calls ImGui::Indent/Unindent around children
#include "../../frontend/composite.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins {

class Indent : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Indent>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Indent::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _begin_container() override {
        float width = 0.0f;  // 0 = use default indent
        if (auto res = _data_bag->get_static("width"); res && res->has_value()) {
            if (auto w = get_as<float>(*res)) width = *w;
            else if (auto w = get_as<double>(*res)) width = static_cast<float>(*w);
            else if (auto w = get_as<int>(*res)) width = static_cast<float>(*w);
        }

        ImGui::Indent(width);
        _container_open = true;
        return Ok();
    }

    Result<void> _end_container() override {
        float width = 0.0f;
        if (auto res = _data_bag->get_static("width"); res && res->has_value()) {
            if (auto w = get_as<float>(*res)) width = *w;
            else if (auto w = get_as<double>(*res)) width = static_cast<float>(*w);
            else if (auto w = get_as<int>(*res)) width = static_cast<float>(*w);
        }

        ImGui::Unindent(width);
        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "indent"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::Indent::create(wf, d, ns, db);
}
