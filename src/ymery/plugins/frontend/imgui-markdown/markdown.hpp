#pragma once
#include "../../../frontend/widget.hpp"
#include <imgui.h>
#include <imgui_markdown.h>

namespace ymery::plugins::imgui_markdown {

inline void MarkdownLinkCallback(ImGui::MarkdownLinkCallbackData data) {
    // Could open URL here
}

inline ImGui::MarkdownImageData MarkdownImageCallback(ImGui::MarkdownLinkCallbackData data) {
    return {};
}

class Markdown : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Markdown>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;
        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Markdown::create failed", res);
        }
        return widget;
    }

    Result<void> init() override {
        if (auto res = Widget::init(); !res) return res;
        _md_config.linkCallback = MarkdownLinkCallback;
        _md_config.tooltipCallback = nullptr;
        _md_config.imageCallback = MarkdownImageCallback;
        _md_config.userData = nullptr;
        return Ok();
    }

protected:
    Result<void> _pre_render_head() override {
        std::string text;
        if (auto res = _data_bag->get("label"); res) {
            if (auto t = get_as<std::string>(*res)) text = *t;
        }
        ImGui::Markdown(text.c_str(), text.size(), _md_config);
        return Ok();
    }

private:
    ImGui::MarkdownConfig _md_config{};
};

} // namespace ymery::plugins::imgui_markdown
