// child widget plugin (scrollable region)
#include "../../frontend/composite.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins {

class Child : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Child>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Child::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _begin_container() override {
        std::string id = "##Child" + _uid;

        ImVec2 size(0, 0);
        if (auto width_res = _data_bag->get_static("width"); width_res) {
            if (auto w = get_as<double>(*width_res)) {
                size.x = static_cast<float>(*w);
            }
        }
        if (auto height_res = _data_bag->get_static("height"); height_res) {
            if (auto h = get_as<double>(*height_res)) {
                size.y = static_cast<float>(*h);
            }
        }

        bool border = false;
        if (auto border_res = _data_bag->get_static("border"); border_res) {
            if (auto b = get_as<bool>(*border_res)) {
                border = *b;
            }
        }

        _container_open = ImGui::BeginChild(id.c_str(), size, border ? ImGuiChildFlags_Borders : 0);
        return Ok();
    }

    Result<void> _end_container() override {
        ImGui::EndChild();
        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "child"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::Child::create(wf, d, ns, db);
}
