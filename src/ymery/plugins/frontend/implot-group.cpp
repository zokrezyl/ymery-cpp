// implot-group widget plugin - Creates subplots context
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include <implot.h>
#include <imgui.h>

namespace ymery::plugins {

class ImplotGroup : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<ImplotGroup>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("ImplotGroup::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        std::string label = "Subplots";
        if (auto res = _data_bag->get("label"); res) {
            if (auto l = get_as<std::string>(*res)) {
                label = *l;
            }
        }

        int rows = 1, cols = 1;
        if (auto res = _data_bag->get("rows"); res) {
            if (auto r = get_as<int>(*res)) rows = *r;
            else if (auto r = get_as<double>(*res)) rows = static_cast<int>(*r);
        }
        if (auto res = _data_bag->get("cols"); res) {
            if (auto c = get_as<int>(*res)) cols = *c;
            else if (auto c = get_as<double>(*res)) cols = static_cast<int>(*c);
        }

        ImVec2 size(-1, -1);
        if (auto res = _data_bag->get("size"); res) {
            if (auto s = get_as<List>(*res)) {
                if (s->size() >= 2) {
                    if (auto w = get_as<double>((*s)[0])) size.x = static_cast<float>(*w);
                    if (auto h = get_as<double>((*s)[1])) size.y = static_cast<float>(*h);
                }
            }
        }

        _is_body_activated = ImPlot::BeginSubplots(label.c_str(), rows, cols, size);
        return Ok();
    }

    Result<void> _post_render_head() override {
        if (_is_body_activated) {
            ImPlot::EndSubplots();
        }
        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "implot-group"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::ImplotGroup::create(wf, d, ns, db);
}
