// implot widget plugin - Creates plot context, renders body inside
#include "../../frontend/composite.hpp"
#include "../../frontend/widget_factory.hpp"
#include <implot.h>
#include <imgui.h>

namespace ymery::plugins {

class Implot : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Implot>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Implot::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _begin_container() override {
        std::string label = "Plot";
        if (auto res = _data_bag->get("label"); res) {
            if (auto l = get_as<std::string>(*res)) {
                label = *l;
            }
        }

        // Get size from params
        ImVec2 size(-1, -1);
        if (auto res = _data_bag->get("size"); res) {
            if (auto s = get_as<List>(*res)) {
                if (s->size() >= 2) {
                    if (auto w = get_as<double>((*s)[0])) size.x = static_cast<float>(*w);
                    if (auto h = get_as<double>((*s)[1])) size.y = static_cast<float>(*h);
                }
            }
        }

        _container_open = ImPlot::BeginPlot(label.c_str(), size);
        return Ok();
    }

    Result<void> _end_container() override {
        if (_container_open) {
            ImPlot::EndPlot();
        }
        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "implot"; }
extern "C" const char* type() { return "widget"; }
extern "C" void* create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return static_cast<void*>(new ymery::Result<ymery::WidgetPtr>(ymery::plugins::Implot::create(wf, d, ns, db)));
}
