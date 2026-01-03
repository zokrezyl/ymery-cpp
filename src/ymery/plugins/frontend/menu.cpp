// menu widget plugin - wraps content in imgui menu
#include "../../frontend/composite.hpp"
#include "../../types.hpp"
#include <imgui.h>

namespace ymery::plugins {

class Menu : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Menu>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Menu::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        std::string label = "NO-LABEL";
        if (auto res = _data_bag->get("label"); res && res->has_value()) {
            if (auto s = get_as<std::string>(*res)) {
                label = *s;
            }
        }

        bool enabled = true;
        if (auto res = _data_bag->get("enabled"); res && res->has_value()) {
            if (auto b = get_as<bool>(*res)) {
                enabled = *b;
            }
        }

        std::string imgui_id = label + "###" + _uid;
        _is_body_activated = ImGui::BeginMenu(imgui_id.c_str(), enabled);
        return Ok();
    }

    Result<void> _begin_container() override {
        // Only render children if menu is open (BeginMenu returned true)
        _container_open = _is_body_activated;
        return Ok();
    }

    Result<void> _post_render_head() override {
        if (_is_body_activated) {
            ImGui::EndMenu();
        }
        return Ok();
    }
};

} // namespace ymery::plugins

extern "C" const char* name() { return "menu"; }
extern "C" const char* type() { return "widget"; }
extern "C" void* create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return static_cast<void*>(new ymery::Result<ymery::WidgetPtr>(ymery::plugins::Menu::create(wf, d, ns, db)));
}
