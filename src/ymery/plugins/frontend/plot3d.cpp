// plot3d widget plugin - 3D plotting container
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include "../../frontend/composite.hpp"
#include <imgui.h>
#include <implot3d.h>

namespace ymery::plugins {

class Plot3D : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Plot3D>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Plot3D::create failed", res);
        }
        return widget;
    }

    Result<void> init() override {
        if (auto res = Widget::init(); !res) {
            return res;
        }
        // Create ImPlot3D context if not exists
        if (!ImPlot3D::GetCurrentContext()) {
            ImPlot3D::CreateContext();
        }
        return Ok();
    }

protected:
    Result<void> _pre_render_head() override {
        std::string label = "Plot3D";
        if (auto res = _data_bag->get("label"); res) {
            if (auto l = get_as<std::string>(*res)) {
                label = *l;
            }
        }

        ImVec2 size(-1, -1);
        if (auto res = _data_bag->get_static("size"); res) {
            if (auto s = get_as<List>(*res)) {
                if (s->size() >= 2) {
                    if (auto w = get_as<double>((*s)[0])) size.x = static_cast<float>(*w);
                    if (auto h = get_as<double>((*s)[1])) size.y = static_cast<float>(*h);
                }
            }
        }

        _container_open = ImPlot3D::BeginPlot(label.c_str(), size);
        _is_body_activated = _container_open;
        return Ok();
    }

    Result<void> _post_render_head() override {
        if (_container_open) {
            ImPlot3D::EndPlot();
        }
        return Ok();
    }

private:
    bool _container_open = false;
};

} // namespace ymery::plugins

extern "C" const char* name() { return "plot3d"; }
extern "C" const char* type() { return "widget"; }
extern "C" void* create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return static_cast<void*>(new ymery::Result<ymery::WidgetPtr>(ymery::plugins::Plot3D::create(wf, d, ns, db)));
}
