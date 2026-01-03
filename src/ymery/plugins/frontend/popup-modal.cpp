// popup-modal widget plugin - modal popup that blocks other windows
// Opens immediately when created, body rendered while open
#include "../../frontend/widget.hpp"
#include "../../frontend/widget_factory.hpp"
#include <imgui.h>

namespace ymery::plugins {

class PopupModal : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<PopupModal>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;

        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("PopupModal::create failed", res);
        }
        return widget;
    }

    Result<void> init() override {
        if (auto res = Widget::init(); !res) {
            return Err<void>("PopupModal: init: could not init base class", res);
        }

        // Get label for popup
        auto label_res = _data_bag->get("label");
        if (!label_res) {
            return Err<void>("PopupModal: init: failed to get label", label_res);
        }

        if (auto label = get_as<std::string>(*label_res)) {
            _label = *label;
        } else {
            _label = "Modal";
        }

        // Open popup immediately
        ImGui::OpenPopup(_label.c_str());
        return Ok();
    }

protected:
    Result<void> _pre_render_head() override {
        // Begin modal popup
        _is_body_activated = ImGui::BeginPopupModal(
            _label.c_str(),
            nullptr,
            ImGuiWindowFlags_AlwaysAutoResize
        );
        return Ok();
    }

    Result<void> _post_render_head() override {
        if (_is_body_activated) {
            ImGui::EndPopup();
        }
        return Ok();
    }

    // Close the popup programmatically
    Result<void> _close() {
        ImGui::CloseCurrentPopup();
        return Ok();
    }

private:
    std::string _label;
};

} // namespace ymery::plugins

extern "C" const char* name() { return "popup-modal"; }
extern "C" const char* type() { return "widget"; }
extern "C" void* create(
    std::shared_ptr<ymery::WidgetFactory> wf,
    std::shared_ptr<ymery::Dispatcher> d,
    const std::string& ns,
    std::shared_ptr<ymery::DataBag> db
) {
    return static_cast<void*>(new ymery::Result<ymery::WidgetPtr>(ymery::plugins::PopupModal::create(wf, d, ns, db)));
}
