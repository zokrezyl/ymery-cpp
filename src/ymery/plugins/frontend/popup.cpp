// popup widget plugin - regular popup (non-modal)
// Opens immediately when created, body rendered while open
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins {

class Popup : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Popup>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Popup::create failed", res);
        }
        return widget;
    }

    Result<void> init() override {
        // Open popup immediately - first stage of imgui popup creation
        ImGui::OpenPopup(_uid.c_str());
        return Widget::init();
    }

protected:
    Result<void> _pre_render_head() override {
        // Check if popup is open
        bool popup_opened = ImGui::BeginPopup(_uid.c_str());
        _is_body_activated = popup_opened;

        if (_render_cycle == 0) {
            // Workaround: in first render cycle it returns always false
            _is_open = true;
        } else {
            _is_open = popup_opened;
            _is_body_activated = popup_opened;
        }

        return Ok();
    }

    Result<void> _post_render_head() override {
        if (_is_body_activated) {
            ImGui::EndPopup();
        }
        return Ok();
    }

private:
    bool _is_open = false;
    int _render_cycle = 0;
};

} // namespace ymery::plugins

extern "C" const char* name() { return "popup"; }
extern "C" const char* type() { return "widget"; }
extern "C" ymery::Result<ymery::WidgetPtr> create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return ymery::plugins::Popup::create(wf, d, ns, db);
}
