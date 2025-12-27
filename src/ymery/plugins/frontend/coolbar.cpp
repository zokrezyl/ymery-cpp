// coolbar widget plugin - macOS-style dock bar
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include "../../frontend/composite.hpp"
#include <imgui.h>
#include <ImCoolBar.h>

namespace ymery::plugins {

class CoolBar : public Composite {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<CoolBar>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("CoolBar::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        ImCoolBarFlags flags = ImCoolBarFlags_None;

        std::string anchor = "bottom";
        if (auto res = _data_bag->get_static("anchor"); res) {
            if (auto a = get_as<std::string>(*res)) {
                anchor = *a;
            }
        }

        if (anchor == "bottom") flags |= ImCoolBarFlags_Horizontal;
        else if (anchor == "top") flags |= ImCoolBarFlags_Horizontal;
        else if (anchor == "left") flags |= ImCoolBarFlags_Vertical;
        else if (anchor == "right") flags |= ImCoolBarFlags_Vertical;

        ImGui::ImCoolBarConfig config;
        if (auto res = _data_bag->get_static("normal_size"); res) {
            if (auto s = get_as<double>(*res)) {
                config.normal_size = static_cast<float>(*s);
            }
        }
        if (auto res = _data_bag->get_static("hovered_size"); res) {
            if (auto s = get_as<double>(*res)) {
                config.hovered_size = static_cast<float>(*s);
            }
        }

        std::string imgui_id = "coolbar###" + _uid;
        _container_open = ImGui::BeginCoolBar(imgui_id.c_str(), flags, config);
        _is_body_activated = _container_open;
        return Ok();
    }

    Result<void> _post_render_head() override {
        if (_container_open) {
            ImGui::EndCoolBar();
        }
        return Ok();
    }

private:
    bool _container_open = false;
};

} // namespace ymery::plugins

extern "C" const char* name() { return "coolbar"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::CoolBar::create(wf, d, ns, db);
}
