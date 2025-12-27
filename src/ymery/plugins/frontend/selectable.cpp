// selectable widget plugin
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins {

class Selectable : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Selectable>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Selectable::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        std::string label = "Selectable";
        if (auto res = _data_bag->get("label"); res && res->has_value()) {
            if (auto l = get_as<std::string>(*res)) label = *l;
        }

        bool selected = false;
        if (auto res = _data_bag->get("selected"); res && res->has_value()) {
            if (auto s = get_as<bool>(*res)) selected = *s;
        }

        ImGuiSelectableFlags flags = ImGuiSelectableFlags_None;
        if (auto res = _data_bag->get_static("span-all-columns"); res && res->has_value()) {
            if (auto s = get_as<bool>(*res); s && *s) {
                flags |= ImGuiSelectableFlags_SpanAllColumns;
            }
        }

        std::string imgui_id = label + "##" + _uid;
        if (ImGui::Selectable(imgui_id.c_str(), &selected, flags)) {
            _data_bag->set("selected", selected);
        }

        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "selectable"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::Selectable::create(wf, d, ns, db);
}
