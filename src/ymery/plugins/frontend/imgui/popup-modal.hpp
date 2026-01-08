#pragma once
#include "../../../frontend/widget.hpp"
#include <imgui.h>

namespace ymery::plugins::imgui {

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
        auto label_res = _data_bag->get("label");
        if (!label_res) {
            return Err<void>("PopupModal: init: failed to get label", label_res);
        }
        if (auto label = get_as<std::string>(*label_res)) {
            _label = *label;
        } else {
            _label = "Modal";
        }
        ImGui::OpenPopup(_label.c_str());
        return Ok();
    }

protected:
    Result<void> _pre_render_head() override {
        _is_body_activated = ImGui::BeginPopupModal(
            _label.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        return Ok();
    }

    Result<void> _post_render_head() override {
        if (_is_body_activated) {
            ImGui::EndPopup();
        }
        return Ok();
    }

private:
    std::string _label;
};

} // namespace ymery::plugins::imgui
