#pragma once
#include "../../../frontend/widget.hpp"
#include "shared_model.hpp"
#include <imgui.h>
#include <spdlog/spdlog.h>

namespace ymery::plugins::editor {

class Preview : public Widget {
public:
    static Result<WidgetPtr> create(
        std::shared_ptr<WidgetFactory> widget_factory,
        std::shared_ptr<Dispatcher> dispatcher,
        const std::string& ns,
        std::shared_ptr<DataBag> data_bag
    ) {
        auto widget = std::make_shared<Preview>();
        widget->_widget_factory = widget_factory;
        widget->_dispatcher = dispatcher;
        widget->_namespace = ns;
        widget->_data_bag = data_bag;
        if (auto res = widget->init(); !res) {
            return Err<WidgetPtr>("Preview::create failed", res);
        }
        return widget;
    }

protected:
    Result<void> _pre_render_head() override {
        auto& model = SharedLayoutModel::instance();

        if (model.empty()) {
            _cached_widget = nullptr;
            _cached_version = 0;

            ImVec2 avail = ImGui::GetContentRegionAvail();
            ImVec2 text_size = ImGui::CalcTextSize("No widgets in layout");
            ImGui::SetCursorPos(ImVec2((avail.x - text_size.x) / 2, (avail.y - text_size.y) / 2));
            ImGui::TextDisabled("No widgets in layout");
            return Ok();
        }

        uint64_t current_version = model.version();
        if (!_cached_widget || _cached_version != current_version) {
            spdlog::debug("Preview: model changed (v{} -> v{}), recreating widget", _cached_version, current_version);
            auto res = _widget_factory->create_widget(_data_bag, model.root(), "app");
            if (res) {
                _cached_widget = *res;
                _cached_version = current_version;
            } else {
                std::string err = error_msg(res);
                spdlog::error("Preview: failed to create widget: {}", err);
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s", err.c_str());
                return Ok();
            }
        }

        if (_cached_widget) {
            _cached_widget->render();
        }
        return Ok();
    }

private:
    WidgetPtr _cached_widget;
    uint64_t _cached_version = 0;
};

} // namespace ymery::plugins::editor
